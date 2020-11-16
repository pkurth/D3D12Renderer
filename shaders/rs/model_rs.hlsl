#ifndef MODEL_RS_HLSLI
#define MODEL_RS_HLSLI

struct transform_cb
{
	mat4 mvp;
	mat4 m;
};

#define USE_ALBEDO_TEXTURE		(1 << 0)
#define USE_NORMAL_TEXTURE		(1 << 1)
#define USE_ROUGHNESS_TEXTURE	(1 << 2)
#define USE_METALLIC_TEXTURE	(1 << 3)
#define USE_AO_TEXTURE			(1 << 4)

struct pbr_material_cb
{
    vec4 albedoTint;
    float roughnessOverride;
    float metallicOverride;
    uint32 flags;
};

#define MODEL_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"DENY_HULL_SHADER_ROOT_ACCESS |" \
"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX),"  \
"RootConstants(num32BitConstants=8, b1, visibility=SHADER_VISIBILITY_PIXEL),"  \
"CBV(b2, visibility=SHADER_VISIBILITY_PIXEL), " \
"StaticSampler(s0," \
    "addressU = TEXTURE_ADDRESS_WRAP," \
    "addressV = TEXTURE_ADDRESS_WRAP," \
    "addressW = TEXTURE_ADDRESS_WRAP," \
    "filter = FILTER_MIN_MAG_MIP_LINEAR," \
    "visibility=SHADER_VISIBILITY_PIXEL)," \
"DescriptorTable(SRV(t0, numDescriptors=4, flags = DESCRIPTORS_VOLATILE), visibility=SHADER_VISIBILITY_PIXEL), " \
"DescriptorTable(SRV(t0, space=1, numDescriptors=2, flags = DESCRIPTORS_VOLATILE), visibility=SHADER_VISIBILITY_PIXEL), " \
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
    "visibility=SHADER_VISIBILITY_PIXEL)," \
"DescriptorTable(SRV(t0, space=2, numDescriptors=1, flags = DESCRIPTORS_VOLATILE), visibility=SHADER_VISIBILITY_PIXEL), " \
"CBV(b0, space=3, visibility=SHADER_VISIBILITY_PIXEL), " \
"DescriptorTable(SRV(t0, space=3, numDescriptors=9, flags = DESCRIPTORS_VOLATILE), visibility=SHADER_VISIBILITY_PIXEL)"

#define MODEL_DEPTH_ONLY_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"DENY_HULL_SHADER_ROOT_ACCESS |" \
"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
"DENY_PIXEL_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX)"

#define MODEL_RS_MVP	                0
#define MODEL_RS_MATERIAL               1
#define MODEL_RS_CAMERA                 2
#define MODEL_RS_PBR_TEXTURES           3
#define MODEL_RS_ENVIRONMENT_TEXTURES   4
#define MODEL_RS_BRDF                   5
#define MODEL_RS_SUN                    6
#define MODEL_RS_LIGHTS                 7

#endif

