# Architecture, Isolation & Production Rules

## Strict Module Isolation & DAG
1. `engine/` is the core library. It MUST NEVER reference or depend on `editor/`, `runtime/`, or `sandbox/`.
2. `editor/` and `runtime/` are standalone applications that depend on `engine/`, but NEVER on each other.
3. Modules within `engine/` have strict acyclic DAG dependencies: `engine_core` -> `engine_rhi` / `engine_scene` -> `engine_renderer` -> `engine_master`.
4. NO circular dependencies between any modules.

## Dynamic Project Agnosticism
1. Game projects are completely decoupled and can reside anywhere on the filesystem.
2. NEVER hardcode project paths (e.g. `sandbox_project`, `projects/ActionDemo`) in engine, editor, or runtime code.
3. Projects are provided dynamically via `--project <path>` or `project.toml`.
4. All asset lookups route through dynamic VFS mounts (`/assets`, `/maps`, `/config`).

## Production-Grade Engineering Standards
1. Zero hardcoded colors, roughness, or name-matching heuristics (`name.find(...)`). Everything is driven by ECS data (`MaterialComponent`, `TransformComponent`, `RigidBodyComponent`).
2. Production mathematical models: Cook-Torrance GGX Specular BRDF, energy-conserving Lambertian diffuse, 4-split Cascaded Shadow Maps with texel snapping, 6-plane normalized Frustum Culling, ACES/AgX HDR tonemapping.
3. Every ECS component must have full serialization/deserialization support in `MapSerializer`.

## Ambiguity Policy
If any requirement, shader layout, or architectural boundary is ambiguous or underspecified, HALT IMMEDIATELY and ask the user.
