#ifndef MATH3D_H
#define MATH3D_H

#include <math.h>
#include <stdbool.h>

#define PI 3.14159265358979323846f

typedef struct {
    float x, y, z;
} vec3;

typedef struct {
    float m[16]; // Column-major
} mat4;

// Vector operations
vec3 vec3_add(vec3 a, vec3 b);
vec3 vec3_sub(vec3 a, vec3 b);
vec3 vec3_mul(vec3 a, float s);
float vec3_dot(vec3 a, vec3 b);
vec3 vec3_cross(vec3 a, vec3 b);
float vec3_length(vec3 a);
vec3 vec3_normalize(vec3 a);

// Matrix operations
mat4 mat4_identity(void);
mat4 mat4_translate(vec3 v);
mat4 mat4_perspective(float fov, float aspect, float near, float far);
mat4 mat4_lookat(vec3 eye, vec3 center, vec3 up);
mat4 mat4_multiply(mat4 a, mat4 b);
mat4 mat4_transpose(mat4 m);
mat4 mat4_inverse(mat4 m);

typedef struct {
    float a, b, c, d; // ax + by + cz + d = 0
} Plane;

typedef struct {
    Plane planes[6];
} Frustum;

// Math helpers
void frustum_extract(Frustum *f, mat4 vp);
bool frustum_intersects_box(const Frustum *f, vec3 min, vec3 max);

#endif // MATH3D_H
