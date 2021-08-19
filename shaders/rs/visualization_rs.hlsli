#ifndef VISUALIZATION_HLSLI
#define VISUALIZATION_HLSLI

struct visualization_cb
{
	vec4 color;
};

#define FLAT_SIMPLE_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"DENY_HULL_SHADER_ROOT_ACCESS |" \
"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX), " \
"RootConstants(num32BitConstants=4, b1, visibility=SHADER_VISIBILITY_PIXEL), " \
"CBV(b2, visibility=SHADER_VISIBILITY_PIXEL)"


#define FLAT_UNLIT_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"DENY_HULL_SHADER_ROOT_ACCESS |" \
"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=16, b0, visibility=SHADER_VISIBILITY_VERTEX)," \
"RootConstants(num32BitConstants=4, b1, visibility=SHADER_VISIBILITY_PIXEL)"




#endif
