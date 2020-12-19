#ifndef MODEL_RS_HLSLI
#define MODEL_RS_HLSLI

struct static_transform_cb
{
    mat4 mvp;
    mat4 m;
};

struct dynamic_transform_cb
{
    mat4 mvp;
    mat4 m;
    mat4 prevFrameMVP;
};

struct depth_only_transform_cb
{
	mat4 mvp;
};

struct lighting_cb
{
    float environmentIntensity;
};

#if defined(DYNAMIC)
#define TRANSFORM_STRING "RootConstants(num32BitConstants=48, b0, visibility=SHADER_VISIBILITY_VERTEX),"
#define MODEL_RS_PREV_FRAME_POSITIONS_STRING ""
#elif defined(ANIMATED)
#define TRANSFORM_STRING "RootConstants(num32BitConstants=48, b0, visibility=SHADER_VISIBILITY_VERTEX),"
#define MODEL_RS_PREV_FRAME_POSITIONS_STRING ", SRV(t0)"
#else
#define TRANSFORM_STRING "RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX),"
#define MODEL_RS_PREV_FRAME_POSITIONS_STRING ""
#endif

#define MODEL_RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
    TRANSFORM_STRING  \
    "RootConstants(num32BitConstants=6, b0, space=1, visibility=SHADER_VISIBILITY_PIXEL),"  \
    "RootConstants(num32BitConstants=1, b2, space=1, visibility=SHADER_VISIBILITY_PIXEL),"  \
    "CBV(b1, space=1, visibility=SHADER_VISIBILITY_PIXEL), " \
    "DescriptorTable(SRV(t0, numDescriptors=4, space=1), visibility=SHADER_VISIBILITY_PIXEL), " \
    "CBV(b0, space=2, visibility=SHADER_VISIBILITY_PIXEL), " \
    "DescriptorTable(SRV(t0, space=2, numDescriptors=13), visibility=SHADER_VISIBILITY_PIXEL), " \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_WRAP," \
        "addressV = TEXTURE_ADDRESS_WRAP," \
        "addressW = TEXTURE_ADDRESS_WRAP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR," \
        "visibility=SHADER_VISIBILITY_PIXEL)," \
    "StaticSampler(s1," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR," \
        "visibility=SHADER_VISIBILITY_PIXEL)," \
    "StaticSampler(s2," \
        "addressU = TEXTURE_ADDRESS_BORDER," \
        "addressV = TEXTURE_ADDRESS_BORDER," \
        "addressW = TEXTURE_ADDRESS_BORDER," \
        "filter = FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT," \
        "visibility=SHADER_VISIBILITY_PIXEL)" \
    MODEL_RS_PREV_FRAME_POSITIONS_STRING

#define MODEL_DEPTH_ONLY_RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
    "DENY_PIXEL_SHADER_ROOT_ACCESS)," \
    "RootConstants(num32BitConstants=16, b0, visibility=SHADER_VISIBILITY_VERTEX)"

#define MODEL_RS_MVP	                0
#define MODEL_RS_MATERIAL               1
#define MODEL_RS_LIGHTING               2
#define MODEL_RS_CAMERA                 3
#define MODEL_RS_PBR_TEXTURES           4
#define MODEL_RS_SUN                    5
#define MODEL_RS_FRAME_CONSTANTS        6
#define MODEL_RS_PREV_FRAME_POSITIONS   7


#endif

