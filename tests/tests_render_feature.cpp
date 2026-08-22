// CPU-side tests for the pluggable render feature seam. No GPU device is
// required: SceneRenderer is constructed with a null RhiContext, features are
// mock objects that record their callbacks, and the RenderGraph executes with a
// default-constructed command buffer (its CPU-only path skips all Vulkan calls).
//
// Coverage per spec:
//   Test 1 - Dynamic Feature Injection
//   Test 2 - Stage Execution Ordering
//   Test 3 - Feature Unregistration / Disabling

#include "engine/renderer/scene_renderer.h"
#include "engine/renderer/render_feature.h"
#include "engine/renderer/render_stage.h"
#include "engine/scene/scene.h"
#include "engine/core/log.h"
#include <cstdio>
#include <string>
#include <vector>
#include <memory>

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond, msg)                                                   \
    do {                                                                   \
        ++g_checks;                                                        \
        if (!(cond)) {                                                     \
            ++g_failures;                                                  \
            std::printf("FAIL [%s:%d]: %s\n", __FILE__, __LINE__, (msg));  \
        }                                                                  \
    } while (0)

// Global callback trace shared by all mock features.
std::vector<std::string> g_setup_calls;
std::vector<std::string> g_execute_calls;
std::vector<std::string> g_post_calls;

void reset_trace() {
    g_setup_calls.clear();
    g_execute_calls.clear();
    g_post_calls.clear();
}

class MockPassFeature final : public engine::renderer::IRenderFeature {
public:
    MockPassFeature(std::string name, engine::renderer::RenderStage stage, int32_t priority = 0)
        : m_name(std::move(name)), m_stage(stage), m_priority(priority) {}

    std::string_view get_name() const noexcept override { return m_name; }
    engine::renderer::RenderStage get_stage() const noexcept override { return m_stage; }
    int32_t get_priority() const noexcept override { return m_priority; }

    void setup(engine::renderer::RenderPassBuilder& builder, const engine::renderer::SceneRenderView& view) override {
        (void)builder; (void)view;
        g_setup_calls.push_back(m_name);
    }

    void execute(engine::renderer::RenderPassContext& ctx, const engine::renderer::SceneRenderView& view) override {
        (void)ctx; (void)view;
        g_execute_calls.push_back(m_name);
    }

    void post_frame(const engine::renderer::SceneRenderView& view) override {
        (void)view;
        g_post_calls.push_back(m_name);
    }

private:
    std::string m_name;
    engine::renderer::RenderStage m_stage;
    int32_t m_priority;
};

engine::renderer::SceneRenderer make_cpu_renderer() {
    return engine::renderer::SceneRenderer(nullptr);
}

engine::scene::Scene make_empty_scene() {
    return engine::scene::Scene("FeatureTestScene");
}

} // namespace

// ---- Test 1: Dynamic Feature Injection ------------------------------------
static void test_dynamic_feature_injection() {
    reset_trace();
    auto renderer = make_cpu_renderer();

    auto mock = std::make_shared<MockPassFeature>("MockPassFeature", engine::renderer::RenderStage::OverlayDebug);
    renderer.register_feature(mock);
    CHECK(renderer.get_feature("MockPassFeature") == mock.get(), "get_feature must return the registered feature");

    engine::scene::Scene scene = make_empty_scene();
    engine::renderer::Camera camera{};
    engine::rhi::RHIImageHandle target{}; // invalid -> CPU-only path

    renderer.render(scene, camera, target);

    CHECK(g_setup_calls.size() == 1 && g_setup_calls[0] == "MockPassFeature",
          "feature setup must execute within the graph");
    CHECK(g_execute_calls.size() == 1 && g_execute_calls[0] == "MockPassFeature",
          "feature execute must execute within the graph");
    CHECK(g_post_calls.size() == 1 && g_post_calls[0] == "MockPassFeature",
          "feature post_frame must be invoked by the host");
}

// ---- Test 2: Stage Execution Ordering --------------------------------------
static void test_stage_execution_ordering() {
    reset_trace();
    auto renderer = make_cpu_renderer();

    // One feature per representative stage + two in the same stage (registration order).
    renderer.register_feature(std::make_shared<MockPassFeature>("OverlayA", engine::renderer::RenderStage::OverlayDebug));
    renderer.register_feature(std::make_shared<MockPassFeature>("DepthA", engine::renderer::RenderStage::DepthPrePass));
    renderer.register_feature(std::make_shared<MockPassFeature>("LightingB", engine::renderer::RenderStage::Lighting));
    renderer.register_feature(std::make_shared<MockPassFeature>("LightingA", engine::renderer::RenderStage::Lighting));
    renderer.register_feature(std::make_shared<MockPassFeature>("OpaqueA", engine::renderer::RenderStage::Opaque));

    engine::scene::Scene scene = make_empty_scene();
    engine::renderer::Camera camera{};
    engine::rhi::RHIImageHandle target{};

    renderer.render(scene, camera, target);

    const std::vector<std::string> expected = {
        "DepthA",        // DepthPrePass (stage 0)
        "OpaqueA",       // Opaque (stage 1)
        "LightingB",     // Lighting (stage 2, registration order: B was registered first)
        "LightingA",
        "OverlayA",      // OverlayDebug (stage 5)
    };

    CHECK(g_execute_calls.size() == expected.size(), "all five features must execute");
    bool order_ok = g_execute_calls.size() == expected.size();
    for (size_t i = 0; order_ok && i < expected.size(); ++i) {
        if (g_execute_calls[i] != expected[i]) order_ok = false;
    }
    if (!order_ok) {
        std::string actual;
        for (const auto& s : g_execute_calls) { actual += s; actual += " "; }
        std::printf("  actual execute order: [%s]\n", actual.c_str());
    }
    CHECK(order_ok, "execute callbacks must occur in strict stage order, registration order within a stage");

    // Priority ordering within a stage: lower priority executes first.
    reset_trace();
    auto renderer2 = make_cpu_renderer();
    renderer2.register_feature(std::make_shared<MockPassFeature>("Late", engine::renderer::RenderStage::PostProcessStack, 5));
    renderer2.register_feature(std::make_shared<MockPassFeature>("Early", engine::renderer::RenderStage::PostProcessStack, -1));
    renderer2.render(scene, camera, target);

    CHECK(g_execute_calls.size() == 2 && g_execute_calls[0] == "Early" && g_execute_calls[1] == "Late",
          "features with lower priority must execute before higher-priority features");
}

// ---- Test 3: Feature Unregistration / Disabling ----------------------------
static void test_feature_unregistration() {
    reset_trace();
    auto renderer = make_cpu_renderer();

    auto keep = std::make_shared<MockPassFeature>("KeepFeature", engine::renderer::RenderStage::OverlayDebug);
    auto remove = std::make_shared<MockPassFeature>("RemoveFeature", engine::renderer::RenderStage::OverlayDebug);
    renderer.register_feature(keep);
    renderer.register_feature(remove);

    engine::scene::Scene scene = make_empty_scene();
    engine::renderer::Camera camera{};
    engine::rhi::RHIImageHandle target{};

    renderer.render(scene, camera, target);
    CHECK(g_execute_calls.size() == 2, "both features execute before unregistration");

    renderer.unregister_feature("RemoveFeature");
    CHECK(renderer.get_feature("RemoveFeature") == nullptr, "unregistered feature must not be resolvable");
    CHECK(renderer.get_feature("KeepFeature") == keep.get(), "other features must remain registered");

    reset_trace();
    renderer.render(scene, camera, target);
    CHECK(g_execute_calls.size() == 1 && g_execute_calls[0] == "KeepFeature",
          "removed feature must not execute in later frames (no dangling references)");

    // Re-register under the same name (replace semantics).
    auto replacement = std::make_shared<MockPassFeature>("KeepFeature", engine::renderer::RenderStage::OverlayDebug);
    renderer.register_feature(replacement);
    CHECK(renderer.get_feature("KeepFeature") == replacement.get(), "re-registration must replace the previous feature");

    // Unregistering a nonexistent name is a no-op.
    renderer.unregister_feature("DoesNotExist");
}

// ---- RenderStage enum sanity ----------------------------------------------
static void test_render_stage_enum() {
    CHECK(static_cast<uint8_t>(engine::renderer::RenderStage::DepthPrePass) == 0, "DepthPrePass must be stage 0");
    CHECK(static_cast<uint8_t>(engine::renderer::RenderStage::Opaque) == 1, "Opaque must be stage 1");
    CHECK(static_cast<uint8_t>(engine::renderer::RenderStage::Lighting) == 2, "Lighting must be stage 2");
    CHECK(static_cast<uint8_t>(engine::renderer::RenderStage::Translucent) == 3, "Translucent must be stage 3");
    CHECK(static_cast<uint8_t>(engine::renderer::RenderStage::PostProcessStack) == 4, "PostProcessStack must be stage 4");
    CHECK(static_cast<uint8_t>(engine::renderer::RenderStage::OverlayDebug) == 5, "OverlayDebug must be stage 5");
    CHECK(engine::renderer::get_render_stage_name(engine::renderer::RenderStage::Lighting) == "Lighting",
          "stage name lookup must match");
}

int main() {
    std::printf("=== Renderer Feature Seam Tests (CPU-only) ===\n");

    test_render_stage_enum();
    test_dynamic_feature_injection();
    test_stage_execution_ordering();
    test_feature_unregistration();

    std::printf("---------------------------------------------\n");
    std::printf("Checks: %d, Failures: %d\n", g_checks, g_failures);
    if (g_failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("TESTS FAILED\n");
    return 1;
}
