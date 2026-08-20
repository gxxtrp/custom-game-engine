# Dedicated Editor Roadmap & Architecture Specification

This document provides the definitive architectural blueprint, code design patterns, and step-by-step implementation guide for the standalone **Game Engine Editor** (`editor/` producing `editor.exe`).

For the project layout, manifest schema, and standalone game structure, see the companion document: [GAME_PROJECT_SPECIFICATION.md](file:///c:/Users/x/work/my-engine/docs/GAME_PROJECT_SPECIFICATION.md).

---

## 1. Vision & Architecture Overview

The dedicated **Editor Application** (`editor/`) is a standalone C++ application that links the core runtime library (`engine_master.lib`). It provides a professional, dockable authoring environment for level design, gameplay scripting, asset management, visual debugging, and distribution packaging.

```
my-engine/
├── engine/                           <-- Pure Runtime SDK & Libraries (`engine_master.lib`)
├── tools/                            <-- Asset Cooker & CLI utilities (`pak_builder.exe`)
│
├── editor/                           <-- DEDICATED EDITOR APPLICATION (`editor.exe`)
│   ├── include/editor/
│   │   ├── editor_app.h              <-- Main Application Driver & DockSpace Layout
│   │   ├── editor_state.h            <-- Play/Pause/Simulate/Step State Machine
│   │   ├── selection_context.h       <-- Multi-Entity Selection & Focus Manager
│   │   ├── command_history.h         <-- Undo / Redo Command Pattern System
│   │   ├── project_hub.h             <-- Project Launcher & New Project Wizard
│   │   ├── project_settings_dlg.h    <-- Visual Project & Engine Settings Editor
│   │   ├── outliner_panel.h          <-- Scene Hierarchy Tree, Search, Parenting & Locks
│   │   ├── inspector_panel.h         <-- Dynamic Component Inspector & "Add Component" Menu
│   │   ├── scene_viewport.h          <-- 3D Viewport with Gizmos, Raycast Picking & Snapping
│   │   ├── content_browser.h         <-- Visual Asset Tree, Thumbnails, & UUID-Safe Ops
│   │   ├── prefab_manager.h          <-- Prefab Creation, Instancing & Overrides
│   │   ├── asset_importer.h          <-- Drag-and-Drop glTF / Audio / Texture Pipeline
│   │   ├── script_watcher.h          <-- FileSystem Watcher for Lua Live Hot-Reloading
│   │   ├── material_editor.h         <-- PBR Material Inspector & Preset Authoring
│   │   ├── environment_panel.h       <-- Skybox (HDRI/IBL), Fog & Post-Processing Editor
│   │   ├── console_panel.h           <-- Live Engine Log Feed with Filters & Stack Traces
│   │   ├── profiler_panel.h          <-- Real-time CPU/GPU Frame Graph, Draw Calls & VRAM
│   │   ├── debug_draw_pass.h         <-- Jolt Physics Colliders & Camera Frustum Wireframes
│   │   ├── autosave_manager.h        <-- Background Autosave & Crash Recovery System
│   │   └── game_exporter.h           <-- Standalone Game Cooker & Binary Packager
│   ├── src/
│   │   ├── main.cpp                  <-- Editor Entrypoint
│   │   ├── editor_app.cpp
│   │   ├── editor_state.cpp
│   │   ├── selection_context.cpp
│   │   ├── command_history.cpp
│   │   ├── project_hub.cpp
│   │   ├── project_settings_dlg.cpp
│   │   ├── outliner_panel.cpp
│   │   ├── inspector_panel.cpp
│   │   ├── scene_viewport.cpp
│   │   ├── content_browser.cpp
│   │   ├── prefab_manager.cpp
│   │   ├── asset_importer.cpp
│   │   ├── script_watcher.cpp
│   │   ├── material_editor.cpp
│   │   ├── environment_panel.cpp
│   │   ├── console_panel.cpp
│   │   ├── profiler_panel.cpp
│   │   ├── debug_draw_pass.cpp
│   │   ├── autosave_manager.cpp
│   │   └── game_exporter.cpp
│   └── CMakeLists.txt                <-- Links `engine_master`, `imgui`, `ImGuizmo`
```

---

## 2. Core Subsystems & Implementation Patterns

### 2.1. Editor State Machine & Scene Snapshotting (`editor_state.h`)

The editor operates in four execution modes:
* **Edit Mode**: Standard authoring. Physics is paused, scripts do not tick, transforms manipulated via gizmos.
* **Play Mode (PIE - Play in Editor)**: Clones active Flecs world, captures gameplay camera, enables physics, starts audio & Lua ticks. On Stop, restores pristine pre-play snapshot.
* **Simulate Mode**: Runs physics, audio, and Lua simulations while retaining free `EditorCamera` flycam control.
* **Step Frame**: Advances simulation by exactly one physics tick ($1/60\text{s}$) for deterministic debugging.

#### Implementation Pattern:
```cpp
enum class EditorMode { Edit, Play, Simulate, Paused };

class EditorStateManager {
public:
    void enter_play_mode(scene::Scene& active_scene) {
        // 1. Snapshot active scene state
        m_scene_snapshot = active_scene.clone();
        m_mode = EditorMode::Play;
        
        // 2. Start physics & scripts
        active_scene.get_physics_world().start_simulation();
        active_scene.get_script_engine().invoke_all_on_init();
    }

    void stop_play_mode(scene::Scene& active_scene) {
        if (m_mode == EditorMode::Edit) return;

        // 1. Stop audio & physics
        active_scene.get_audio_engine().stop_all();
        active_scene.get_physics_world().stop_simulation();

        // 2. Restore pre-play snapshot
        active_scene.restore_from(*m_scene_snapshot);
        m_scene_snapshot.reset();
        m_mode = EditorMode::Edit;
    }

    void step_frame(scene::Scene& active_scene, float dt = 1.0f / 60.0f) {
        if (m_mode == EditorMode::Edit || m_mode == EditorMode::Paused) {
            active_scene.update(dt);
            active_scene.get_physics_world().step_simulation(dt);
        }
    }

    EditorMode get_mode() const { return m_mode; }

private:
    EditorMode m_mode{EditorMode::Edit};
    std::unique_ptr<scene::Scene> m_scene_snapshot;
};
```

---

### 2.2. Multi-Entity Selection Context (`selection_context.h`)

Tracks primary and multi-selected entities with fast lookup and notification events:

```cpp
class SelectionContext {
public:
    void select(flecs::entity entity, bool multi_select = false) {
        if (!multi_select) {
            m_selected_entities.clear();
        }
        if (entity.is_valid()) {
            m_selected_entities.insert(entity.id());
            m_primary_entity = entity;
        } else {
            m_primary_entity = flecs::entity::null();
        }
    }

    void deselect(flecs::entity entity) {
        m_selected_entities.erase(entity.id());
        if (m_primary_entity.id() == entity.id()) {
            m_primary_entity = m_selected_entities.empty() 
                ? flecs::entity::null() 
                : flecs::entity(entity.world(), *m_selected_entities.begin());
        }
    }

    void clear() {
        m_selected_entities.clear();
        m_primary_entity = flecs::entity::null();
    }

    bool is_selected(flecs::entity entity) const {
        return m_selected_entities.contains(entity.id());
    }

    flecs::entity get_primary() const { return m_primary_entity; }
    const std::unordered_set<uint64_t>& get_all_selected() const { return m_selected_entities; }
    size_t count() const { return m_selected_entities.size(); }

private:
    std::unordered_set<uint64_t> m_selected_entities;
    flecs::entity m_primary_entity;
};
```

---

### 2.3. Undo / Redo Command History (`command_history.h`)

All scene modifications (transforms, component edits, additions, deletions, reparenting) must execute through commands:

```cpp
class IEditorCommand {
public:
    virtual ~IEditorCommand() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual std::string get_name() const = 0;
};

// 1. Transform Command (Gizmo manipulation)
class EntityTransformCommand : public IEditorCommand {
public:
    EntityTransformCommand(flecs::entity entity, const scene::TransformComponent& old_t, const scene::TransformComponent& new_t)
        : m_entity(entity), m_old_transform(old_t), m_new_transform(new_t) {}

    void execute() override {
        if (m_entity.is_valid()) *m_entity.get_mut<scene::TransformComponent>() = m_new_transform;
    }
    void undo() override {
        if (m_entity.is_valid()) *m_entity.get_mut<scene::TransformComponent>() = m_old_transform;
    }
    std::string get_name() const override { return "Transform Entity"; }

private:
    flecs::entity m_entity;
    scene::TransformComponent m_old_transform;
    scene::TransformComponent m_new_transform;
};

// 2. Command History Manager
class CommandHistory {
public:
    void execute_command(std::unique_ptr<IEditorCommand> cmd) {
        cmd->execute();
        // Discard any redos beyond current index
        if (m_cursor < m_history.size()) {
            m_history.erase(m_history.begin() + m_cursor, m_history.end());
        }
        m_history.push_back(std::move(cmd));
        m_cursor++;
        if (m_history.size() > m_max_history) {
            m_history.pop_front();
            m_cursor--;
        }
    }

    void undo() {
        if (m_cursor > 0) {
            m_cursor--;
            m_history[m_cursor]->undo();
        }
    }

    void redo() {
        if (m_cursor < m_history.size()) {
            m_history[m_cursor]->execute();
            m_cursor++;
        }
    }

    bool can_undo() const { return m_cursor > 0; }
    bool can_redo() const { return m_cursor < m_history.size(); }

private:
    std::deque<std::unique_ptr<IEditorCommand>> m_history;
    size_t m_cursor{0};
    size_t m_max_history{100};
};
```

---

### 2.4. 3D Viewport, Raycast Picking & Snapping (`scene_viewport.h`)

#### Rendering & Viewport Image
The 3D Scene is rendered to an offscreen Vulkan image and displayed via `ImGui::Image`:
```cpp
void SceneViewport::render_viewport_ui(VkDescriptorSet viewport_texture_id) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    ImVec2 viewport_size = ImGui::GetContentRegionAvail();
    // 1. Resize offscreen render target if window resized
    handle_resize(viewport_size.x, viewport_size.y);

    // 2. Draw offscreen texture
    ImGui::Image((ImTextureID)viewport_texture_id, viewport_size);

    // 3. Render ImGuizmo overlay
    render_gizmos();

    // 4. Handle mouse raycast picking
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver()) {
        perform_raycast_picking();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}
```

#### Mouse Raycast Math:
```cpp
core::Ray SceneViewport::screen_point_to_ray(float mouse_x, float mouse_y, const core::Mat4& view, const core::Mat4& proj, float vw, float vh) {
    // 1. Convert to Normalized Device Coordinates (NDC) [-1, 1]
    float ndc_x = (2.0f * mouse_x) / vw - 1.0f;
    float ndc_y = 1.0f - (2.0f * mouse_y) / vh; // Invert Y for Vulkan

    // 2. Unproject near and far points
    core::Mat4 inv_view_proj = (proj * view).inverted();
    core::Vec4 near_point = inv_view_proj * core::Vec4(ndc_x, ndc_y, 0.0f, 1.0f);
    core::Vec4 far_point  = inv_view_proj * core::Vec4(ndc_x, ndc_y, 1.0f, 1.0f);
    near_point /= near_point.w;
    far_point  /= far_point.w;

    core::Vec3 ray_origin = core::Vec3(near_point.x, near_point.y, near_point.z);
    core::Vec3 ray_dir = (core::Vec3(far_point.x, far_point.y, far_point.z) - ray_origin).normalized();
    return core::Ray{ ray_origin, ray_dir };
}
```

#### Viewport Shading Modes:
* Dropdown in Viewport Toolbar:
  * `Lit` (Full PBR + Shadows)
  * `Unlit / Albedo` (Color only, shadowless)
  * `Wireframe` (Geometry topology)
  * `Normal Buffer` (World/tangent normals)
  * `Roughness / Metallic` (Material parameter debug)

---

### 2.5. Outliner & Scene Hierarchy (`outliner_panel.h`)

#### Recursive Tree Rendering with Drag-and-Drop Parenting:
```cpp
void OutlinerPanel::draw_entity_node(flecs::entity entity, SelectionContext& selection) {
    auto& tag = entity.get<scene::TagComponent>()->name;
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selection.is_selected(entity)) flags |= ImGuiTreeNodeFlags_Selected;

    bool has_children = entity.children_count() > 0;
    if (!has_children) flags |= ImGuiTreeNodeFlags_Leaf;

    bool node_open = ImGui::TreeNodeEx((void*)(uintptr_t)entity.id(), flags, "%s", tag.c_str());

    // 1. Selection logic
    if (ImGui::IsItemClicked()) {
        bool multi = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift;
        selection.select(entity, multi);
    }

    // 2. Drag Source (Parenting)
    if (ImGui::BeginDragDropSource()) {
        uint64_t eid = entity.id();
        ImGui::SetDragDropPayload("OUTLINER_ENTITY", &eid, sizeof(uint64_t));
        ImGui::Text("Move %s", tag.c_str());
        ImGui::EndDragDropSource();
    }

    // 3. Drop Target (Reparent)
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("OUTLINER_ENTITY")) {
            uint64_t dragged_id = *(const uint64_t*)payload->Data;
            flecs::entity dragged_entity(entity.world(), dragged_id);
            dragged_entity.child_of(entity); // Reparent in Flecs
        }
        ImGui::EndDragDropTarget();
    }

    // 4. Recursive Children
    if (node_open) {
        entity.children([&](flecs::entity child) {
            draw_entity_node(child, selection);
        });
        ImGui::TreePop();
    }
}
```

---

### 2.6. Dynamic Component Inspector (`inspector_panel.h`)

* Iterates over all attached components of the primary entity.
* Supports **Multi-Selection Batch Editing**: when multiple entities are selected, editing a property applies the value or delta across all selected entities via a compound command.
* **"Add Component" Popup**: searchable list of all registered engine and Lua components.

```cpp
void InspectorPanel::draw_inspector(SelectionContext& selection, CommandHistory& history) {
    ImGui::Begin("Inspector");
    flecs::entity entity = selection.get_primary();
    if (!entity.is_valid()) {
        ImGui::TextDisabled("No entity selected");
        ImGui::End();
        return;
    }

    // Tag Component
    if (auto* tag = entity.get_mut<scene::TagComponent>()) {
        char buf[128];
        strncpy_s(buf, tag->name.c_str(), sizeof(buf));
        if (ImGui::InputText("Name", buf, sizeof(buf))) {
            tag->name = buf;
        }
    }

    ImGui::Separator();

    // Transform Component
    if (auto* trans = entity.get_mut<scene::TransformComponent>()) {
        draw_transform_editor(*trans, entity, history);
    }

    // Mesh Renderer Component
    if (auto* mr = entity.get_mut<scene::MeshRendererComponent>()) {
        draw_mesh_renderer_editor(*mr, entity, history);
    }

    // Add Component Button
    if (ImGui::Button("Add Component...", ImVec2(-1, 30))) {
        ImGui::OpenPopup("AddComponentPopup");
    }
    draw_add_component_popup(entity);

    ImGui::End();
}
```

---

### 2.7. Prefab & Entity Template System (`prefab_manager.h`)

* **Save as Prefab**: Dragging an entity from Outliner into Content Browser serializes the entity and all children to `assets/prefabs/<Name>.prefab.toml`.
* **Prefab Instances**: Instantiated entities receive a `PrefabComponent`:
  ```cpp
  struct PrefabComponent {
      assets::UUID prefab_uuid;
      bool is_overridden{false};
  };
  ```
* **Property Overrides**: Changes to prefab instances are marked in blue in the Inspector with options to **Apply Overrides to Base Prefab** or **Revert to Prefab Defaults**.

---

### 2.8. Content Browser & Asset Management (`content_browser.h`)

* Folder tree navigation through the project's `/assets/`, `/maps/`, and `/scripts/`.
* Thumbnail generation for 3D meshes, materials, and textures.
* **Asset Quick-Preview**:
  * Audio files: mini waveform with **Play / Stop** preview button.
  * Textures: resolution and channel format display.
  * 3D Meshes: isolated thumbnail render.
* **Drag-and-Drop Payloads**:
  * Drag `.glb` $\rightarrow$ Spawns Mesh in Viewport.
  * Drag `.png` $\rightarrow$ Assigns Texture in Inspector.
  * Drag `.lua` $\rightarrow$ Attaches `ScriptComponent`.
  * Drag `.prefab.toml` $\rightarrow$ Spawns Prefab Instance.

---

### 2.9. Diagnostics, Physics Wireframes & Console (`console_panel.h`, `profiler_panel.h`)

* **Live Log Console**: Captures all `LOG_INFO`, `LOG_WARN`, `LOG_ERROR` messages into an interactive ImGui table with search, severity filter toggles, and copy-to-clipboard.
* **Profiler Panel**: Graphs Frame Time (CPU vs GPU ms), Draw Call count, Triangle count, VRAM allocation, and per-thread Job System worker load.
* **Physics Debug Wireframes**: Integrates with Jolt Physics `DebugRenderer` to draw collision bounds (Boxes, Spheres, Capsules, Convex Hulls) directly in the Vulkan Viewport.

---

## 3. Phased Implementation Roadmap

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       EDITOR IMPLEMENTATION PHASING                         │
├─────────────────────────────────────────────────────────────────────────────┤
│ Milestone 1: Editor Shell, DockSpace Layout & Top Menu Bar                  │
│ Milestone 2: Scene Outliner & Dynamic Component Inspector                   │
│ Milestone 3: 3D Viewport Tooling, Gizmos (W/E/R) & Raycast Picking          │
│ Milestone 4: PIE State Machine (Play/Pause/Simulate/Step) & Snapshotting    │
│ Milestone 5: Undo / Redo Command History System                             │
│ Milestone 6: Content Browser, Prefab System & Asset Drag-and-Drop           │
│ Milestone 7: Diagnostics (Physics Wireframes, Log Console & Profiler)       │
│ Milestone 8: Game Packaging, Cooker & Standalone Distribution Exporter      │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Milestone 1: Editor Shell, DockSpace Layout & Top Menu Bar
* **Files:** `editor/CMakeLists.txt`, `editor/include/editor/editor_app.h`, `editor/src/main.cpp`, `editor/src/editor_app.cpp`
* **Tasks:**
  1. Create `editor/CMakeLists.txt` linking `engine_master` and ImGui dependencies.
  2. Implement `EditorApp` wrapping SDL3 event loop and Vulkan swapchain.
  3. Set up `ImGui::DockSpaceOverViewport()` with default panel layout:
     * Top: Menu Bar (`File`, `Edit`, `GameObject`, `Window`, `Help`)
     * Center: Viewport
     * Left: Outliner (Top) & Content Browser (Bottom)
     * Right: Inspector & Environment
     * Bottom: Console & Profiler
  4. Implement Status Bar at the bottom showing current project name, FPS, and active mode.

### Milestone 2: Scene Outliner & Dynamic Component Inspector
* **Files:** `outliner_panel.h/.cpp`, `inspector_panel.h/.cpp`, `selection_context.h/.cpp`
* **Tasks:**
  1. Build `SelectionContext` supporting single and multi-entity selection.
  2. Implement `OutlinerPanel` with recursive tree traversal of Flecs entities, search filter, and drag-and-drop parenting.
  3. Implement `InspectorPanel` rendering fields for `TransformComponent`, `MeshRendererComponent`, `DirectionalLightComponent`, `PointLightComponent`, `SpotLightComponent`, `CameraComponent`, `RigidBodyComponent`, and `ScriptComponent`.
  4. Add "Add Component" popup with search filter.

### Milestone 3: 3D Viewport Tooling, Gizmos & Raycast Picking
* **Files:** `scene_viewport.h/.cpp`
* **Tasks:**
  1. Implement offscreen render-to-texture framebuffer in Vulkan and blit to `ImGui::Image`.
  2. Integrate `ImGuizmo` with shortcuts: `W` (Translate), `E` (Rotate), `R` (Scale), `Q` (Select).
  3. Add grid snapping (0.1m, 0.5m, 1.0m) and angle snapping (15°, 45°, 90°).
  4. Implement mouse raycast picking to select entities by clicking directly on 3D meshes in the viewport.
  5. Implement `F` key shortcut to smoothly focus camera on selected entities.
  6. Add Viewport Shading dropdown: `Lit`, `Unlit`, `Wireframe`, `Normals`, `Roughness/Metallic`.

### Milestone 4: PIE State Machine & Scene Snapshotting
* **Files:** `editor_state.h/.cpp`
* **Tasks:**
  1. Implement `EditorStateManager` with `Edit`, `Play`, `Simulate`, `Paused` modes.
  2. Implement deep cloning of active Flecs scene into memory snapshot when hitting **Play**.
  3. Enable Jolt physics simulation and tick Lua scripts in Play/Simulate mode.
  4. Cleanly restore scene snapshot when clicking **Stop** with zero lost edits.
  5. Implement **Step Frame** ($1/60\text{s}$ tick advance).

### Milestone 5: Undo / Redo Command History System
* **Files:** `command_history.h/.cpp`
* **Tasks:**
  1. Implement `IEditorCommand` base class and `CommandHistory` stack.
  2. Implement `EntityTransformCommand` triggered on ImGuizmo drag release.
  3. Implement `ComponentModifyCommand` for Inspector float/color/bool changes.
  4. Implement `EntityCreateDeleteCommand` and `ParentChangeCommand`.
  5. Bind standard shortcuts: `Ctrl+Z` (Undo), `Ctrl+Y` / `Ctrl+Shift+Z` (Redo).

### Milestone 6: Content Browser, Prefab System & Asset Drag-and-Drop
* **Files:** `content_browser.h/.cpp`, `prefab_manager.h/.cpp`, `asset_importer.h/.cpp`
* **Tasks:**
  1. Build `ContentBrowserPanel` visualizing `/assets/`, `/maps/`, `/scripts/` folders.
  2. Implement SDL3 file drag-and-drop for glTF, textures, and audio.
  3. Implement audio quick-preview (play/stop miniaudio preview on click).
  4. Implement Prefab creation (drag from Outliner to Browser) and instancing (drag from Browser to Viewport).
  5. Implement UUID-safe asset renaming and moving.

### Milestone 7: Visual Diagnostics, Console & Profiler
* **Files:** `console_panel.h/.cpp`, `profiler_panel.h/.cpp`, `debug_draw_pass.h/.cpp`
* **Tasks:**
  1. Build `ConsolePanel` capturing `LOG_INFO`, `LOG_WARN`, `LOG_ERROR` with regex filter and clear button.
  2. Build `ProfilerPanel` rendering real-time graphs for CPU/GPU ms, draw calls, triangles, and VRAM.
  3. Build `DebugDrawPass` drawing Jolt physics collision wireframes (Box, Sphere, Capsule) in the 3D viewport.
  4. Implement `AutosaveManager` periodically saving recovery snapshots to `.intermediate/autosave/`.

### Milestone 8: Game Packaging & One-Click Exporter
* **Files:** `game_exporter.h/.cpp`
* **Tasks:**
  1. Build Packaging Dialog with target platform and configuration (Debug / Release).
  2. Scan `startup_map` and recursively prune unused assets.
  3. Invoke `pak_builder` to package maps and assets into `game_data.pak`.
  4. Copy stripped runtime executable (`game.exe` $\rightarrow$ `<ProjectName>.exe`) and runtime DLLs into `dist/<ProjectName>/`.
