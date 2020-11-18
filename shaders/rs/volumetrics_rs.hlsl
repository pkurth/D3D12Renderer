#ifndef VOLUMETRICS_RS_HLSLI
#define VOLUMETRICS_RS_HLSLI

struct volumetrics_transform_cb
{
	mat4 mv;
	mat4 mvp;
};

struct volumetrics_bounding_box_cb
{
	vec4 minCorner;
	vec4 maxCorner;
};

#define VOLUMETRICS_RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	"DENY_HULL_SHADER_ROOT_ACCESS |" \
	"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
	"DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
	"RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX),"  \
	"RootConstants(num32BitConstants=8, b1, visibility=SHADER_VISIBILITY_PIXEL),"  \
	"CBV(b2, visibility=SHADER_VISIBILITY_PIXEL)," \
	"StaticSampler(s0," \
		"addressU = TEXTURE_ADDRESS_WRAP," \
		"addressV = TEXTURE_ADDRESS_WRAP," \
		"addressW = TEXTURE_ADDRESS_WRAP," \
		"filter = FILTER_MIN_MAG_MIP_POINT," \
		"visibility=SHADER_VISIBILITY_PIXEL)," \
    "StaticSampler(s1," \
        "addressU = TEXTURE_ADDRESS_BORDER," \
        "addressV = TEXTURE_ADDRESS_BORDER," \
        "addressW = TEXTURE_ADDRESS_BORDER," \
        "filter = FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT," \
        "visibility=SHADER_VISIBILITY_PIXEL)," \
	"DescriptorTable(SRV(t0, numDescriptors=1), visibility=SHADER_VISIBILITY_PIXEL), " \
	"DescriptorTable(SRV(t1, numDescriptors=4), visibility=SHADER_VISIBILITY_PIXEL), " \
	"CBV(b3, visibility=SHADER_VISIBILITY_PIXEL)"

#define VOLUMETRICS_RS_MVP			0
#define VOLUMETRICS_RS_BOX			1
#define VOLUMETRICS_RS_CAMERA		2
#define VOLUMETRICS_RS_DEPTHBUFFER  3
#define VOLUMETRICS_RS_SUNCASCADES	4
#define VOLUMETRICS_RS_SUN			5

#endif
