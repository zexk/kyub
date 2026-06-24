# AGENTS.md

Agent context for the Kyub voxel game.

Kyub is a C99 voxel game built on the [Kiln](https://github.com/zexk/kiln) engine.
It uses Kiln's low-level Vulkan renderer (`kiln_renderer`, the `r_*.c` wrappers),
Kiln's archetype ECS, platform layer (X11 / Win32), camera, physics, and UI.
Kiln is consumed as an external dependency — there is no vendored copy.

## Build

```sh
nix develop                 # dev shell: cmake, ninja, glslc, Vulkan, mingw cross
cmake -B build -G Ninja     # KILN_DIR is injected from the flake input
ninja -C build              # binary at ./build/kyub
```

`KILN_DIR` must point at a Kiln checkout. The flake passes
`-DKILN_DIR=${kiln}` automatically; the dev shell prints the exact command.
Non-Nix users clone Kiln separately and set `-DKILN_DIR=/path/to/kiln` (or the
`KILN_DIR` environment variable) — CMake errors with instructions if it's unset.

Via Nix directly:

```sh
nix build            # native Linux/x86_64
nix build .#win32    # Windows cross-build (mingwW64), runnable under Wine
```

## Run

```sh
./build/kyub
KILN_LOG=DEBUG ./build/kyub   # log levels: DEBUG | INFO | WARN | ERROR
```

## Controls

| Input | Action |
|-------|--------|
| WASD | Move |
| Shift | Sprint |
| Space | Jump |
| Mouse | Look |
| Left click | Break block |
| Right click | Place block |
| Scroll | Cycle hotbar block |
| Esc / T | Pause menu (toggles cursor) |
| F3 | Debug overlay (fps, pos, chunk count) |

## Layout

```
src/         game code
shaders/     GLSL sources, compiled to SPIR-V via glslc at build time
assets/      block textures (16x16 RGBA PNG, one per face)
saves/       world saves (gitignored)
```

Headers live next to their `.c`.

| File | Responsibility |
|------|----------------|
| `src/main.c` | entry point, game loop, input, rendering, HUD/pause UI |
| `src/voxel.c` | chunk terrain generation (biomes, caves, sea-level water) |
| `src/inventory.c` | hotbar + inventory UI, slot management |
| `src/components.c` | ECS component registration, block-type registry |
| `src/systems.c` | ECS systems (`sys_movement`: integrate + resolve collisions) |

Block textures, HUD primitives, and block picking come from Kiln, not local
code: `kiln_texture` (deduplicating texture-array loader, `texture_array_*`),
`kiln_ui` (rects/text/buttons via `ui_gl_t`), and `phys_raycast_voxel`
(DDA block raycast in `kiln_physics`).

## Renderer

Kyub links `kiln_renderer` — Kiln's **low-level** Vulkan renderer, not the
high-level scene renderer (`kiln_render`).
Shaders: `hud`, `skybox` (fullscreen-triangle), `outline` (block highlight),
`inv` (inventory overlay); chunk geometry uses kiln_voxel's own `voxel.vert/frag`.
Add a new shader by extending the `foreach` list in `CMakeLists.txt`.

`renderer_save_screenshot(path)` writes the next presented frame to a binary
PPM — useful for visual verification.

## Voxels

- `Chunk`: `uint8_t blocks[16][16][16]` (single vertical layer, world is 16 tall),
  indexed `[x][y][z]`. `BlockType` enum in `voxel.h`.
- Block types are ECS entities carrying a `C_BlockDef` (id, textures per face,
  solid/opaque/hardness). `g_block_entities[BlockType]` maps enum → entity.
- Meshing: each chunk holds 3 LOD meshes (steps 1/2/4); the draw loop picks one
  by distance and frustum-culls via the chunk AABB. Per-face culling, not greedy
  merging.

## Textures

All block textures load into one Vulkan texture array (16x16 per layer). Unique
texture paths are deduplicated into array layers at startup; each `C_BlockDef`
resolves `layer_top/bottom/side/default`. A face samples its layer via the
`texture_layer` vertex attribute.

## World persistence

Chunks save under `saves/default/` as one versioned binary file per chunk
(`chunk_X_Z.kch`), using a stable string block-ID palette (`kyub:stone`, …) so
the enum can change without breaking saves. Edited chunks flush periodically, on
unload, and at shutdown.

## Conventions

- C99, manual memory management — pair `mesh_init`/`mesh_free`, `world_init`/`world_free`.
- Coordinates: `world_*` take world-space block coords; chunk-local indexing is
  `[x][y][z]`.
