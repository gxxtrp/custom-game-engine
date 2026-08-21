# Modern Game Engine — Documentation Index

Welcome to the **Modern Game Engine** technical documentation. This engine is a high-performance, GPU-driven, modular game engine written in **C++23**, powered by **Vulkan 1.3**, **SDL3**, **Flecs ECS**, **Jolt Physics 3D**, **miniaudio**, and **Lua/Sol2**.

---

## 📚 Table of Contents

1. [**Domain Glossary (`CONTEXT.md`)**](../CONTEXT.md)
   * Canonical terms: `EngineKernel`, `ISubsystem`, `EngineContext`, `IViewportPresenter`, `TypeRegistry`, `ExecutionPhase`, `MapSerializer`.

2. [**Engine Architecture Overview**](ARCHITECTURE.md)
   * High-level architectural DAG and subsystem dependency graph.
   * `EngineKernel` 5-phase execution model (`PreTick`, `Simulation`, `PostSimulation`, `Render`, `Present`).
   * `IViewportPresenter` presentation seam (Standalone vs Headless).
   * Compilation model (`engine_master.lib` consumed by `runtime.exe`).

3. [**Runtime Subsystems Specification**](RUNTIME_SPECIFICATION.md)
   * Comprehensive breakdown of runtime subsystems (`engine/core`, `engine/jobs`, `engine/events`, `engine/config`, `engine/vfs`, `engine/assets`, `engine/project`, `engine/input`, `engine/importer`, `engine/rhi`, `engine/scene`, `engine/renderer`, `engine/physics`, `engine/audio`, `engine/scripting`).
   * Compile-time type reflection and automated `MapSerializer` TOML persistence.
   * RHI Bindless Heap, Clustered Froxel Lighting, CSM, Post-Processing (ACES/AgX, Bloom, TAA).
   * Flecs ECS integration, Jolt Physics synchronization, and Lua OOP dispatch.

4. [**Game Project Specification**](GAME_PROJECT_SPECIFICATION.md)
   * Standalone project directory layout, `project.toml` manifest schema, and VFS mount tiers.
   * Dual gameplay logic tiers (Lua scripting and C++ game modules).
   * Input mapping, audio busses, and standalone release packaging.

5. [**Building and Running**](BUILDING.md)
   * Toolchain prerequisites (`clang-cl`, CMake 3.25+, Ninja, vcpkg).
   * Debug and Release CMake Presets.
   * Running the `runtime.exe` standalone player and `pak_builder.exe` tool.
