# Engine Core Refactoring & Renderer Unification Plan

This plan addresses all hardcoded heuristics, prototype shortcuts, and missing component synchronizations across the engine core, while connecting the existing modern renderer subsystems (`Material`, `Lighting`, `Serialization`, `Physics`).

---

## 🎯 Objectives

1. **Eliminate All Hardcoded Heuristics in Rendering**:
   - Create `MaterialComponent` with PBR parameters (`base_color`, `metallic`, `roughness`, `emissive`, texture UUIDs).
   - Purge all string-matching (`name.find("Ground")`, `name.find("Box")`, etc.) and fallback guessing from `SceneRenderer`.
   - Derive directional light direction strictly from `TransformComponent.rotation` (quaternion forward vector).
   - Wire `MaterialComponent` into the Inspector UI with an RGBA color picker and Metallic/Roughness sliders.

2. **Complete `MapSerializer` (Fix Data Loss)**:
   - Serialize and deserialize **all** engine components:
     - `RigidBodyComponent` & `ColliderComponent` (Physics)
     - `ScriptComponent` (Lua script path, class name)
     - `AudioSourceComponent` & `AudioListenerComponent` (Spatial audio)
     - `MaterialComponent` (PBR parameters & texture links)
     - `PointLightComponent`, `SpotLightComponent`, `DirectionalLightComponent`, `CameraComponent`

3. **Fix Physics Collider Scaling**:
   - Multiply box half-extents, sphere radii, and capsule dimensions by `transform.scale` in `PhysicsSystem::create_body`.

4. **Fix Audio Source Synchronization**:
   - Implement `AudioEngine::sync_ecs_audio` so entities with `AudioSourceComponent` can play, loop, and update their 3D emitter positions in real-time.

5. **Fix Game Exporter Packaging Target**:
   - Point `GameExporter` to bundle the dedicated `runtime/runtime.exe` executable.

---

## Proposed Changes

### Component 1: Engine Scene & Components (`engine/scene`)

#### [MODIFY] [components.h](file:///c:/Users/x/work/my-engine/engine/scene/include/engine/scene/components.h)
- Define `MaterialComponent`:
  ```cpp
  struct MaterialComponent {
      assets::UUID material_uuid{assets::UUID::generate()};
      core::Vec4 base_color{0.8f, 0.8f, 0.8f, 1.0f};
      float metallic{0.0f};
      float roughness{0.5f};
      core::Vec3 emissive{0.0f, 0.0f, 0.0f};
      float emissive_strength{1.0f};
      assets::UUID albedo_texture_uuid;
      assets::UUID normal_texture_uuid;
      assets::UUID metallic_roughness_texture_uuid;
  };
  ```
- Register `MaterialComponent` in `Scene::register_components()`.

#### [MODIFY] [scene.cpp](file:///c:/Users/x/work/my-engine/engine/scene/src/scene.cpp)
- Register `MaterialComponent` in Flecs component registry.

#### [MODIFY] [map_serializer.cpp](file:///c:/Users/x/work/my-engine/engine/scene/src/map_serializer.cpp)
- Add serialization and deserialization blocks for:
  - `MaterialComponent`
  - `RigidBodyComponent` (mass, friction, restitution, damping, gravity factor, is_sensor, motion_type)
  - `ColliderComponent` (shape_type, box_half_extents, radius, half_height, offset, rotation)
  - `ScriptComponent` (script_path, class_name)
  - `AudioSourceComponent` (sound_name, volume, pitch, loop, is_3d, min_dist, max_dist)
  - `AudioListenerComponent` (is_active)

---

### Component 2: Engine Renderer (`engine/renderer`)

#### [MODIFY] [scene_renderer.cpp](file:///c:/Users/x/work/my-engine/engine/renderer/src/scene_renderer.cpp)
- Purge all `name.find(...)` color hacks.
- Read `MaterialComponent.base_color`, `metallic`, and `roughness` directly from the entity if present (defaulting to clean neutral white `(0.85, 0.85, 0.85, 1.0)` if missing).
- Calculate directional light vector using `transform.rotation.rotate(core::Vec3(0.0f, -1.0f, 0.0f))` instead of `t.position.normalized()`.
- Pass dynamic material roughness into `MeshPushConstants.camera_pos_roughness.w`.

---

### Component 3: Engine Physics & Audio (`engine/physics`, `engine/audio`)

#### [MODIFY] [physics_system.cpp](file:///c:/Users/x/work/my-engine/engine/physics/src/physics_system.cpp)
- Scale Jolt collision shapes by entity transform scale:
  - Box: `col.box_half_extents * transform.scale`
  - Sphere: `col.radius * max(scale.x, scale.y, scale.z)`
  - Capsule: `col.radius * max(scale.x, scale.z)`, `col.half_height * scale.y`

#### [MODIFY] [audio_engine.cpp](file:///c:/Users/x/work/my-engine/engine/audio/src/audio_engine.cpp)
- Implement `sync_ecs_audio` to update active spatial sound emitter positions from `TransformComponent.position`.

---

### Component 4: Editor UI & Tools (`editor/src/panels`, `editor/src/tools`, `editor/src/core`)

#### [MODIFY] [inspector_panel.cpp](file:///c:/Users/x/work/my-engine/editor/src/panels/inspector_panel.cpp)
- Add `draw_material_editor(flecs::entity entity)`:
  - Color edit `ImGui::ColorEdit4("Base Color", ...)`
  - Metallic slider `ImGui::SliderFloat("Metallic", ...)`
  - Roughness slider `ImGui::SliderFloat("Roughness", ...)`
  - Emissive color and strength controls.
- Add "Add Component -> Material" context menu item.

#### [MODIFY] [editor_app.cpp](file:///c:/Users/x/work/my-engine/editor/src/core/editor_app.cpp)
- When creating primitive entities (`Cube`, `Sphere`, `Plane`, `PlayerController`), attach a proper default `MaterialComponent` with distinct curated default colors (e.g., green for Ground, orange for Cube, red for Sphere) stored as **actual component data**, not hardcoded in the renderer.

#### [MODIFY] [game_exporter.cpp](file:///c:/Users/x/work/my-engine/editor/src/tools/game_exporter.cpp)
- Update executable search path to prioritize `build/<config>/runtime/runtime.exe`.

---

## Verification Plan

### Automated Build & Execution
1. **Compilation**: Run `cmake --build --preset x64-clang-release` to verify zero compile or link errors.
2. **Editor Test**: Run `editor.exe --timeout 2.0` to verify editor initializes cleanly with the new MaterialComponent and serialized systems.
3. **Runtime Test**: Run `runtime.exe --project projects/ActionDemo --timeout 2.0` to verify runtime loads and renders the data-driven materials.

### Manual Verification
1. Open Editor, spawn a Cube, change its Color / Roughness in the Inspector, and confirm the viewport updates in real-time.
2. Save the scene (`Ctrl+S`), restart the editor, and verify that Physics, Scripts, and Materials persist without data loss.
3. Scale a physics body non-uniformly and verify the collision box wireframe matches the scaled mesh.
