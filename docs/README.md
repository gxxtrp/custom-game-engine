# Modern Game Engine — Documentation Index

Welcome to the **Modern Game Engine** technical documentation. This engine is a high-performance, GPU-driven, modular game engine written in **C++23**, powered by **Vulkan 1.3**, **SDL3**, **Flecs ECS**, **Jolt Physics 3D**, **miniaudio**, and **Lua/Sol2**.

---

## 📚 Table of Contents

1. [**Engine Architecture Overview**](ARCHITECTURE.md)
   * High-level architectural DAG and subsystem dependency graph.
   * Core design philosophies: zero-overhead abstractions, data-oriented design (DOD), GPU-driven rendering, and memory leak tracking.
   * Compilation model (`engine_master.lib` vs standalone executables).

2. [**Runtime Subsystems Specification**](RUNTIME_SPECIFICATION.md)
   * Comprehensive breakdown of all 15 runtime subsystems (`engine/core`, `engine/jobs`, `engine/events`, `engine/config`, `engine/vfs`, `engine/assets`, `engine/project`, `engine/input`, `engine/importer`, `engine/rhi`, `engine/scene`, `engine/renderer`, `engine/physics`, `engine/audio`, `engine/scripting`, `engine/ui`).
   * RHI Bindless Heap, Clustered Froxel Lighting, CSM, Post-Processing (ACES/AgX, Bloom, TAA).
   * Flecs ECS integration, Jolt Physics synchronization, and Lua metatable OOP dispatch.

3. [**Dedicated Editor Roadmap & Specification**](EDITOR_ROADMAP.md)
   * Architectural blueprint for the dedicated **`editor/`** application (`editor.exe`).
   * Play / Pause / Simulate / Step execution loop.
   * Undo/Redo command history pattern.
   * Asset drag-and-drop, Lua live hot-reloading, material graphs, and one-click game export.

4. [**Building and Running**](BUILDING.md)
   * Toolchain prerequisites (`clang-cl`, CMake 3.25+, Ninja, vcpkg).
   * Debug and Release CMake Presets.
   * Running the Sandbox testbed and PAK builder tool.

5. [**Original Design & Component DAG (v5)**](design.md)
   * Detailed specification of initial DAG dependency resolution and tech stack choices.
