#include "math3d.h"

vec3 vec3_add(vec3 a, vec3 b) { return (vec3){a.x + b.x, a.y + b.y, a.z + b.z}; }
vec3 vec3_sub(vec3 a, vec3 b) { return (vec3){a.x - b.x, a.y - b.y, a.z - b.z}; }
vec3 vec3_mul(vec3 a, float s) { return (vec3){a.x * s, a.y * s, a.z * s}; }
float vec3_dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

vec3 vec3_cross(vec3 a, vec3 b) {
    return (vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float vec3_length(vec3 a) { return sqrtf(vec3_dot(a, a)); }

vec3 vec3_normalize(vec3 a) {
    float len = vec3_length(a);
    if (len == 0) return (vec3){0, 0, 0};
    return vec3_mul(a, 1.0f / len);
}

mat4 mat4_identity(void) {
    mat4 m = {0};
    m.m[0] = 1.0f; m.m[5] = 1.0f; m.m[10] = 1.0f; m.m[15] = 1.0f;
    return m;
}

mat4 mat4_translate(vec3 v) {
    mat4 m = mat4_identity();
    m.m[12] = v.x; m.m[13] = v.y; m.m[14] = v.z;
    return m;
}

mat4 mat4_perspective(float fov, float aspect, float near, float far) {
    mat4 m = {0};
    float tan_half_fov = tanf(fov / 2.0f);
    m.m[0] = 1.0f / (aspect * tan_half_fov);
    m.m[5] = 1.0f / tan_half_fov;
    m.m[10] = -far / (far - near);
    m.m[14] = -(far * near) / (far - near);
    m.m[11] = -1.0f;
    return m;
}

mat4 mat4_lookat(vec3 eye, vec3 center, vec3 up) {
    vec3 f = vec3_normalize(vec3_sub(center, eye));
    vec3 s = vec3_normalize(vec3_cross(f, up));
    vec3 u = vec3_cross(s, f);

    mat4 m = mat4_identity();
    m.m[0] = s.x;
    m.m[4] = s.y;
    m.m[8] = s.z;
    m.m[1] = u.x;
    m.m[5] = u.y;
    m.m[9] = u.z;
    m.m[2] = -f.x;
    m.m[6] = -f.y;
    m.m[10] = -f.z;
    m.m[12] = -vec3_dot(s, eye);
    m.m[13] = -vec3_dot(u, eye);
    m.m[14] = vec3_dot(f, eye);
    return m;
}

mat4 mat4_multiply(mat4 a, mat4 b) {
    mat4 res = {0};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                res.m[i + j * 4] += a.m[i + k * 4] * b.m[k + j * 4];
            }
        }
    }
    return res;
}

void frustum_extract(Frustum *f, mat4 vp) {
    // Left
    f->planes[0] = (Plane){vp.m[3] + vp.m[0], vp.m[7] + vp.m[4], vp.m[11] + vp.m[8], vp.m[15] + vp.m[12]};
    // Right
    f->planes[1] = (Plane){vp.m[3] - vp.m[0], vp.m[7] - vp.m[4], vp.m[11] - vp.m[8], vp.m[15] - vp.m[12]};
    // Bottom
    f->planes[2] = (Plane){vp.m[3] + vp.m[1], vp.m[7] + vp.m[5], vp.m[11] + vp.m[9], vp.m[15] + vp.m[13]};
    // Top
    f->planes[3] = (Plane){vp.m[3] - vp.m[1], vp.m[7] - vp.m[5], vp.m[11] - vp.m[9], vp.m[15] - vp.m[13]};
    // Near
    f->planes[4] = (Plane){vp.m[3] + vp.m[2], vp.m[7] + vp.m[6], vp.m[11] + vp.m[10], vp.m[15] + vp.m[14]};
    // Far
    f->planes[5] = (Plane){vp.m[3] - vp.m[2], vp.m[7] - vp.m[6], vp.m[11] - vp.m[10], vp.m[15] - vp.m[14]};

    for (int i = 0; i < 6; i++) {
        float len = sqrtf(f->planes[i].a * f->planes[i].a + f->planes[i].b * f->planes[i].b + f->planes[i].c * f->planes[i].c);
        f->planes[i].a /= len; f->planes[i].b /= len; f->planes[i].c /= len; f->planes[i].d /= len;
    }
}

bool frustum_intersects_box(const Frustum *f, vec3 min, vec3 max) {
    for (int i = 0; i < 6; i++) {
        vec3 p = {
            (f->planes[i].a > 0) ? max.x : min.x,
            (f->planes[i].b > 0) ? max.y : min.y,
            (f->planes[i].c > 0) ? max.z : min.z
        };
        if (f->planes[i].a * p.x + f->planes[i].b * p.y + f->planes[i].c * p.z + f->planes[i].d < 0) {
            return false;
        }
    }
    return true;
}
