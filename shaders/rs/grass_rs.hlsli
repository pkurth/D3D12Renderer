#ifndef GRASS_RS_HLSLI
#define GRASS_RS_HLSLI

#include "indirect.hlsli"

struct grass_blade
{
    vec3 position;
    uint32 type;
    vec2 facing;
};

struct grass_draw
{
    D3D12_DRAW_ARGUMENTS draw;
};


struct grass_cb
{
    uint32 numVertices;
    float halfWidth;
    float height;
    float lod; // [0, 1]

    vec3 windDirection;
    float time;
};

#define GRASS_RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
    "DENY_PIXEL_SHADER_ROOT_ACCESS)," \
    "RootConstants(b0, num32BitConstants=8, visibility=SHADER_VISIBILITY_VERTEX), " \
    "CBV(b1), " \
    "SRV(t0)"

#define GRASS_RS_CB         0
#define GRASS_RS_CAMERA     1
#define GRASS_RS_BLADES     2





struct grass_generation_cb
{
    vec3 chunkCorner;
    float chunkSize;

    float uvScale;
    float amplitudeScale;
};

#define GRASS_GENERATION_RS \
	"RootConstants(num32BitConstants=6, b0), " \
	"DescriptorTable(SRV(t0, numDescriptors=2), UAV(u0, numDescriptors=2)), " \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"

#define GRASS_GENERATION_RS_CB        0
#define GRASS_GENERATION_RS_RESOURCES 1




#endif
