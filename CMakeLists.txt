cmake_minimum_required(VERSION 3.20)
project(kyub C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# ── Kiln engine ──────────────────────────────────────────────────────────────
if(NOT DEFINED KILN_DIR AND DEFINED ENV{KILN_DIR})
  set(KILN_DIR "$ENV{KILN_DIR}")
endif()
if(NOT DEFINED KILN_DIR)
  message(FATAL_ERROR
    "KILN_DIR is not set. Clone kiln from https://github.com/zexk/kiln and "
    "configure with -DKILN_DIR=/path/to/kiln (Nix users get this automatically "
    "via the flake input).")
endif()
add_subdirectory(${KILN_DIR} ${CMAKE_CURRENT_BINARY_DIR}/kiln)

# ── Shader compilation ───────────────────────────────────────────────────────
find_program(GLSLC glslc REQUIRED)
set(shader_src_dir  ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(shader_out_dir  ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(kv_shader_src   ${KILN_DIR}/src/voxel/shaders)
file(MAKE_DIRECTORY ${shader_out_dir})

set(spv_files "")

# Game shaders (in kyub/shaders/)
foreach(shader
    hud.vert     hud.frag
    skybox.vert  skybox.frag
    outline.vert outline.frag
    inv.vert     inv.frag)
    add_custom_command(
        OUTPUT  ${shader_out_dir}/${shader}.spv
        COMMAND ${GLSLC} ${shader_src_dir}/${shader} -o ${shader_out_dir}/${shader}.spv
        DEPENDS ${shader_src_dir}/${shader}
        COMMENT "glslc ${shader}")
    list(APPEND spv_files ${shader_out_dir}/${shader}.spv)
endforeach()

# kiln-voxel shaders (sourced from kiln)
foreach(shader voxel.vert voxel.frag)
    add_custom_command(
        OUTPUT  ${shader_out_dir}/${shader}.spv
        COMMAND ${GLSLC} ${kv_shader_src}/${shader} -o ${shader_out_dir}/${shader}.spv
        DEPENDS ${kv_shader_src}/${shader}
        COMMENT "glslc (kiln-voxel) ${shader}")
    list(APPEND spv_files ${shader_out_dir}/${shader}.spv)
endforeach()

add_custom_target(kyub_shaders DEPENDS ${spv_files})

# ── Game executable ──────────────────────────────────────────────────────────
add_executable(kyub
    src/main.c
    src/voxel.c
    src/inventory.c
    src/components.c
    src/systems.c)

add_dependencies(kyub kyub_shaders)

add_custom_command(TARGET kyub POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        ${CMAKE_CURRENT_BINARY_DIR}/assets)

target_include_directories(kyub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${KILN_DIR}/src/core
    ${KILN_DIR}/src/platform
    ${KILN_DIR}/src/render
    ${KILN_DIR}/src/ui
    ${KILN_DIR}/src/texture
    ${KILN_DIR}/src/ecs
    ${KILN_DIR}/src/camera
    ${KILN_DIR}/src/physics
    ${KILN_DIR}/src/voxel)

target_link_libraries(kyub PRIVATE
    kiln_renderer
    kiln_ecs
    kiln_camera
    kiln_platform
    kiln_physics
    kiln_core
    kiln_ui
    kiln_texture
    kiln_voxel)
