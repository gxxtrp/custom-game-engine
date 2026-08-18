# Runtime Subsystems Specification

This document provides technical details for every runtime subsystem in the engine.

---

## 1. Engine Core & Foundation (`engine/core`)
* **SIMD Math**: Custom math library (`Vec2`, `Vec3`, `Vec4`, `Mat4`, `Quat`, `Ray`, `Plane`, `Frustum`, `AABB`) with SSE4.2/AVX2 intrinsics. Implements matrix decomposition, Shepperd's quaternion extraction, and right-handed Vulkan projection.
* **Allocators**:
  * `LinearAllocator`: Ultra-fast bump allocator for transient per-frame allocations.
  * `StackAllocator`: Marker-based LIFO memory for scoped subroutines.
  * `PoolAllocator`: Fixed-size O(1) block allocator for ECS components.
  * `GlobalAllocator`: Heap tracker recording allocation count, active bytes, and peak usage.
* **Platform & Windowing**: SDL3 integration with raw event hook and window lifecycle management.

---

## 2. Job System (`engine/jobs`)
* Lock-free Chase-Lev work-stealing deque per worker thread.
* Supports parent-child job dependency counters (`Counter`), parallel for-each batching, and priority queues.

---

## 3. Event Bus (`engine/events`)
* Type-erased, thread-safe publish-subscribe message broker.
* Immediate and deferred queued event dispatch with listener tokens for RAII subscription lifetimes.

---

## 4. Configuration & CVars (`engine/config`)
* TOML v3 parser/serializer using `toml++`.
* Strongly-typed console variables (`CVar<int>`, `CVar<float>`, `CVar<std::string>`, `CVar<bool>`) with min/max clamps and runtime change callbacks.

---

## 5. Virtual File System & PAK Archives (`engine/vfs`)
* Priority-based virtual mount router (`/assets`, `/config`, `/maps`).
* `PhysicalMountPoint`: Reads and writes directly from OS disk directories.
* `PakArchiveMountPoint`: Encapsulates packed binary `.pak` files with header tables, TOC offsets, and zero-allocation streaming.

---

## 6. Asset Management & Project (`engine/assets`, `engine/project`)
* 128-bit RFC 4122 UUID asset identification.
* Manifest-based project directory loader (`project.toml`) with automatic VFS mount discovery.
* glTF 2.0 asset importer (`engine/importer`) via `cgltf`.

---

## 7. Flecs ECS Scene System (`engine/scene`)
* Archetype-based entity-component-system (Flecs 4.1.6).
* Core components: `TagComponent`, `UUIDComponent`, `TransformComponent`, `WorldTransformComponent`, `CameraComponent`.
* Hierarchy propagation: Parent-child relationship tracking with automated world matrix evaluation.
* Map Serializer: TOML level save/load mechanism.

---

## 8. Vulkan 1.3 RHI & Bindless Heap (`engine/rhi`)
* **Instance & Device**: Vulkan 1.3 core API with dynamic rendering, synchronization2, buffer device address (BDA), maintenance4, and descriptor indexing.
* **VMA Integration**: Fast GPU memory allocation for buffers (`VkBuffer`) and images (`VkImage`).
* **Bindless Descriptor Heap**:
  * `16,384` Combined Image Samplers (`set = 0, binding = 0`)
  * `16,384` Storage Buffers (`set = 0, binding = 1`)
  * `256` Immutable Samplers (`set = 0, binding = 2`)
* **Swapchain**: Triple-buffered mailbox/FIFO swapchain with automatic recreation on resize.

---

## 9. Renderer & RenderGraph (`engine/renderer`)
* **RenderGraph**: Directed Acyclic Graph (DAG) for pass scheduling, automatic memory aliasing, layout transition barriers, and execution.
* **Meshlets**: 64-vertex / 126-triangle cluster generation with cone-axis backface and frustum culling.
* **Lighting**:
  * Clustered Froxel Grid ($16 \times 9 \times 24$ frustum slices) for thousands of active dynamic lights.
  * 4-Cascade Directional Shadow Maps (CSM) with subpixel texel snapping.
  * Volumetric Fog injection and raymarching ($160 \times 90 \times 64$, `RGBA16F`).
  * Weighted Blended Order-Independent Transparency (WBOIT).
  * ReSTIR DI/GI spatio-temporal reservoir resampling.
* **Post-Processing & TAA**:
  * Tone Mapping: Fitted ACES and AgX color transforms.
  * Temporal Anti-Aliasing (TAA): 16-phase Halton(2,3) subpixel jitter with YCoCg neighborhood bounding-box clamping.
  * HDR Bloom: 6-stage downsample/upsample mip chain with 13-tap tent filter and Karis luma weighting.
  * Auto-Exposure: 256-bin log-luminance histogram with exponential eye adaptation.

---

## 10. Jolt Physics 3D (`engine/physics`)
* High-performance multi-threaded 3D simulation with broadphase layer filters (Non-Moving, Moving, Sensor).
* `RigidBodyComponent` (Static, Kinematic, Dynamic) and `ColliderComponent` (Box, Sphere, Capsule, Cylinder).
* Flecs ECS bidirectional synchronization (`sync_to_physics()` / `sync_from_physics()`).
* 3D world raycasting with hit point, normal, distance, and entity ID resolution.

---

## 11. Spatial Audio Subsystem (`engine/audio`)
* Low-latency audio backend powered by `miniaudio`.
* PIMPL isolation preventing header leakage.
* 5-bus hierarchy (`Master`, `SFX`, `Music`, `Voice`, `Ambient`).
* 3D distance attenuation, spatial panning, Doppler shift, and HRTF orientation.
* Automatic Flecs ECS sync for `AudioListenerComponent` and `AudioSourceComponent`.

---

## 12. Scripting Subsystem (`engine/scripting`)
* Embedded Lua 5.4 engine with `Sol2` high-speed C++ bindings.
* Full access to SIMD math (`Vec2`, `Vec3`, `Vec4`, `Quat`), Logging, Input, Audio, Physics, and Flecs Entities.
* Prototype metatable dispatch (`__index`) executing `on_init(self, entity)` and `on_update(self, entity, dt)`.

---

## 13. Editor UI & Gizmo Subsystem (`engine/ui`)
* ImGui 1.92+ docking layout with custom obsidian/slate dark theme.
* Native Vulkan 1.3 Dynamic Rendering ImGui backend (zero renderpass legacy objects).
* Panels:
  * **Scene Hierarchy Panel**: Live Flecs entity tree with safe deferred deletion.
  * **Entity Inspector Panel**: Dynamic component inspector for Transform, Physics, Audio, and Script fields.
  * **Content Browser Panel**: Interactive VFS file explorer.
  * **Profiler & Console Panel**: Live rolling FPS chart, hardware specs, and heap memory usage.
* **EditorCamera**: Orbit, pan, dolly, and FPS flycam controls.
* **ImGuizmo 3D Manipulator**: Translate, Rotate, Scale in World/Local space with grid snapping.
* **Viewport Mouse Picking**: Screen-to-world unprojected raycast entity selection.
