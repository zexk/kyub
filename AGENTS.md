# AGENTS.md - Kyub Voxel Engine

## Build Commands

```bash
# Enter development shell (required for building)
nix develop

# Build inside dev shell
make

# Or build directly with Nix
nix build
```

The binary is at `./build/kyub`.

## Project Structure

- `src/` - Main engine (main.c, gl_ext.c, shader.c, voxel.c, mesh.c, world.c, etc.)
- `include/` - Headers
- `shaders/` - GLSL sources
- `assets/` - Textures (atlas.png)

## Development Conventions

- **C99 only**: Use `-std=c99` flag
- **Manual memory management**: Always call `mesh_free()` and `world_free()` after use
- **Custom GL loading**: Add new OpenGL functions to `include/gl_ext.h` and register in `src/gl_ext.c`

## Key Files

- `src/gl_ext.c` / `include/gl_ext.h` - Custom OpenGL function loader
- `src/world.c` - Chunk/voxel world management
- `src/voxel.c` - Voxel data structures
- `src/shader.c` - Shader compilation and management

## Running

```bash
./build/kyub
```