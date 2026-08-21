# Stage 1: Enterprise Architecture & Extensibility Implementation Plan

This document defines the actionable implementation plan for Stage 1 of the engine modernization. It focuses on establishing a declarative Directed Acyclic Graph (DAG) subsystem kernel, decoupling windowing via a clean viewport presenter seam, and eliminating manual serialization boilerplate through universal compile-time reflection.

---

## 1. Objectives & Architectural Boundaries

1. **Retain Canonical Module Layout**: Preserve `engine/` (`engine/core`, `engine/rhi`, `engine/scene`, `engine/renderer`, etc.) and `runtime/` as mandated by [AGENTS.md](file:///c:/Users/x/work/my-engine/AGENTS.md).
2. **Subsystem Kernel Orchestration**: Replace hardcoded procedural startup in `Engine::init()` with `EngineKernel` (topological sort via Kahn's algorithm, 5-phase tick execution, reverse-order shutdown).
3. **Viewport Presentation Seam (`IViewportPresenter`)**: Introduce `WindowSwapchainPresenter` (standalone client) and `HeadlessPresenter` (zero-window CI/test suites).
4. **Compile-Time Reflection (`TypeRegistry`)**: Provide macro-based schema reflection to automate `MapSerializer` TOML save/load and guarantee 100% persistence for all components.
5. **Dual-Path VFS Asset Pipeline**: Support direct live asset loading in development and zero-parse `.pak` streaming in release builds.

---

## 2. Implementation Tasks by Component

### Component 1: Engine Core Foundation (`engine/core`)
- [ ] **Task 1.1**: Create `engine/core/include/engine/core/subsystem.h` defining `ISubsystem`, `ExecutionPhase`, and `SubsystemDependencyBuilder`.
- [ ] **Task 1.2**: Create `engine/core/include/engine/core/engine_context.h` providing thread-safe type-indexed service location.
- [ ] **Task 1.3**: Create `engine/core/include/engine/core/engine_kernel.h` and `engine/core/src/engine_kernel.cpp` implementing `EngineKernel` and `KernelBuilder` with Kahn's DAG topological sort.
- [ ] **Task 1.4**: Create `engine/core/include/engine/core/reflection.h` defining `TypeRegistry` and macro-driven field reflection (`REFLECT_STRUCT_BEGIN`, `REFLECT_FIELD`, `REFLECT_STRUCT_END`).

### Component 2: Hardware Abstraction & Presentation Seam (`engine/rhi`)
- [ ] **Task 2.1**: Create `engine/rhi/include/engine/rhi/viewport_presenter.h` defining `IViewportPresenter`.
- [ ] **Task 2.2**: Implement `WindowSwapchainPresenter` in `engine/rhi/src/window_swapchain_presenter.cpp` wrapping SDL3 window + Vulkan swapchain presentation.
- [ ] **Task 2.3**: Implement `HeadlessPresenter` in `engine/rhi/src/headless_presenter.cpp` for non-windowed test and CI environments.

### Component 3: Scene Subsystem & Reflected Persistence (`engine/scene`)
- [ ] **Task 3.1**: Declare reflection metadata for all scene components in `engine/scene/include/engine/scene/components.h`.
- [ ] **Task 3.2**: Refactor `MapSerializer` in `engine/scene/src/map_serializer.cpp` into a generic TOML visitor driven by `TypeRegistry`.
- [ ] **Task 3.3**: Implement `SceneSubsystem` in `engine/scene/src/scene_subsystem.cpp` participating in `ExecutionPhase::Simulation` and `ExecutionPhase::PostSimulation`.

### Component 4: Subsystem Adapters for Core Systems
- [ ] **Task 4.1**: Implement `ISubsystem` on `PhysicsSystem` (`engine/physics`).
- [ ] **Task 4.2**: Implement `ISubsystem` on `AudioEngine` (`engine/audio`).
- [ ] **Task 4.3**: Implement `ISubsystem` on `ScriptEngine` (`engine/scripting`).

### Component 5: Orchestration & Standalone Runtime
- [ ] **Task 5.1**: Refactor `Engine` in `engine/src/engine.cpp` to initialize and tick through `EngineKernel` and `IViewportPresenter`.
- [ ] **Task 5.2**: Update `RuntimeApp` in `runtime/src/runtime_app.cpp` to construct `EngineKernel` with `--headless` and `--project` support.

---

## 3. Verification Plan

1. **Kernel DAG Topological Sorter Unit Tests**: Verify linear dependency sorting, cycle detection rejection, and reverse-order shutdown.
2. **Reflection Round-Trip Tests**: Assert that serializing and deserializing reflected components produces exact representations.
3. **Headless Engine Simulation Test**: Execute `runtime.exe --headless --timeout 2.0` to verify 120 frames execute with zero window creation.
4. **Standalone Client Test**: Launch `runtime.exe --project <project_path>` to verify rendering, physics, audio, and scripts.
