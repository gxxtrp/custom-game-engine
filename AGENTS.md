# Engine Architecture & Development Rules

These rules are mandatory constraints for all AI agents and developers working on this codebase.

---

## 0. DO NOT TOUCH GIT
* **do not run any git command**
* **do not touch git folder**

## 1. 🛡️ Codebase & Module Isolation (Strict DAG)

### A. Three-Tier Isolation
* **`engine/` (Core Runtime Library)**:
  - Must remain **100% isolated**.
  - Must **NEVER** include headers from or reference `editor/`, `runtime/`, or `sandbox/`.
  - Modules within `engine/` must strictly follow a **Directed Acyclic Graph (DAG)**:
    `engine_core` $\rightarrow$ `engine_rhi` / `engine_scene` $\rightarrow$ `engine_renderer` $\rightarrow$ `engine_master`.
  - **Zero circular dependencies** are permitted between any engine modules.
* **`editor/` (Standalone Editor Shell)**:
  - Consumes `engine_master` and `engine_ui`.
  - Must **NEVER** include or link against `runtime/`.
* **`runtime/` (Standalone Lean Game Player)**:
  - Consumes `engine_master`.
  - Must **NEVER** include or link against `editor/` or `engine_ui`.

---

## 2. 📁 Dynamic Project Agnosticism (No Hardcoded Paths)

* **Game Projects Can Reside Anywhere**:
  - A game project is an independent directory containing a `project.toml` and its own `assets/`, `maps/`, `scripts/`, and `config/` folders.
  - **NEVER** hardcode paths to specific projects (e.g., `sandbox_project`, `projects/ActionDemo`) in engine, editor, or runtime source code.
* **Dynamic VFS Mounts**:
  - `runtime.exe` and `editor.exe` accept the project location dynamically via `--project <path>` or local discovery.
  - All asset, texture, mesh, sound, and script lookups must route through the Virtual File System (`/assets`, `/maps`, `/config`) mounted at runtime from the active project root.

---

## 3. 🏭 Production-Grade Engineering (No Prototypes / No Hardcoding)

* **Zero Hardcoded Heuristics**:
  - **NEVER** use string-matching heuristics (e.g., `name.find("Ground")`, `name.find("Box")`) to deduce colors, meshes, or material properties.
  - All rendering and gameplay data must be driven strictly by **ECS components** (`MaterialComponent`, `MeshRendererComponent`, `TransformComponent`, `RigidBodyComponent`, etc.).
* **Production Shading & Math**:
  - Use exact physically-based rendering formulas: Cook-Torrance GGX Specular BRDF ($D_{GGX}, G_{Smith}, F_{Schlick}$), energy conservation, 4-split Cascaded Shadow Maps with sub-pixel texel snapping, and ACES/AgX HDR tonemapping.
  - Use exact 6-plane Gribb-Hartmann normalized geometric frustum culling.
* **Complete Persistence**:
  - Every component added to the engine must be fully supported in `MapSerializer` (both TOML serialization and deserialization) so saving/loading maps never causes data loss.

---

## 4. 🛑 Ambiguity & Halt Policy

* If any requirement, shader layout, or architectural boundary is ambiguous or underspecified:
  - **HALT IMMEDIATELY and ask the user for clarification before writing code.**
