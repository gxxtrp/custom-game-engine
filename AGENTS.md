# Engine Architecture & Development Rules

These rules are mandatory constraints for all AI agents and developers working on this codebase.

---

## 0. DO NOT TOUCH GIT
* **do not run any git command**
* **do not touch git folder**

---

## 1. 🛡️ Codebase & Module Isolation (Strict DAG)

### A. Modular Engine Core Stack
* **`engine/` (Core Runtime Library - 100% Headless & UI-Agnostic)**:
  - Must remain **100% isolated** from any UI or editor tooling.
  - Modules within `engine/` strictly follow a **Directed Acyclic Graph (DAG)**:
    `engine_core` $\rightarrow$ `engine_rhi` / `engine_scene` $\rightarrow$ `engine_renderer` $\rightarrow$ `engine_master`.
  - **Zero circular dependencies** are permitted between any engine modules.
* **`runtime/` (Standalone High-Performance Game Player)**:
  - Consumes `engine_master`.
  - Lean, direct client for standalone gameplay simulation and rendering.

---

## 2. 📁 Dynamic Project Agnosticism (No Hardcoded Paths)

* **Game Projects Can Reside Anywhere**:
  - A game project is an independent directory containing a `project.toml` and its own `assets/`, `maps/`, `scripts/`, and `config/` folders.
  - **NEVER** hardcode paths to specific projects in engine or runtime source code.
* **Dynamic VFS Mounts**:
  - `runtime.exe` accepts the project location dynamically via `--project <path>` or local discovery.
  - All asset, texture, mesh, sound, and script lookups route through the Virtual File System (`/assets`, `/maps`, `/config`) mounted at runtime from the active project root.

---

## 3. 🏭 Production-Grade Engineering (No Prototypes / No Hardcoding)

* **Zero Hardcoded Heuristics**:
  - **NEVER** use string-matching heuristics to deduce colors, meshes, or material properties.
  - All rendering and gameplay data is driven strictly by **ECS components** (`MaterialComponent`, `MeshRendererComponent`, `TransformComponent`, `RigidBodyComponent`, etc.).
* **Production Shading & Math**:
  - Use exact physically-based rendering formulas: Cook-Torrance GGX Specular BRDF ($D_{GGX}, G_{Smith}, F_{Schlick}$), energy conservation, 4-split Cascaded Shadow Maps with sub-pixel texel snapping, and ACES/AgX HDR tonemapping.
  - Use exact 6-plane Gribb-Hartmann normalized geometric frustum culling.
* **Complete Persistence**:
  - Every component added to the engine must be fully supported in `MapSerializer` (both TOML serialization and deserialization).

---

## 4. 🛑 Ambiguity & Halt Policy

* If any requirement, shader layout, or architectural boundary is ambiguous or underspecified:
  - **HALT IMMEDIATELY and ask the user for clarification before writing code.**
