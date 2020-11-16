#ifndef PRESENT_RS_HLSLI
#define PRESENT_RS_HLSLI

struct tonemap_cb
{
	float A; // Shoulder strength.
	float B; // Linear strength.
	float C; // Linear angle.
	float D; // Toe strength.
	float E; // Toe Numerator.
	float F; // Toe denominator.
	// Note E/F = Toe angle.
	float linearWhite;

	float exposure;
};

struct present_cb
{
	uint32 displayMode;
	float standardNits;
};

static tonemap_cb defaultTonemapParameters()
{
	tonemap_cb result;
	result.exposure = 0.2f;
	result.A = 0.22f;
	result.B = 0.3f;
	result.C = 0.1f;
	result.D = 0.2f;
	result.E = 0.01f;
	result.F = 0.3f;
	result.linearWhite = 11.2f;
	return result;
}

static float acesFilmic(float x, float A, float B, float C, float D, float E, float F)
{
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - (E / F);
}

static float filmicTonemapping(float color, tonemap_cb tonemap)
{
	float expExposure = exp2(tonemap.exposure);
	color *= expExposure;

	float r = acesFilmic(color, tonemap.A, tonemap.B, tonemap.C, tonemap.D, tonemap.E, tonemap.F) /
		acesFilmic(tonemap.linearWhite, tonemap.A, tonemap.B, tonemap.C, tonemap.D, tonemap.E, tonemap.F);

	return r;
}

#define PRESENT_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"DENY_VERTEX_SHADER_ROOT_ACCESS |" \
"DENY_HULL_SHADER_ROOT_ACCESS |" \
"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=8, b0, visibility=SHADER_VISIBILITY_PIXEL),"  \
"RootConstants(num32BitConstants=2, b1, visibility=SHADER_VISIBILITY_PIXEL),"  \
"StaticSampler(s0," \
	"addressU = TEXTURE_ADDRESS_CLAMP," \
	"addressV = TEXTURE_ADDRESS_CLAMP," \
	"addressW = TEXTURE_ADDRESS_CLAMP," \
	"filter = FILTER_MIN_MAG_LINEAR_MIP_POINT,"	\
	"visibility=SHADER_VISIBILITY_PIXEL)," \
"DescriptorTable(SRV(t0, numDescriptors=1, flags = DESCRIPTORS_VOLATILE), visibility=SHADER_VISIBILITY_PIXEL)"

#define PRESENT_RS_TONEMAP	0
#define PRESENT_RS_PRESENT	1
#define PRESENT_RS_TEX		2

#endif

