#ifndef VISUALIZATION_HLSLI
#define VISUALIZATION_HLSLI

struct visualization_cb
{
	vec4 color;
};

struct visualization_textured_cb
{
	vec4 color;
	vec2 uv0;
	vec2 uv1;
};

#define FLAT_SIMPLE_TEXTURED_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"DENY_HULL_SHADER_ROOT_ACCESS |" \
"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX), " \
"RootConstants(num32BitConstants=8, b1, visibility=SHADER_VISIBILITY_PIXEL), " \
"DescriptorTable(SRV(t0, numDescriptors=1), visibility=SHADER_VISIBILITY_PIXEL), " \
"StaticSampler(s0, visibility=SHADER_VISIBILITY_PIXEL), " \
"CBV(b2, visibility=SHADER_VISIBILITY_PIXEL)"

#define FLAT_SIMPLE_RS_TRANFORM			0
#define FLAT_SIMPLE_RS_CB				1
#define FLAT_SIMPLE_RS_TEXTURE			2
#define FLAT_SIMPLE_RS_CAMERA			3


#define FLAT_UNLIT_TEXTURED_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"DENY_HULL_SHADER_ROOT_ACCESS |" \
"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=16, b0, visibility=SHADER_VISIBILITY_VERTEX)," \
"RootConstants(num32BitConstants=8, b1, visibility=SHADER_VISIBILITY_PIXEL), " \
"DescriptorTable(SRV(t0, numDescriptors=1), visibility=SHADER_VISIBILITY_PIXEL), " \
"StaticSampler(s0, visibility=SHADER_VISIBILITY_PIXEL)"

#define FLAT_UNLIT_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"DENY_HULL_SHADER_ROOT_ACCESS |" \
"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=16, b0, visibility=SHADER_VISIBILITY_VERTEX)," \
"RootConstants(num32BitConstants=4, b1, visibility=SHADER_VISIBILITY_PIXEL)"

#define FLAT_UNLIT_RS_TRANFORM			0
#define FLAT_UNLIT_RS_CB				1
#define FLAT_UNLIT_RS_TEXTURE			2



#endif
