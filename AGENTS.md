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
- `assets/` - Textures (`assets/textures/*.png`)

## Renderer Backends

- **Vulkan** (default): `RENDERER=vulkan` - Modern, primary backend
- **OpenGL** (fallback): `RENDERER=opengl` - Legacy compatibility

## Development Conventions

- **C99 only**: Use `-std=c99` flag
- **Manual memory management**: Always call `mesh_free()` and `world_free()` after use
- **Separate shader files**: Vulkan uses `.vert`/`.frag`, OpenGL uses `.gl.vert`/`.gl.frag`

## Key Files

- `src/renderer/renderer_gl.c` - OpenGL rendering backend (fallback)
- `src/renderer/renderer_vulkan.c` - Vulkan rendering backend (default)
- `src/platform/platform_x11.c` - X11 platform layer
- `src/world.c` - Chunk/voxel world management
- `src/voxel.c` - Voxel data structures
- `src/mesh.c` - Mesh generation and management
- `src/ecs.c` - Entity Component System core
- `src/components.c` - Game component definitions
- `src/systems.c` - ECS systems (movement, etc.)
- `src/math3d.c` - Math utilities (mat4, vec3, frustum)

## Texture System

- **GL_TEXTURE_2D_ARRAY**: All block textures loaded into a single texture array (16×16 per texture)
- **Per-face layers**: Each block face (top/bottom/side) can use a different texture layer
- **Path deduplication**: Unique texture paths are collected at init, each gets one array layer
- **Texture location**: `assets/textures/*.png` (e.g., `dirt.png`, `grass_top.png`, `grass_side.png`)
- **Fallback**: Missing textures log a warning and render as black for that face

## World Persistence

- **Save location**: local saves are written under `saves/default/` and ignored by git
- **Chunk files**: one versioned file per chunk (`chunk_X_Z.kch`)
- **Compatibility**: chunk data uses a stable string block-ID palette (`kyub:stone`, etc.), not raw enum meanings
- **Format**: fixed magic/version header plus typed sections; unknown future sections can be skipped
- **Flush policy**: edited chunks save periodically, on unload, and during shutdown

## ECS Architecture

- Entity = `uint32_t` index into flat arrays
- Component pools = flat arrays indexed by entity ID (sparse, not packed)
- Up to 32 component types, 4096 max entities
- Global `ECS g_ecs` declared in `components.h`
- Block types are entities with `C_BlockDef` components

## Critical Bugs & Fixes

### Skybox Rendering
- **Problem**: Cube-based skybox caused black triangles (w-clipping) and seam discoloration at cube edges
- **Fix**: Fullscreen triangle approach with per-pixel ray reconstruction via inverse projection/view matrices
- **Shader**: `shaders/skybox.gl.vert` now uses `inv_projection` and `inv_view_rotation` uniforms
- **Geometry**: Single triangle covering NDC `{-1,-1}, {3,-1}, {-1,3}` instead of 36-vertex cube

### OpenGL Resource Leak
- **Problem**: VAO/buffer/texture handles only incremented, never reused; exhausted after 256 allocations causing invisible chunks
- **Fix**: Free lists (`g_vao_free_list[]`, `g_buffer_free_list[]`, `g_texture_free_list[]`) in create/destroy functions

### Stride=0 Bug
- **Problem**: `glVertexArrayVertexBuffer` with stride=0 is a no-op (doesn't bind buffer per GL 4.5 spec)
- **Fix**: All `renderer_attrib_pointer` calls use actual strides (12 for 3-float, 8 for 2-float)
- **Affected**: Skybox and outline VAOs

### Buffer Upload
- **Problem**: `mesh_upload()` used `renderer_buffer_sub_data()` on fresh VBOs with zero storage
- **Fix**: Use `renderer_buffer_data()` which allocates storage + uploads atomically

### UI State Corruption
- **Problem**: `renderer_push_attrib`/`renderer_pop_attrib` are no-ops in GL backend; UI rendering disabled depth/cull, enabled blend
- **Fix**: Explicit state restoration at frame start (enable depth_test, enable cull_face, disable blend)

### Transparent Window
- **Fix**: `GLX_ALPHA_SIZE = 0` in `platform_x11.c`; `_NET_WM_WINDOW_OPACITY` set before `XMapWindow`; `CWBackPixel` to black

### Mesa RADV Crash
- **GPU**: AMD RX 9060 XT (GFX1200 RDNA4)
- **Driver**: Mesa 26.0.6 crashes with SIGSEGV in `libvulkan_radeon.so` at `vkCreateGraphicsPipelines`
- **Workaround**: Use OpenGL backend (`make RENDERER=opengl`)

## Running

```bash
./build/kyub
```
