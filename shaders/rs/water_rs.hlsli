#ifndef WATER_RS_HLSLI
#define WATER_RS_HLSLI

struct water_transform_cb
{
    mat4 mvp;
};

struct water_cb
{
    vec4 deepColor;
    vec4 shallowColor;
    float transition;
};


#define WATER_RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
    "RootConstants(num32BitConstants=16, b0, visibility=SHADER_VISIBILITY_VERTEX), " \
    "RootConstants(num32BitConstants=12, b1, visibility=SHADER_VISIBILITY_PIXEL), " \
    "CBV(b2, visibility=SHADER_VISIBILITY_PIXEL), " \
    "DescriptorTable(SRV(t0, numDescriptors=2), visibility=SHADER_VISIBILITY_PIXEL)," \

#define WATER_RS_TRANSFORM      0
#define WATER_RS_SETTINGS       1
#define WATER_RS_CAMERA         2
#define WATER_RS_TEXTURES       3

#endif

