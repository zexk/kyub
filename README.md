# Kyub - Minimalist Voxel Engine

C99 voxel engine with Vulkan + OpenGL backends.

## Building

```bash
nix develop          # Enter dev shell
make                 # Debug build (Vulkan)
make RENDERER=opengl # Debug build (OpenGL)
make release         # Optimized build
```

### Run

```bash
KYUB_LOG=debug ./build/kyub  # With debug logging
./build/kyub                 # Normal
```

## Controls

| Key | Action |
|-----|--------|
| W/A/S/D | Move |
| Space | Jump |
| Shift | Sprint |
| Mouse | Look |
| Left Click | Break block |
| Right Click | Place block |
| Mousewheel | Cycle block type |
| T / Escape | Toggle pause/exit |
| P | (removed, Nuklear GUI deleted) |

## Architecture

- **Renderer** (`src/renderer/`) - Vulkan + OpenGL 4.5 fallback (DSA)
- **ECS** (`src/ecs.c`) - Flat-array entity-component system
- **World** (`src/world.c`) - Chunk-based terrain with persistence
- **Mesh** (`src/mesh.c`) - Greedy meshing with ambient occlusion
- **Texture array** (`GL_TEXTURE_2D_ARRAY`) - 16x16 per-block PNGs, per-face layers
- **Noise** (`src/noise.c`) - Perlin noise terrain generation
- **Hotbar** - 7-slot block selector at bottom of screen

## Project Structure

```
src/          - Engine source
include/      - Headers
shaders/      - GLSL (.gl.vert/.gl.frag for OpenGL)
assets/       - Block textures (assets/textures/*.png)
saves/        - World saves (gitignored)
build/        - Compiled output
```
