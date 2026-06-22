# Kyub

C99 voxel game. Vulkan renderer via [Kiln](https://github.com/zexk/kiln).

Chunk-based world with greedy meshing, ambient occlusion, biome terrain, cave carving, sea-level water, and LOD (3 levels). Chunks persist to disk in a compact binary format.

## Build

```sh
nix develop
cmake -B build -G Ninja && ninja -C build
```

Without Nix (Linux, X11, Vulkan SDK required):

```sh
cmake -B build -G Ninja -DKILN_DIR=/path/to/kiln
ninja -C build
```

`KILN_DIR` defaults to `extern/kiln` (the git submodule).

## Run

```sh
./build/kyub
KILN_LOG=debug ./build/kyub   # with logging
```

## Controls

| Key | Action |
|-----|--------|
| WASD | Move |
| Space | Jump |
| Mouse | Look |
| Left click | Break block |
| Right click | Place block |
| Scroll | Cycle block type |
| Escape | Pause / exit |

## Structure

```
src/          game source (mesh, world, voxel, ECS components, GUI, systems)
shaders/      GLSL sources, compiled to SPIR-V at build time
assets/       block textures (16x16 PNG per face)
saves/        world saves (gitignored)
extern/kiln/  Kiln engine (git submodule)
```

## License

MIT.
