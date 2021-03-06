#ifndef POST_PROCESSING_RS_HLSLI
#define POST_PROCESSING_RS_HLSLI

#define POST_PROCESSING_BLOCK_SIZE 16



// ----------------------------------------
// BLOOM
// ----------------------------------------

struct bloom_threshold_cb
{
    vec2 invDimensions;
    float threshold;
};

#define BLOOM_THRESHOLD_RS \
    "RootFlags(0), " \
    "RootConstants(num32BitConstants=3, b0),"  \
    "StaticSampler(s0," \
	    "addressU = TEXTURE_ADDRESS_CLAMP," \
	    "addressV = TEXTURE_ADDRESS_CLAMP," \
	    "addressW = TEXTURE_ADDRESS_CLAMP," \
	    "filter = FILTER_MIN_MAG_MIP_LINEAR)," \
    "DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 1) )"


#define BLOOM_THRESHOLD_RS_CB           0
#define BLOOM_THRESHOLD_RS_TEXTURES     1


struct bloom_combine_cb
{
    vec2 invDimensions;
    float strength;
};

#define BLOOM_COMBINE_RS \
    "RootFlags(0), " \
    "RootConstants(num32BitConstants=3, b0),"  \
    "StaticSampler(s0," \
	    "addressU = TEXTURE_ADDRESS_CLAMP," \
	    "addressV = TEXTURE_ADDRESS_CLAMP," \
	    "addressW = TEXTURE_ADDRESS_CLAMP," \
	    "filter = FILTER_MIN_MAG_MIP_LINEAR)," \
    "DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 2) )"


#define BLOOM_COMBINE_RS_CB           0
#define BLOOM_COMBINE_RS_TEXTURES     1




// ----------------------------------------
// BLIT
// ----------------------------------------

struct blit_cb
{
    vec2 invDimensions;
};

#define BLIT_RS \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 2), " \
    "DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 1) )," \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"


#define BLIT_RS_CB                    0
#define BLIT_RS_TEXTURES              1



// ----------------------------------------
// GAUSSIAN BLUR
// ----------------------------------------

struct gaussian_blur_cb
{
    vec2 invDimensions;
    float stepScale;
    uint32 directionAndSourceMipLevel; // Direction (0 is horizontal, 1 is vertical) | sourceMipLevel.
};

#define GAUSSIAN_BLUR_RS \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 4), " \
    "DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 1) )," \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"


#define GAUSSIAN_BLUR_RS_CB           0
#define GAUSSIAN_BLUR_RS_TEXTURES     1



// ----------------------------------------
// HIERARCHICAL LINEAR DEPTH
// ----------------------------------------

struct hierarchical_linear_depth_cb
{
    vec2 invDimensions;
};

#define HIERARCHICAL_LINEAR_DEPTH_RS \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 2), " \
    "CBV(b1), " \
    "DescriptorTable( UAV(u0, numDescriptors = 6), SRV(t0, numDescriptors = 1) )," \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"


#define HIERARCHICAL_LINEAR_DEPTH_RS_CB           0
#define HIERARCHICAL_LINEAR_DEPTH_RS_CAMERA       1
#define HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES     2



// ----------------------------------------
// TEMPORAL ANTI-ALIASING
// ----------------------------------------

struct taa_cb
{
    vec4 projectionParams;
    vec2 dimensions;
};

#define TAA_RS \
    "RootFlags(0), " \
    "RootConstants(num32BitConstants=8, b0),"  \
    "DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 4) ), " \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"

#define TAA_RS_CB               0
#define TAA_RS_TEXTURES         1




// ----------------------------------------
// SPECULAR AMBIENT
// ----------------------------------------

struct specular_ambient_cb
{
    vec2 invDimensions;
};

#define SPECULAR_AMBIENT_RS \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 2), " \
    "CBV(b1), " \
    "DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 6) )," \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"


#define SPECULAR_AMBIENT_RS_CB           0
#define SPECULAR_AMBIENT_RS_CAMERA       1
#define SPECULAR_AMBIENT_RS_TEXTURES     2




// ----------------------------------------
// TONEMAPPING
// ----------------------------------------

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

#define TONEMAP_RS \
    "RootFlags(0), " \
    "RootConstants(num32BitConstants=8, b0),"  \
    "DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 1) )"

#define TONEMAP_RS_CB               0
#define TONEMAP_RS_TEXTURES         1




// ----------------------------------------
// PRESENT
// ----------------------------------------

#define PRESENT_SDR 0
#define PRESENT_HDR 1


struct present_cb
{
    uint32 displayMode;
    float standardNits;
    float sharpenStrength;
    uint32 offset; // x-offset | y-offset.
};

#define PRESENT_RS \
    "RootFlags(0), " \
    "RootConstants(num32BitConstants=4, b0),"  \
    "DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 1) )"

#define PRESENT_RS_CB               0
#define PRESENT_RS_TEXTURES         1

#endif
