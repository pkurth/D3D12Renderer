#ifndef TRANSFORM_HLSLI
#define TRANSFORM_HLSLI

// Attention: Even though transform.m is declared as a 3x4 matrix, due to padding it is actually a 4x4 matrix (each column is padded to a float4). Therefore it takes 16 floats of space.
// On the CPU side we act as if 'm' is a mat4. My hope is that a 3x4 matrix uses fewer instructions than a 4x4 matrix, when multiplied with a vector. This remains to be tested though.

#ifndef HLSL
#define mat3x4 mat4
#endif

struct transform_cb
{
    mat4 mvp;
    mat3x4 m;
};

#endif
