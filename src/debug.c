#include "debug.h"
#include "gl_ext.h"
#include <stdio.h>
#include <string.h>

static bool enabled = true;
static GLuint vao = 0, vbo = 0;

void debug_init(void) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
}

void debug_toggle(void) {
    enabled = !enabled;
}

bool debug_is_enabled(void) {
    return enabled;
}

static void draw_digit(float x, float y, float scale, int digit) {
    float s = scale;
    float pts[32];
    int count = 0;

    // Simple 7-segment display - fixed patterns
    // Format: top, topl, topr, mid, botl, botr, bot, right, left
    switch (digit) {
        case 0: // Box without middle
            pts[count++] = x;      pts[count++] = y+s;  pts[count++] = x+s; pts[count++] = y+s;
            pts[count++] = x+s;    pts[count++] = y+s;  pts[count++] = x+s; pts[count++] = y;
            pts[count++] = x+s;    pts[count++] = y;    pts[count++] = x;   pts[count++] = y;
            pts[count++] = x;      pts[count++] = y;    pts[count++] = x;   pts[count++] = y+s;
            break;
        case 1: // Right line
            pts[count++] = x+s;    pts[count++] = y+s;  pts[count++] = x+s; pts[count++] = y;
            break;
        case 2: // Top, top-right, mid, bot-left, bot
            pts[count++] = x;      pts[count++] = y+s;  pts[count++] = x+s; pts[count++] = y+s;
            pts[count++] = x+s;    pts[count++] = y+s;  pts[count++] = x+s; pts[count++] = y+s*0.5f;
            pts[count++] = x+s;    pts[count++] = y+s*0.5f; pts[count++] = x;   pts[count++] = y;
            pts[count++] = x;      pts[count++] = y;    pts[count++] = x+s; pts[count++] = y;
            break;
        case 3: // Top, top-right, mid, bot-right, bot
            pts[count++] = x;      pts[count++] = y+s;  pts[count++] = x+s; pts[count++] = y+s;
            pts[count++] = x+s;    pts[count++] = y+s;  pts[count++] = x+s; pts[count++] = y;
            pts[count++] = x+s;    pts[count++] = y+s*0.5f; pts[count++] = x;   pts[count++] = y+s*0.5f;
            pts[count++] = x;      pts[count++] = y;    pts[count++] = x+s; pts[count++] = y;
            break;
        case 4: // Top-left, top-right, mid, bot-right
            pts[count++] = x;      pts[count++] = y+s;  pts[count++] = x;   pts[count++] = y;
            pts[count++] = x;      pts[count++] = y+s*0.5f; pts[count++] = x+s; pts[count++] = y+s*0.5f;
            pts[count++] = x+s;    pts[count++] = y+s;  pts[count++] = x+s; pts[count++] = y;
            break;
        case 5: // Top, top-left, mid, bot-right, bot
            pts[count++] = x;      pts[count++] = y+s;  pts[count++] = x+s; pts[count++] = y+s;
            pts[count++] = x;      pts[count++] = y+s;  pts[count++] = x;   pts[count++] = y+s*0.5f;
            pts[count++] = x;      pts[count++] = y+s*0.5f; pts[count++] = x+s; pts[count++] = y+s*0.5f;
            pts[count++] = x+s;    pts[count++] = y+s*0.5f; pts[count++] = x+s; pts[count++] = y;
            pts[count++] = x+s;    pts[count++] = y;    pts[count++] = x;   pts[count++] = y;
            break;
        case 6: // Top, top-left, mid, bot-left, bot-right, bot
            pts[count++] = x;      pts[count++] = y+s;  pts[count++] = x+s; pts[count++] = y+s;
            pts[count++] = x;      pts[count++] = y+s;  pts[count++] = x;   pts[count++] = y;
            pts[count++] = x;      pts[count++] = y+s*0.5f; pts[count++] = x+s; pts[count++] = y+s*0.5f;
            pts[count++] = x+s;    pts[count++] = y+s*0.5f; pts[count++] = x+s; pts[count++] = y;
            pts[count++] = x+s;    pts[count++] = y;    pts[count++] = x;   pts[count++] = y;
            break;
        case 7: // Top, top-right, bot-right
            pts[count++] = x;      pts[count++] = y+s;  pts[count++] = x+s; pts[count++] = y+s;
            pts[count++] = x+s;    pts[count++] = y+s;  pts[count++] = x+s; pts[count++] = y;
            break;
        case 8: // Full box
            pts[count++] = x;      pts[count++] = y+s;  pts[count++] = x+s; pts[count++] = y+s;
            pts[count++] = x+s;    pts[count++] = y+s;  pts[count++] = x+s; pts[count++] = y;
            pts[count++] = x+s;    pts[count++] = y;    pts[count++] = x;   pts[count++] = y;
            pts[count++] = x;      pts[count++] = y;    pts[count++] = x;   pts[count++] = y+s;
            pts[count++] = x+s;    pts[count++] = y+s*0.5f; pts[count++] = x;   pts[count++] = y+s*0.5f;
            break;
        case 9: // Top, top-right, mid, bot-left, bot-right, bot
            pts[count++] = x;      pts[count++] = y+s;  pts[count++] = x+s; pts[count++] = y+s;
            pts[count++] = x+s;    pts[count++] = y+s;  pts[count++] = x+s; pts[count++] = y;
            pts[count++] = x;      pts[count++] = y+s*0.5f; pts[count++] = x+s; pts[count++] = y+s*0.5f;
            pts[count++] = x+s;    pts[count++] = y+s*0.5f; pts[count++] = x+s; pts[count++] = y;
            pts[count++] = x+s;    pts[count++] = y;    pts[count++] = x;   pts[count++] = y;
            break;
    }

    glBufferData(GL_ARRAY_BUFFER, count * sizeof(float), pts, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_LINES, 0, count / 2);
}

static void draw_string(float x, float y, float scale, const char *str) {
    int len = strlen(str);
    for (int i = 0; i < len; i++) {
        char c = str[i];
        if (c >= '0' && c <= '9') {
            draw_digit(x, y, scale, c - '0');
        } else if (c == '.') {
            float pts[] = { x, y, x + scale*0.3f, y };
            glBufferData(GL_ARRAY_BUFFER, sizeof(pts), pts, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
            glEnableVertexAttribArray(0);
            glDrawArrays(GL_LINES, 0, 2);
        }
        x += scale * 0.8f;
    }
}

void debug_render(unsigned int shader_program, float fps, int chunks, float cam_x, float cam_y, float cam_z, float yaw, float pitch, int look_x, int look_y, int look_z) {
    if (!enabled) return;

    glDisable(GL_DEPTH_TEST);
    glUseProgram(shader_program);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // Draw background panel - single 6-vertex draw
    float panel[] = { 
        -0.7f, -0.3f,  // v0
        -0.3f, -0.3f,  // v1
        -0.7f, -0.7f,  // v2
        -0.3f, -0.3f,  // v1 (repeat for tri 2)
        -0.3f, -0.7f,  // v3
        -0.7f, -0.7f   // v2 (repeat)
    };
    glDepthMask(GL_FALSE);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(panel), panel, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);
    glUniform3f(glGetUniformLocation(shader_program, "uColor"), 0.3f, 0.3f, 0.3f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Draw text
    glUniform3f(glGetUniformLocation(shader_program, "uColor"), 1.0f, 1.0f, 1.0f);

    float px = -0.68f;
    float py = -0.55f;
    float scale = 0.1f;

    // FPS
    char fps_str[16];
    snprintf(fps_str, sizeof(fps_str), "%.1f", fps);
    draw_string(px, py, scale, fps_str);

    // Chunks
    py -= scale * 1.2f;
    char chunk_str[16];
    snprintf(chunk_str, sizeof(chunk_str), "%d", chunks);
    draw_string(px, py, scale, chunk_str);

    // Camera position
    py -= scale * 1.2f;
    char pos_str[32];
    snprintf(pos_str, sizeof(pos_str), "%d%d%d", (int)cam_x, (int)cam_y, (int)cam_z);
    draw_string(px, py, scale, pos_str);

    // Camera direction
    py -= scale * 1.2f;
    char dir_str[32];
    snprintf(dir_str, sizeof(dir_str), "%d%d", (int)yaw, (int)pitch);
    draw_string(px, py, scale, dir_str);

    // Looking at
    py -= scale * 1.2f;
    char look_str[32];
    snprintf(look_str, sizeof(look_str), "%d%d%d", look_x, look_y, look_z);
    draw_string(px, py, scale, look_str);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}