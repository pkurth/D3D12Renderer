#ifndef DEPTH_ONLY_RS_HLSLI
#define DEPTH_ONLY_RS_HLSLI

struct shadow_transform_cb
{
    mat4 mvp;
};

struct point_shadow_transform_cb
{
    mat4 m;
    vec3 lightPosition;
    float maxDistance;
    float flip;
    float padding[3];
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
    "RootConstants(num32BitConstants=16, b0, visibility=SHADER_VISIBILITY_VERTEX)"

#define POINT_SHADOW_RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
    "DENY_PIXEL_SHADER_ROOT_ACCESS)," \
    "RootConstants(num32BitConstants=24, b0, visibility=SHADER_VISIBILITY_VERTEX)"

#define DEPTH_ONLY_RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
    "RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX), " \
    "RootConstants(num32BitConstants=1, b1, visibility=SHADER_VISIBILITY_PIXEL), " \
    "RootConstants(num32BitConstants=4, b2, visibility=SHADER_VISIBILITY_PIXEL)"

#define ANIMATED_DEPTH_ONLY_RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
    "RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX), " \
    "RootConstants(num32BitConstants=1, b1, visibility=SHADER_VISIBILITY_PIXEL), " \
    "RootConstants(num32BitConstants=4, b2, visibility=SHADER_VISIBILITY_PIXEL), " \
    "SRV(t0)"

#define DEPTH_ONLY_RS_MVP                   0
#define DEPTH_ONLY_RS_OBJECT_ID             1
#define DEPTH_ONLY_RS_CAMERA_JITTER         2
#define DEPTH_ONLY_RS_PREV_FRAME_POSITIONS  3

#define SHADOW_RS_MVP                       0

#endif

