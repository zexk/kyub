cmake_minimum_required(VERSION 3.20)
project(kyub C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# ── Kiln engine ──────────────────────────────────────────────────────────────
add_subdirectory(extern/kiln)

# ── Shader compilation ───────────────────────────────────────────────────────
find_program(GLSLC glslc REQUIRED)
set(shader_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(shader_out_dir ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${shader_out_dir})

set(spv_files "")
foreach(shader
    basic.vert   basic.frag
    hud.vert     hud.frag
    skybox.vert  skybox.frag
    outline.vert outline.frag)
    add_custom_command(
        OUTPUT  ${shader_out_dir}/${shader}.spv
        COMMAND ${GLSLC} ${shader_src_dir}/${shader}
                         -o ${shader_out_dir}/${shader}.spv
        DEPENDS ${shader_src_dir}/${shader}
        COMMENT "glslc ${shader}")
    list(APPEND spv_files ${shader_out_dir}/${shader}.spv)
endforeach()
add_custom_target(kyub_shaders DEPENDS ${spv_files})

# ── Game executable ──────────────────────────────────────────────────────────
add_executable(kyub
    src/main.c
    src/voxel.c
    src/mesh.c
    src/components.c
    src/world.c
    src/texture.c
    src/gui.c
    src/systems.c)

add_dependencies(kyub kyub_shaders)

add_custom_command(TARGET kyub POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        ${CMAKE_CURRENT_BINARY_DIR}/assets)

target_include_directories(kyub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/kiln/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/kiln/src/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/kiln/src/render
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/kiln/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/kiln/src/ecs
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/kiln/src/camera
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/kiln/src/physics)

target_link_libraries(kyub PRIVATE
    kiln_renderer
    kiln_ecs
    kiln_camera
    kiln_platform
    kiln_physics
    kiln_core
    kiln_ui)
