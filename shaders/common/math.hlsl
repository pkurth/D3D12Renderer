#ifndef MATH_H
#define MATH_H

static const float pi = 3.14159265359f;
static const float invPI = 0.31830988618379067153776752674503f;
static const float inv2PI = 0.15915494309189533576888376337251f;
static const float2 invAtan = float2(inv2PI, invPI);


static float inverseLerp(float l, float u, float v) { return (v - l) / (u - l); }
static float remap(float v, float oldL, float oldU, float newL, float newU) { return lerp(newL, newU, inverseLerp(oldL, oldU, v)); }

#endif
