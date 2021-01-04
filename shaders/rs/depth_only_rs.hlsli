#ifndef DEPTH_ONLY_RS_HLSLI
#define DEPTH_ONLY_RS_HLSLI

struct shadow_transform_cb
{
    mat4 mvp;
};

struct depth_only_object_id_cb
{
    uint32 id;
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

#define DEPTH_ONLY_RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
    "RootConstants(num32BitConstants=1, b1, visibility=SHADER_VISIBILITY_PIXEL), " \
    "RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX)"

#define ANIMATED_DEPTH_ONLY_RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
    "RootConstants(num32BitConstants=1, b1, visibility=SHADER_VISIBILITY_PIXEL), " \
    "RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX), " \
    "SRV(t0)"

#define DEPTH_ONLY_RS_OBJECT_ID             0
#define DEPTH_ONLY_RS_MVP                   1
#define DEPTH_ONLY_RS_PREV_FRAME_POSITIONS  2

#define SHADOW_RS_MVP                       0

#endif

