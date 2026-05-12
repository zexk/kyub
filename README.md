# Kyub - Minimalist Voxel Engine

A lightweight C99 voxel engine with Vulkan renderer.

## Building

### Prerequisites

**Linux (Nix):**
```bash
nix develop          # Enter development shell
make                # Build debug
make release        # Build optimized
```

**Linux (Manual):**
- GCC or Clang
- X11 development headers
- Vulkan SDK (vulkan-loader, vulkan-headers)
- glslangValidator (from glslang) for SPIR-V compilation
- stb_image (place in include/stb/)

### Build Commands

```bash
make                 # Debug build (with -g, logger enabled)
make release         # Optimized build (-O2, -DNDEBUG)
make clean          # Clean build artifacts
```

### Environment Variables

| Variable | Values | Default | Effect |
|----------|--------|---------|--------|
| `KYUB_LOG` | `debug`, `info`, `warn`, `error` | `warn` | Set logging level |

```bash
KYUB_LOG=debug ./build/kyub   # Verbose debug output
./build/kyub                 # Only warnings and errors
```

### Running

```bash
./build/kyub
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
| T | Toggle pause menu |
| P | Toggle UI |
| 1-3 | Select block type |

## Architecture

- **Renderer** (`src/renderer/`) - Vulkan renderer and OpenGL fallback
- **World** (`src/world.c`) - Chunk-based terrain management
- **Mesh** (`src/mesh.c`) - Greedy meshing with ambient occlusion
- **Noise** (`src/noise.c`) - Perlin noise terrain generation
- **Persistence** (`src/world.c`) - Versioned per-chunk save files in `saves/default/`

## Project Structure

```
src/          - Engine source
include/      - Public headers
shaders/      - GLSL sources (.vert/.frag/.comp)
assets/       - Textures
saves/        - Local world saves (gitignored)
build/        - Compiled output
```
