#ifndef SKY_RS_HLSLI
#define SKY_RS_HLSLI

#define TERRAIN_LOD_0_VERTICES_PER_DIMENSION 129
#define TERRAIN_MAX_LOD 5


struct terrain_cb
{
	vec3 minCorner;
	uint32 lod;
	float chunkSize;
	float amplitudeScale;
	uint32 scaleDownByLODs; // 4 * 8 bit.
};

#define SCALE_DOWN_BY_LODS(negX, posX, negZ, posZ) ((negX << 24) | (posX << 16) | (negZ << 8) | posZ)
#define DECODE_LOD_SCALE(v, negX, posX, negZ, posZ) negX = v >> 24, posX = (v >> 16) & 0xFF, negZ = (v >> 8) & 0xFF, posZ = v & 0xFF;

struct terrain_transform_cb
{
	mat4 vp;
};


#define TERRAIN_RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	"DENY_HULL_SHADER_ROOT_ACCESS |" \
	"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
	"DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
	"RootConstants(num32BitConstants=16, b0, visibility=SHADER_VISIBILITY_VERTEX)," \
	"RootConstants(num32BitConstants=7, b1)," \
	"DescriptorTable(SRV(t0, numDescriptors=1), visibility=SHADER_VISIBILITY_VERTEX)," \
	"DescriptorTable(SRV(t1, numDescriptors=1), visibility=SHADER_VISIBILITY_PIXEL)," \
	"CBV(b1, space=1), " \
	"CBV(b2, space=1), " \
	"DescriptorTable(SRV(t0, space=1, numDescriptors=6), visibility=SHADER_VISIBILITY_PIXEL)," \
    "DescriptorTable(SRV(t0, space=2, numDescriptors=3), visibility=SHADER_VISIBILITY_PIXEL), " \
	"StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR," \
        "visibility=SHADER_VISIBILITY_PIXEL), " \
	"StaticSampler(s1," \
        "addressU = TEXTURE_ADDRESS_WRAP," \
        "addressV = TEXTURE_ADDRESS_WRAP," \
        "addressW = TEXTURE_ADDRESS_WRAP," \
        "filter = FILTER_ANISOTROPIC," \
        "visibility=SHADER_VISIBILITY_PIXEL)"


#define TERRAIN_RS_TRANSFORM		0
#define TERRAIN_RS_CB				1
#define TERRAIN_RS_HEIGHTMAP		2
#define TERRAIN_RS_NORMALMAP		3
#define TERRAIN_RS_CAMERA			4
#define TERRAIN_RS_LIGHTING			5
#define TERRAIN_RS_TEXTURES			6
#define TERRAIN_RS_FRAME_CONSTANTS  7




#define TERRAIN_DEPTH_ONLY_RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
    "RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX), " \
    "RootConstants(num32BitConstants=7, b1, visibility=SHADER_VISIBILITY_VERTEX), " \
    "RootConstants(num32BitConstants=1, space=1, b0, visibility=SHADER_VISIBILITY_PIXEL), " \
    "RootConstants(num32BitConstants=4, space=1, b1, visibility=SHADER_VISIBILITY_PIXEL), " \
	"DescriptorTable(SRV(t0, numDescriptors=1), visibility=SHADER_VISIBILITY_VERTEX)"

#define TERRAIN_DEPTH_ONLY_RS_TRANSFORM		0
#define TERRAIN_DEPTH_ONLY_RS_CB			1
#define TERRAIN_DEPTH_ONLY_RS_OBJECT_ID     2
#define TERRAIN_DEPTH_ONLY_RS_CAMERA_JITTER 3
#define TERRAIN_DEPTH_ONLY_RS_HEIGHTMAP		4






struct terrain_generation_settings_cb
{
	vec2 domainWarpNoiseOffset;
	vec2 noiseOffset;

	uint32 heightWidth;
	uint32 heightHeight;
	uint32 normalWidth;
	uint32 normalHeight;

	float positionScale;
	float normalScale;

	float scale;

	float domainWarpStrength;

	uint32 domainWarpOctaves;

	uint32 noiseOctaves;
};

struct terrain_generation_cb
{
	vec2 minCorner;
};

#define TERRAIN_GENERATION_RS \
	"RootConstants(num32BitConstants=2, b0), " \
	"CBV(b1), " \
	"DescriptorTable(UAV(u0, numDescriptors=2))"

#define TERRAIN_GENERATION_RS_CB		0
#define TERRAIN_GENERATION_RS_SETTINGS	1
#define TERRAIN_GENERATION_RS_TEXTURES	2




#endif

