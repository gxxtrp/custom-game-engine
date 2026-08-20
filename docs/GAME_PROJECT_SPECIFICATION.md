# Game Project Architecture & Specification

This document details the architectural standards, disk layout, manifest schema, and lifecycle for standalone **Game Projects** created with **Modern Game Engine**.

---

## 1. Vision & Core Philosophy

1. **Engine / Project Decoupling**: A Game Project is completely independent of the engine source repository. Projects can live anywhere on the user's filesystem (e.g. `D:/MyProjects/CyberShooter/`).
2. **Virtual File System (VFS) Isolation**: All assets and maps are referenced through virtual paths (`/assets/...`, `/maps/...`, `/scripts/...`). Moving a project folder or sharing it across team members never breaks asset paths.
3. **Dual Gameplay Tiers**:
   * **Lua Scripting (`sol2`)**: For fast iterative gameplay logic, entity controllers, and rapid prototyping without recompilation.
   * **Native C++ Game Modules (`.dll`)**: For high-performance custom ECS components, systems, and algorithms.
4. **Clean Asset Lifecycle**: Raw assets (`.glb`, `.png`, `.wav`) are imported with companion `.meta` sidecar files tracking 128-bit UUIDs for rock-solid reference integrity.

---

## 2. Project Directory Structure

Every Game Project conforms to the following standardized layout on disk:

```
<ProjectRoot>/                              <-- Standalone Project Directory (e.g., CyberShooter/)
├── project.toml                            <-- Primary Project Manifest & Configuration
│
├── assets/                                 <-- Content Root (Mounted to VFS as `/assets`)
│   ├── models/                             <-- 3D Meshes (.glb, .gltf) + .meta files
│   ├── textures/                           <-- Images (.png, .ktx2, .hdr) + .meta files
│   ├── materials/                          <-- PBR Material Definitions (.toml) + .meta files
│   ├── audio/                              <-- Sound FX & Music (.wav, .mp3, .ogg) + .meta files
│   ├── prefabs/                            <-- Reusable Entity Prefabs (.prefab.toml)
│   └── shaders/                            <-- Custom Project Shaders (.slang, .spv)
│
├── maps/                                   <-- Level & Scene Definitions (Mounted to VFS as `/maps`)
│   ├── main_menu.map                       <-- Scene TOML map
│   ├── level_01.map
│   └── sandbox.map
│
├── scripts/                                <-- Gameplay Logic (Mounted to VFS as `/scripts`)
│   ├── main.lua                            <-- Optional Game entrypoint script
│   ├── controllers/                        <-- Entity controllers (player, camera, vehicles)
│   ├── ai/                                 <-- State machines & behavior trees
│   └── ui/                                 <-- In-game HUD & menu logic
│
├── config/                                 <-- Project Settings (Mounted to VFS as `/config`)
│   ├── input.toml                          <-- Action & Axis Mappings (Keyboard, Mouse, Gamepad)
│   ├── physics.toml                        <-- Collision Layers & Filtering Matrix
│   └── audio.toml                          <-- Audio Busses & Volume Mixers
│
├── src/                                    <-- (Optional) Native C++ Game Module
│   ├── game_module.cpp                     <-- Custom Flecs Components & Systems
│   └── CMakeLists.txt                      <-- Builds `CyberShooter.dll` linking `engine_master`
│
└── .intermediate/                          <-- Temporary Artifacts (Excluded in .gitignore)
    ├── cache/                              <-- Derived Data Cache (DDC) & Shader Bytecode
    ├── logs/                               <-- Runtime & Editor session logs
    └── editor_layout.ini                   <-- ImGui dockspace state & window positions
```

---

## 3. Project Manifest Specification (`project.toml`)

The `project.toml` file at the root defines all metadata, startup scenes, display parameters, and subsystem configs:

```toml
[project]
name = "Cyber Shooter"
version = "0.1.0"
engine_version = "1.0.0"
project_id = "e4d3a2b1-9c8e-4f7a-b6c5-1a2b3c4d5e6f"
startup_map = "/maps/main_menu.map"
description = "A high-octane cyberpunk tactical shooter"
author = "Dev Studio"

[display]
window_title = "Cyber Shooter"
width = 1920
height = 1080
vsync = true
fullscreen = false
target_framerate = 144
resizable = true

[rendering]
ray_tracing = true
shadow_quality = "high"             # "low", "medium", "high", "ultra"
shadow_resolution = 2048
hdr_color_grading = true
bloom_intensity = 0.05
tone_mapper = "AgX"                 # "ACES", "AgX", "Reinhard", "Linear"

[physics]
fixed_timestep = 0.0166667          # 60 Hz Jolt simulation step
gravity = [0.0, -9.81, 0.0]
max_bodies = 10240
max_body_pairs = 65536

[audio]
master_volume = 1.0
sfx_volume = 0.8
music_volume = 0.7
spatial_audio = true

[modules]
native_module = ""                  # Optional: "bin/CyberShooter.dll"
```

---

## 4. Virtual File System (VFS) Tiered Mounting

When a project is loaded, the engine initializes the VFS with prioritized mount tiers:

```
                            VIRTUAL FILE SYSTEM (VFS)
                                        │
             ┌──────────────────────────┴──────────────────────────┐
             ▼                                                     ▼
 [ PRIORITY 10: USER PROJECT ]                         [ PRIORITY 1: ENGINE CORE ]
   `<ProjectRoot>/assets/`  -> `/assets`                 `<EngineRoot>/assets/` -> `/engine_assets`
   `<ProjectRoot>/maps/`    -> `/maps`                   (Built-in PBR shaders, fallback pink texture,
   `<ProjectRoot>/scripts/` -> `/scripts`                 default fonts, editor gizmo icons)
   `<ProjectRoot>/config/`  -> `/config`
```

* **Overriding Engine Defaults**: If a user places a file at `/assets/shaders/pbr.frag`, it takes priority over `/engine_assets/shaders/pbr.frag`.
* **Clean Fallbacks**: If a texture or mesh fails to load, the engine safely resolves to `/engine_assets/textures/missing_texture.png`.

---

## 5. Gameplay Logic Architecture

### 5.1. Lua Scripting Layer (`sol2`)
* Every script defines a table with standard lifecycle callbacks:
  ```lua
  -- scripts/controllers/player_controller.lua
  local PlayerController = {}

  function PlayerController:on_init(entity)
      self.move_speed = 10.0
      self.jump_force = 7.5
      self.rigidbody = entity:get_rigidbody()
      self.audio = entity:get_audio_source()
  end

  function PlayerController:on_update(entity, dt)
      local move = Input.get_vector2("MoveForward", "MoveRight")
      self.rigidbody:set_linear_velocity(Vec3(move.x * self.move_speed, 0, move.y * self.move_speed))

      if Input.is_action_just_pressed("Jump") and self.rigidbody:is_grounded() then
          self.rigidbody:add_impulse(Vec3(0, self.jump_force, 0))
          self.audio:play_sound("/assets/audio/sfx/jump.wav")
      end
  end

  function PlayerController:on_collision(entity, other, hit_info)
      -- Handle impact damage or triggers
  end

  function PlayerController:on_destroy(entity)
      -- Cleanup
  end

  return PlayerController
  ```

### 5.2. Native C++ Game Module (`.dll`)
* For performance-critical systems, custom AI pathfinding, or custom shaders:
  ```cpp
  // src/game_module.cpp
  #include <engine/scene/scene.h>
  #include <engine/core/log.h>

  struct HealthComponent {
      float current_health{100.0f};
      float max_health{100.0f};
  };

  extern "C" GAME_API void register_game_module(engine::scene::Scene& scene) {
      auto world = scene.get_world();

      world.component<HealthComponent>();

      world.system<HealthComponent>("HealthRegenSystem")
          .each([](HealthComponent& health) {
              if (health.current_health < health.max_health) {
                  health.current_health += 1.0f * 0.016f;
              }
          });

      LOG_INFO("GameModule", "CyberShooter native module registered successfully!");
  }
  ```

---

## 6. Input & Action Mapping (`config/input.toml`)

Decouples physical device inputs (Keyboard, Mouse, Gamepad) from gameplay actions:

```toml
# config/input.toml

[actions.Jump]
keys = ["Space", "Gamepad_ButtonA"]

[actions.Fire]
keys = ["Mouse_Left", "Gamepad_RightTrigger"]

[actions.Interact]
keys = ["Key_E", "Gamepad_ButtonX"]

[axes.MoveForward]
positive = ["Key_W", "Gamepad_LeftStickUp"]
negative = ["Key_S", "Gamepad_LeftStickDown"]

[axes.MoveRight]
positive = ["Key_D", "Gamepad_LeftStickRight"]
negative = ["Key_A", "Gamepad_LeftStickLeft"]

[axes.LookX]
mouse_axis = "Mouse_DeltaX"
gamepad_axis = "Gamepad_RightStickX"
sensitivity = 0.15

[axes.LookY]
mouse_axis = "Mouse_DeltaY"
gamepad_axis = "Gamepad_RightStickY"
sensitivity = 0.15
invert = true
```

---

## 7. Project Hub & Workflow Lifecycle

```
                           [ PROJECT HUB (project_hub.h) ]
                                         │
                 ┌───────────────────────┴───────────────────────┐
                 ▼                                               ▼
     [ CREATE NEW PROJECT ]                            [ OPEN EXISTING PROJECT ]
     • Select Template (FPS, TPS, Empty)               • Browse for `project.toml`
     • Specify Project Name & Path                     • Or pick from Recent Projects
                 │                                               │
                 └───────────────────────┬───────────────────────┘
                                         ▼
                             [ LAUNCH EDITOR (editor.exe) ]
                             • Mount Project to VFS
                             • Load `startup_map`
                             • Begin Edit Session
                                         │
                           (Ready for Distribution?)
                                         │
                                         ▼
                             [ ONE-CLICK PACKAGING ]
                             • Cook Assets into `game_data.pak`
                             • Bundle standalone `CyberShooter.exe`
```

---

## 8. Standalone Game Packaging

When packaging a project for release:
1. **Asset Dependency Pruning**: The cooker scans `startup_map` and recursively collects only used assets into `game_data.pak`.
2. **Binary Strip**: Editor UI (`imgui`, `ImGuizmo`), gizmo shaders, and debug symbols are excluded.
3. **Release Bundle**:
   ```
   dist/CyberShooter/
   ├── CyberShooter.exe             <-- Stripped Standalone Runtime
   ├── game_data.pak                <-- Compressed & Encrypted Content Archive
   ├── project.toml                 <-- Read-only Game Configuration
   └── *.dll                        <-- Required Runtime DLLs (Vulkan, SDL3, Lua, etc.)
   ```
