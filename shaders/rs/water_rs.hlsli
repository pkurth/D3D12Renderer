#ifndef WATER_RS_HLSLI
#define WATER_RS_HLSLI

#include "transform.hlsli"

struct water_cb
{
    vec4 deepColor;
    vec4 shallowColor;
    vec2 uvOffset;
    float shallowDepth;
    float transitionStrength;
    float normalmapStrength;
};


#define WATER_RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
    "RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX), " \
    "RootConstants(num32BitConstants=16, b0, space=1, visibility=SHADER_VISIBILITY_PIXEL), " \
    "CBV(b1, space=1, visibility=SHADER_VISIBILITY_PIXEL), " \
    "CBV(b2, space=1, visibility=SHADER_VISIBILITY_PIXEL), " \
    "DescriptorTable(SRV(t0, numDescriptors=3), visibility=SHADER_VISIBILITY_PIXEL)," \
    "DescriptorTable(SRV(t0, space=2, numDescriptors=7), visibility=SHADER_VISIBILITY_PIXEL), " \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR," \
        "visibility=SHADER_VISIBILITY_PIXEL), " \
    "StaticSampler(s1," \
        "addressU = TEXTURE_ADDRESS_WRAP," \
        "addressV = TEXTURE_ADDRESS_WRAP," \
        "addressW = TEXTURE_ADDRESS_WRAP," \
        "filter = FILTER_ANISOTROPIC," \
        "visibility=SHADER_VISIBILITY_PIXEL), " \
    "StaticSampler(s2," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT," \
        "visibility=SHADER_VISIBILITY_PIXEL)"

#define WATER_RS_TRANSFORM          0
#define WATER_RS_SETTINGS           1
#define WATER_RS_CAMERA             2
#define WATER_RS_LIGHTING           3
#define WATER_RS_TEXTURES           4
#define WATER_RS_FRAME_CONSTANTS    5

#endif

