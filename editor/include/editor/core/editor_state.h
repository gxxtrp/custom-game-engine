#pragma once

#include "engine/scene/scene.h"
#include "editor/core/selection_context.h"
#include <string>
#include <memory>
#include <functional>

namespace editor {

enum class EditorMode {
    Edit,
    Play,
    Simulate,
    Paused
};

class EditorStateManager {
public:
    using ModeChangedCallback = std::function<void(EditorMode previous_mode, EditorMode new_mode)>;

    EditorStateManager();
    ~EditorStateManager();

    void start_play(engine::scene::Scene& active_scene, SelectionContext& selection);
    void start_simulate(engine::scene::Scene& active_scene, SelectionContext& selection);
    void pause();
    void resume();
    void stop(engine::scene::Scene& active_scene, SelectionContext& selection);
    void step_frame(engine::scene::Scene& active_scene, float dt = 1.0f / 60.0f);

    void update(engine::scene::Scene& active_scene, float dt);

    EditorMode get_mode() const { return m_mode; }
    bool is_playing() const { return m_mode == EditorMode::Play; }
    bool is_simulating() const { return m_mode == EditorMode::Simulate; }
    bool is_paused() const { return m_mode == EditorMode::Paused; }
    bool is_editing() const { return m_mode == EditorMode::Edit; }
    bool has_snapshot() const { return !m_scene_snapshot_toml.empty(); }

    void add_mode_listener(ModeChangedCallback callback) {
        m_mode_listeners.push_back(std::move(callback));
    }

private:
    void set_mode(EditorMode mode);
    void snapshot_scene(engine::scene::Scene& active_scene);
    void restore_scene(engine::scene::Scene& active_scene, SelectionContext& selection);
    void cleanup_scene_physics(engine::scene::Scene& active_scene);

    EditorMode m_mode{EditorMode::Edit};
    EditorMode m_pre_pause_mode{EditorMode::Play};
    std::string m_scene_snapshot_toml;
    std::vector<ModeChangedCallback> m_mode_listeners;
};

} // namespace editor
