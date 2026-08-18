# Dedicated Editor Roadmap & Architecture Specification

This document details the architectural plan and technical roadmap for the standalone **Game Engine Editor** (`editor/` producing `editor.exe`).

---

## 1. Overview & Vision

While the core runtime (`engine/`) provides the underlying SDK and basic UI widgets (`engine/ui`), the dedicated **Editor Application** (`editor/`) serves as the authoring suite for level design, gameplay scripting, asset importing, and game distribution.

---

## 2. Editor Architecture Blueprint

```
my-engine/
├── engine/                       <-- Pure Runtime SDK & Libraries (`engine_master.lib`)
├── tools/                        <-- Asset Cooker & CLI utilities (`pak_builder.exe`)
│
├── editor/                       <-- DEDICATED EDITOR APPLICATION (`editor.exe`)
│   ├── include/editor/
│   │   ├── editor_app.h          <-- Main Editor Application Driver
│   │   ├── editor_state.h        <-- Play/Pause/Simulate Execution State
│   │   ├── command_history.h     <-- Undo / Redo Command Pattern System
│   │   ├── project_hub.h         <-- Project Launcher & New Project Wizard
│   │   ├── asset_importer.h      <-- Drag-and-drop glTF / Audio / Texture pipeline
│   │   ├── script_watcher.h      <-- FileSystem watcher for Lua live hot-reloading
│   │   ├── material_editor.h     <-- PBR Material Inspector & Shader Presets
│   │   ├── scene_view.h          <-- Dedicated 3D Viewport with Grid & Gizmos
│   │   └── game_exporter.h       <-- Standalone Game Cooker & Binary Packager
│   ├── src/
│   │   ├── main.cpp              <-- Editor Entrypoint
│   │   ├── editor_app.cpp
│   │   ├── command_history.cpp
│   │   ├── project_hub.cpp
│   │   ├── asset_importer.cpp
│   │   ├── script_watcher.cpp
│   │   ├── material_editor.cpp
│   │   ├── scene_view.cpp
│   │   └── game_exporter.cpp
│   └── CMakeLists.txt            <-- Links `engine_master`
```

---

## 3. Key Feature Modules

### 3.1. Play / Pause / Simulate Execution Modes (`editor_state.h`)
* **Edit Mode (Default)**:
  * Camera moves freely via `EditorCamera`.
  * Physics simulation is paused or running in preview mode.
  * Lua scripts do not mutate runtime state; transforms are edited via `ImGuizmo`.
* **Play Mode (Play in Editor / PIE)**:
  * Creates a clone/snapshot of the active Flecs scene.
  * Starts active gameplay camera, Jolt physics dynamics, audio emitters, and Lua controller ticks.
  * Pressing **Stop** cleanly restores the pre-play scene snapshot with zero lost edits.
* **Simulate Mode**:
  * Runs physics and particle simulations while maintaining full `EditorCamera` flycam control.
* **Step Frame**:
  * Advances the simulation by exactly one fixed physics/script step ($1/60\text{s}$) for precise debugging.

---

### 3.2. Undo / Redo Command History (`command_history.h`)
* Classic Command Pattern:
  ```cpp
  class IEditorCommand {
  public:
      virtual ~IEditorCommand() = default;
      virtual void execute() = 0;
      virtual void undo() = 0;
      virtual std::string get_name() const = 0;
  };
  ```
* Supports:
  * `EntityTransformCommand`: Records translation/rotation/scale before and after gizmo release.
  * `EntityCreateDeleteCommand`: Restores deleted entities or cleans up created ones.
  * `ComponentModifyCommand`: Tracks changes made inside the Inspector.
* Standard `Ctrl+Z` (Undo) and `Ctrl+Y` / `Ctrl+Shift+Z` (Redo) shortcuts.

---

### 3.3. Asset Drag-and-Drop & Import Pipeline (`asset_importer.h`)
* SDL3 Drop Event integration:
  * Dragging `.gltf` / `.glb` files into the Viewport automatically spawns a mesh entity with PBR materials.
  * Dragging `.png` / `.ktx2` into the Inspector assigns textures to the active material.
  * Dragging `.wav` / `.mp3` onto an entity automatically adds an `AudioSourceComponent`.
  * Dragging `.lua` script files binds the `ScriptComponent` class name.

---

### 3.4. Script Hot-Reloading (`script_watcher.h`)
* Background filesystem watcher monitoring `assets/scripts/` directory.
* When a `.lua` file is saved in VSCode or external editor:
  1. Lua environment recompiles the changed script chunk.
  2. Updates the script prototype table in `ScriptEngine`.
  3. Re-invokes `on_init` or maintains existing entity state seamlessly without restarting the engine!

---

### 3.5. Material Graph & PBR Preset Editor (`material_editor.h`)
* Real-time preview of Albedo, Normal Map, Metallic-Roughness, Emissive, and Ambient Occlusion.
* Built-in material presets (Gold, Copper, Rough Plastic, Polished Glass, Concrete, Rust).
* Direct serialization into `.toml` material definition assets in `/assets/materials/`.

---

### 3.6. One-Click Game Packaging & Export (`game_exporter.h`)
* Automated build and distribution export:
  1. Compiles project scenes into optimized TOML/binary maps.
  2. Invokes `pak_builder` to generate encrypted/compressed `game_data.pak`.
  3. Copies the stripped standalone game executable (`game.exe`) and required DLLs (`vulkan-1.dll`, `SDL3.dll`, `lua.dll`, `flecs.dll`, `tomlplusplus-3.dll`).
  4. Produces a ready-to-ship distribution folder in `dist/<ProjectName>/`.

---

## 4. Implementation Phasing for `editor/`

1. **Step 1: Editor Application Skeleton & Windowing**
   * Create `editor/CMakeLists.txt` linking `engine_master`.
   * Implement `EditorApp` with top menu bar, status bar, and docking layout.
2. **Step 2: Play/Pause/Simulate State Machine & Scene Snapshotting**
   * Integrate scene cloning with Flecs snapshot/restore.
3. **Step 3: Command History & Undo/Redo**
   * Wire gizmo manipulation and inspector changes to `CommandHistory`.
4. **Step 4: Live Asset Importer & Drag-and-Drop**
   * Integrate SDL3 drag-and-drop for glTF, audio, and textures.
5. **Step 5: Lua Script Live Hot-Reload Watcher**
   * Monitor script files and reload Lua state at runtime.
6. **Step 6: Packaging & Game Exporter**
   * One-click PAK cooking and standalone game directory bundling.
