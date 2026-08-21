# Domain Glossary & Context

This document defines the canonical domain terminology for the **Modern Game Engine** architecture. All subsystems, interfaces, tests, and documentation must adhere to these terms.

---

## 1. Core Architecture & Extensibility

* **`EngineKernel`**: The central orchestrator that resolves subsystem dependencies via Directed Acyclic Graph (DAG) topological sorting (Kahn's algorithm) and drives phase-based execution and reverse shutdown.
* **`ISubsystem`**: The interface implemented by runtime modules (`RhiSubsystem`, `PhysicsSubsystem`, `AudioSubsystem`, `ScriptSubsystem`, `SceneSubsystem`) to participate in kernel lifecycle and declare dependencies via `SubsystemDependencyBuilder`.
* **`EngineContext`**: A thread-safe, type-indexed service locator passed into subsystems during initialization, ticking, and shutdown for cross-subsystem communication.
* **`ExecutionPhase`**: The 5 deterministic execution phases in each frame tick:
  1. `PreTick`: Platform events, window messaging, input polling, and frame time delta updates.
  2. `Simulation`: Fixed-timestep physics simulation (`PhysicsSystem`), Flecs scene progress (`flecs::world::progress`), and Lua script controller updates.
  3. `PostSimulation`: Scene transform hierarchy evaluation, 3D spatial audio emitter synchronization, and camera/frustum extraction.
  4. `Render`: Scene extraction, RenderGraph pass scheduling, and GPU command buffer recording.
  5. `Present`: Frame submission to `IViewportPresenter` and GPU fence synchronization.
* **`IViewportPresenter`**: The presentation seam decoupling the RHI rendering loop from window handles. Concrete adapters:
  * `WindowSwapchainPresenter`: Production standalone game client rendering to an SDL3 window via Vulkan swapchains.
  * `HeadlessPresenter`: Non-windowed presentation for CI/CD automated test suites and compute jobs.
  * `OffscreenBufferPresenter`: (Future) Renders to an offscreen GPU texture for editor viewports.

---

## 2. Reflection & Persistence

* **`TypeRegistry`**: Compile-time metadata registry storing field types, byte offsets, labels, and flags for ECS components.
* **`MapSerializer`**: Schema-driven TOML serializer and deserializer that traverses reflected component fields dynamically, guaranteeing complete persistence without per-field manual parsing.

---

## 3. Storage & Asset Pipeline

* **`Virtual File System (VFS)`**: Priority-based virtual mount router (`/assets`, `/maps`, `/scripts`, `/config`) isolating game projects from physical filesystem paths.
* **`AssetMeta`**: Sidecar TOML metadata (`.meta`) storing 128-bit RFC 4122 UUIDs, asset types, and dependency graphs.
* **`PakArchive`**: Packed binary archive format for zero-allocation memory-mapped streaming in production builds.

---

## 4. Graphics & Hardware Abstraction

* **`RhiContext`**: Vulkan 1.3 Hardware Abstraction Layer utilizing dynamic rendering, synchronization2, timeline semaphores, and a unified bindless descriptor heap.
* **`RenderGraph`**: Directed Acyclic Graph (DAG) for frame render pass compilation, transient memory aliasing, and barrier placement.
* **`IRenderPassExtension`**: Extensibility hook allowing custom passes (gizmos, picking, custom post-FX) to be injected into the RenderGraph.
