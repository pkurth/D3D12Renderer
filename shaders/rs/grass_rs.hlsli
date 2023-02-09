#ifndef GRASS_RS_HLSLI
#define GRASS_RS_HLSLI

#include "indirect.hlsli"

struct grass_blade
{
    vec3 position;
    uint32 misc; // Type, lod
    vec2 facing; // sin(angle), cos(angle)

#ifdef HLSL
    void initialize(float3 position_, float angle, uint type, float lod)
    {
        position = position_;
        sincos(angle, facing.x, facing.y);
        misc = (type << 16) | f32tof16(lod);
    }

    uint type()
    {
        return misc >> 16;
    }

    float lod()
    {
        return f16tof32(misc & 0xFFFF);
    }
#endif
};

struct grass_draw
{
    D3D12_DRAW_ARGUMENTS draw;
};


struct grass_cb
{
    vec2 windDirection;
    float time;

    uint32 numVertices;
    float halfWidth;
    float height;
};

#define GRASS_RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
    "RootConstants(num32BitConstants=6, b0, visibility=SHADER_VISIBILITY_VERTEX), " \
    "SRV(t0, visibility=SHADER_VISIBILITY_VERTEX), " \
    "CBV(b1), " \
    "CBV(b2, visibility=SHADER_VISIBILITY_PIXEL)"

#define GRASS_RS_CB         0
#define GRASS_RS_BLADES     1
#define GRASS_RS_CAMERA     2
#define GRASS_RS_LIGHTING   3


#define GRASS_DEPTH_ONLY_RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
    "RootConstants(num32BitConstants=6, b0, visibility=SHADER_VISIBILITY_VERTEX), " \
    "SRV(t0, visibility=SHADER_VISIBILITY_VERTEX), " \
    "CBV(b1, visibility=SHADER_VISIBILITY_VERTEX), " \
    "RootConstants(num32BitConstants=1, space=1, b0, visibility=SHADER_VISIBILITY_PIXEL), " \
    "RootConstants(num32BitConstants=4, space=1, b1, visibility=SHADER_VISIBILITY_PIXEL)"

#define GRASS_DEPTH_ONLY_RS_CB              0
#define GRASS_DEPTH_ONLY_RS_BLADES          1
#define GRASS_DEPTH_ONLY_RS_CAMERA          2
#define GRASS_DEPTH_ONLY_RS_OBJECT_ID       3
#define GRASS_DEPTH_ONLY_RS_CAMERA_JITTER   4



#define GRASS_GENERATION_BLOCK_SIZE 8


struct grass_generation_common_cb
{
    vec4 frustumPlanes[6];
    vec3 cameraPosition;
    float chunkSize;
    float amplitudeScale;
    float uvScale;

    float lodChangeStartDistance;
    float lodChangeEndDistance;
};

struct grass_generation_cb
{
    vec3 chunkCorner;
    uint32 lodIndex;
};

#define GRASS_GENERATION_RS \
	"RootConstants(num32BitConstants=4, b0), " \
	"CBV(b1), " \
	"DescriptorTable(SRV(t0, numDescriptors=2), UAV(u0, numDescriptors=3)), " \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"

#define GRASS_GENERATION_RS_CB        0
#define GRASS_GENERATION_RS_COMMON    1
#define GRASS_GENERATION_RS_RESOURCES 2



#define GRASS_CREATE_DRAW_CALLS_BLOCK_SIZE 32


struct grass_create_draw_calls_cb
{
    uint32 maxNumInstances;
};

#define GRASS_CREATE_DRAW_CALLS_RS \
    "RootConstants(num32BitConstants=1, b0), " \
	"UAV(u0), " \
	"SRV(t0)"

#define GRASS_CREATE_DRAW_CALLS_RS_CB			    0
#define GRASS_CREATE_DRAW_CALLS_RS_OUTPUT			1
#define GRASS_CREATE_DRAW_CALLS_RS_MESH_COUNTS		2


#endif
