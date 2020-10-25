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

static tonemap_cb defaultTonemapParameters()
{
	tonemap_cb result;
	result.exposure = 0.5f;
	result.A = 0.22f;
	result.B = 0.3f;
	result.C = 0.1f;
	result.D = 0.2f;
	result.E = 0.01f;
	result.F = 0.3f;
	result.linearWhite = 11.2f;
	return result;
}

#define PRESENT_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"DENY_VERTEX_SHADER_ROOT_ACCESS |" \
"DENY_HULL_SHADER_ROOT_ACCESS |" \
"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=8, b0, visibility=SHADER_VISIBILITY_PIXEL)" 

#define PRESENT_RS_MVP_CBV_PARAM 0

#endif
