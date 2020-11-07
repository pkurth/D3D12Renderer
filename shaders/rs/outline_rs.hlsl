#ifndef OUTLINE_RS_HLSLI
#define OUTLINE_RS_HLSLI

struct outline_cb
{
    mat4 mvp;
    vec4 color;
};

#define OUTLINE_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"DENY_HULL_SHADER_ROOT_ACCESS |" \
"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
"DENY_PIXEL_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=20, b0, visibility=SHADER_VISIBILITY_VERTEX)"

#define OUTLINE_RS_MVP	                0

#endif

