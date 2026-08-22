# Project Overview
Custom high-performance, real-time graphics and game engine built from scratch in modern C++ (C++20/23).

## Core Architecture & Goals
- Low-Level Graphics Backend: Native Vulkan rendering pipeline with explicit GPU resource management, bindless descriptor indexing, dynamic rendering, and frame-graph/render-graph execution.
- Memory & Performance: Custom cache-friendly memory allocators (linear/arena, frame allocators, slab/pool, TLSF GPU heap management), data-oriented layout (SoA/DoD), and zero runtime heap allocations in hot render paths.
- Concurrency: Fiber/job-based task system with lock-free work-stealing queues for parallel command buffer generation and asset streaming.
- Build & Tooling: CMake / Ninja build matrix with strict Clang compiler warnings, ASan/TSan diagnostics, and Vulkan validation layers integration.
