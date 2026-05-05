# Project: kyub (Voxel Engine)

`kyub` is a minimalist 3D voxel engine written in C99, designed from scratch with a manual X11/GLX platform layer and custom OpenGL function loading.

## Project Overview
*   **Language**: C99
*   **Platform Layer**: X11 (XLib)
*   **Graphics**: OpenGL (GLX)
*   **Build System**: Makefile + Nix Flakes
*   **Architecture**:
    *   `src/`: Main engine logic (main loop, platform code, graphics pipeline).
    *   `include/`: Headers for data structures and utilities.
    *   `shaders/`: GLSL source files.
    *   `assets/`: Texture atlases (e.g., `atlas.png`).

## Building & Running
The project uses Nix for dependency management and build reproducibility.

### Development Environment
To enter the development shell (includes compilers, debuggers, and required libraries):
```bash
nix develop
```

### Build
To build the project:
```bash
nix build
```
Or use the Makefile directly inside the dev shell:
```bash
make
```

### Running
Once built, the binary is located at `./build/kyub`.
```bash
./build/kyub
```

## Development Conventions
*   **C99 Standard**: Code strictly adheres to C99 (`-std=c99`).
*   **Memory Management**: All voxel data, chunks, and meshes are manually allocated and managed. Ensure all `mesh_free()` and `world_free()` calls are correctly placed.
*   **Graphics**: Custom OpenGL loader is used (`gl_ext.c`). Add new GL function pointers to `include/gl_ext.h` and register them in `src/gl_ext.c` if needed.
*   **Contributions**: All changes should be verified via `nix build` before committing.
