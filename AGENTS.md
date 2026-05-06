# AGENTS.md - Kyub Voxel Engine

## Build Commands

```bash
# Enter development shell (required for building)
nix develop

# Build inside dev shell (Vulkan is default)
make

# Build with OpenGL fallback
make RENDERER=opengl

# Or build directly with Nix
nix build
```

The binary is at `./build/kyub`.

## Project Structure

- `src/` - Main engine (main.c, renderer_vulkan.c/renderer_gl.c, voxel.c, mesh.c, world.c, etc.)
- `include/` - Headers
- `shaders/` - GLSL sources (separate .vert/.frag for Vulkan, .gl.vert/.gl.frag for OpenGL)
- `assets/` - Textures (atlas.png)

## Renderer Backends

- **Vulkan** (default): `RENDERER=vulkan` - Modern, primary backend
- **OpenGL** (fallback): `RENDERER=opengl` - Legacy compatibility

## Development Conventions

- **C99 only**: Use `-std=c99` flag
- **Manual memory management**: Always call `mesh_free()` and `world_free()` after use
- **Separate shader files**: Vulkan uses `.vert`/`.frag`, OpenGL uses `.gl.vert`/`.gl.frag`

## Key Files

- `src/renderer_vulkan.c` - Vulkan rendering backend (default)
- `src/renderer_gl.c` - OpenGL rendering backend (fallback)
- `src/platform_x11.c` - X11 platform layer
- `src/world.c` - Chunk/voxel world management
- `src/voxel.c` - Voxel data structures
- `src/mesh.c` - Mesh generation and management

## Running

```bash
./build/kyub
```