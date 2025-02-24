#ifndef Q_MATHLIB_H
#define Q_MATHLIB_H

#include <cmath>
#include <cassert>

//---------------------------------------------------------------------
// If the project already has a vector type (for example, from q_vec3.h),
// make sure that header defines VEC3_T_DEFINED. Otherwise, we define it here.
//---------------------------------------------------------------------
#ifndef VEC3_T_DEFINED
typedef float vec3_t[3];
#define VEC3_T_DEFINED
#endif

//---------------------------------------------------------------------
// QM_VectorCopy: Copies vector 'in' into vector 'out'.
//---------------------------------------------------------------------
inline void QM_VectorCopy(const vec3_t in, vec3_t out) {
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
}

//---------------------------------------------------------------------
// QM_VectorSubtract: out = a - b
//---------------------------------------------------------------------
inline void QM_VectorSubtract(const vec3_t a, const vec3_t b, vec3_t out) {
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

//---------------------------------------------------------------------
// QM_VectorAdd: out = a + b
//---------------------------------------------------------------------
inline void QM_VectorAdd(const vec3_t a, const vec3_t b, vec3_t out) {
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

//---------------------------------------------------------------------
// QM_VectorMA: Multiply-and-add: out = start + (scale * direction)
//---------------------------------------------------------------------
inline void QM_VectorMA(const vec3_t start, float scale, const vec3_t direction, vec3_t out) {
    out[0] = start[0] + scale * direction[0];
    out[1] = start[1] + scale * direction[1];
    out[2] = start[2] + scale * direction[2];
}

//---------------------------------------------------------------------
// QM_VectorLength: Returns the Euclidean length of vector 'v'
//---------------------------------------------------------------------
inline float QM_VectorLength(const vec3_t v) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

//---------------------------------------------------------------------
// QM_VectorNormalize: Normalizes vector 'v' in-place.
// If the vector is near zero length, defaults to (1,0,0).
//---------------------------------------------------------------------
inline void QM_VectorNormalize(vec3_t v) {
    float len = QM_VectorLength(v);
    if (len > 1e-6f) {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    } else {
        // Prevent division by zero: default to unit vector along X.
        v[0] = 1.0f; 
        v[1] = 0.0f; 
        v[2] = 0.0f;
    }
}

//---------------------------------------------------------------------
// QM_VectorScale: Scales vector 'in' by 'scale' and stores the result in 'out'.
//---------------------------------------------------------------------
inline void QM_VectorScale(const vec3_t in, float scale, vec3_t out) {
    out[0] = in[0] * scale;
    out[1] = in[1] * scale;
    out[2] = in[2] * scale;
}

//---------------------------------------------------------------------
// QM_DotProduct: Returns the dot product of vectors 'a' and 'b'
//---------------------------------------------------------------------
inline float QM_DotProduct(const vec3_t a, const vec3_t b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

//---------------------------------------------------------------------
// QM_CrossProduct: Computes the cross product (a x b) and stores the result in 'out'
//---------------------------------------------------------------------
inline void QM_CrossProduct(const vec3_t a, const vec3_t b, vec3_t out) {
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

//---------------------------------------------------------------------
// QM_AngleVectors: Converts an angle (in degrees) vector (pitch, yaw, roll)
// into forward, right, and up vectors.
// If any of the output pointers is null, that component is skipped.
//---------------------------------------------------------------------
inline void QM_AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up) {
    constexpr float DEG2RAD = 3.14159265358979323846f / 180.0f;
    float pitch = angles[0] * DEG2RAD;
    float yaw   = angles[1] * DEG2RAD;
    float roll  = angles[2] * DEG2RAD;

    float sp = std::sin(pitch);
    float cp = std::cos(pitch);
    float sy = std::sin(yaw);
    float cy = std::cos(yaw);
    float sr = std::sin(roll);
    float cr = std::cos(roll);

    if (forward) {
        forward[0] = cp * cy;
        forward[1] = cp * sy;
        forward[2] = -sp;
    }
    if (right) {
        right[0] = -sr * sp * cy - cr * -sy;
        right[1] = -sr * sp * sy - cr * cy;
        right[2] = -sr * cp;
    }
    if (up) {
        up[0] = cr * sp * cy - sr * -sy;
        up[1] = cr * sp * sy - sr * cy;
        up[2] = cr * cp;
    }
}

#endif // Q_MATHLIB_H
