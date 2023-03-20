#ifndef DEPTH_ONLY_RS_HLSLI
#define DEPTH_ONLY_RS_HLSLI

struct shadow_transform_cb
{
    mat4 mvp;
};

struct point_shadow_cb
{
    vec3 lightPosition;
    float maxDistance;
    float flip;
};

struct depth_only_object_id_cb
{
    uint32 id;
};

struct depth_only_camera_jitter_cb
{
    vec2 jitter;
    vec2 prevFrameJitter;
};

struct depth_only_transform_cb
{
    mat4 mvp;
    mat4 prevFrameMVP;
};

#define SHADOW_RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
    "DENY_PIXEL_SHADER_ROOT_ACCESS)," \
    "SRV(t0, visibility=SHADER_VISIBILITY_VERTEX), " \
    "RootConstants(num32BitConstants=16, b0, visibility=SHADER_VISIBILITY_VERTEX)"

#define POINT_SHADOW_RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
    "DENY_PIXEL_SHADER_ROOT_ACCESS)," \
    "SRV(t0, visibility=SHADER_VISIBILITY_VERTEX), " \
    "RootConstants(num32BitConstants=5, b0, visibility=SHADER_VISIBILITY_VERTEX), " \

#define DEPTH_ONLY_RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
    "SRV(t0, visibility=SHADER_VISIBILITY_VERTEX), " \
    "SRV(t1, visibility=SHADER_VISIBILITY_VERTEX), " \
    "SRV(t2, visibility=SHADER_VISIBILITY_VERTEX), " \
    "CBV(b0, space=1)"

#define ALPHA_CUTOUT_DEPTH_ONLY_RS \
    DEPTH_ONLY_RS ", " \
    "DescriptorTable(SRV(t0, numDescriptors=1, space=1), visibility=SHADER_VISIBILITY_PIXEL), " \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_WRAP," \
        "addressV = TEXTURE_ADDRESS_WRAP," \
        "addressW = TEXTURE_ADDRESS_WRAP," \
        "filter = FILTER_MIN_MAG_LINEAR_MIP_POINT," \
        "visibility=SHADER_VISIBILITY_PIXEL)"

#define DEPTH_ONLY_RS_TRANSFORM             0
#define DEPTH_ONLY_RS_PREV_FRAME_TRANSFORM  1
#define DEPTH_ONLY_RS_OBJECT_ID             2
#define DEPTH_ONLY_RS_CAMERA                3
#define DEPTH_ONLY_RS_ALPHA_TEXTURE         4

#define SHADOW_RS_TRANSFORMS                0
#define SHADOW_RS_VIEWPROJ                  1
#define POINT_SHADOW_RS_CB                  1

#endif

