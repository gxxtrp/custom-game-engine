# Building and Running the Engine

This guide explains how to configure, build, and run the engine runtime and its associated tools.

---

## 1. Prerequisites & Toolchain

* **Compiler**: LLVM `clang-cl` (with Visual Studio Build Tools 2022+ / Windows 10/11 SDK).
* **Build System**: CMake 3.25+ with Ninja.
* **Package Manager**: vcpkg (with `VCPKG_ROOT` configured or using `C:/vcpkg`).
* **Vulkan SDK**: Vulkan 1.3+ GPU driver support.

All external libraries are managed automatically via `vcpkg.json`:
* `sdl3[core,vulkan]`
* `vulkan-headers`, `vulkan-loader`, `vulkan-memory-allocator`
* `flecs` (4.1.6)
* `joltphysics` (5.6.0)
* `miniaudio` (0.11.25)
* `lua` (5.5.1), `sol2` (3.5.0)
* `tomlplusplus` (3.4.0)
* `cgltf` (1.15)

---

## 2. Configuration & Building

### 2.1. Debug Build (`x64-clang-debug`)
Includes validation layers, assertions, and full debug symbols:
```powershell
# Configure
cmake -B build/x64-clang-debug -S . --preset x64-clang-debug

# Build all targets (engine_master, pak_builder, runtime)
cmake --build --preset x64-clang-debug
```

### 2.2. Release Build (`x64-clang-release`)
Fully optimized with AVX2 instruction acceleration:
```powershell
# Configure
cmake -B build/x64-clang-release -S . --preset x64-clang-release

# Build all targets
cmake --build --preset x64-clang-release
```

---

## 3. Running Binaries

### 3.1. Running the Standalone Game Runtime (`runtime.exe`)
```powershell
# Run standalone game project
.\build\x64-clang-release\runtime\runtime.exe --project <path_to_project_directory>

# Run in headless mode (no window created; ideal for CI and automated tests)
.\build\x64-clang-release\runtime\runtime.exe --project <path_to_project_directory> --headless --timeout 2.0
```

### 3.2. Running the PAK Builder Tool
```powershell
# Pack an asset directory into a binary .pak archive
.\build\x64-clang-release\tools\pak_builder\pak_builder.exe --input <project_path>/assets --output game_data.pak
```
