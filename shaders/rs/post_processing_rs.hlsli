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
// MORPHOLOGY
// ----------------------------------------

struct morphology_cb
{
    uint32 radius;
    uint32 direction; // 0 = x, 1 = y.
    uint32 dimInDirection;
};

#define MORPHOLOGY_MAX_RADIUS 16
#define MORPHOLOGY_BLOCK_SIZE 128

#define MORPHOLOGY_RS \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 3), " \
    "DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 1) )"


#define MORPHOLOGY_RS_CB              0
#define MORPHOLOGY_RS_TEXTURES        1



// ----------------------------------------
// HIERARCHICAL LINEAR DEPTH
// ----------------------------------------

struct hierarchical_linear_depth_cb
{
    vec4 projectionParams;
    vec2 invDimensions;
};

#define HIERARCHICAL_LINEAR_DEPTH_RS \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 8), " \
    "DescriptorTable( UAV(u0, numDescriptors = 6), SRV(t0, numDescriptors = 1) )," \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"


#define HIERARCHICAL_LINEAR_DEPTH_RS_CB           0
#define HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES     1



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
// HBAO
// ----------------------------------------

struct hbao_cb
{
    uint32 screenWidth;
    uint32 screenHeight;
    float depthBufferMipLevel;
    uint32 numRays;
    vec2 rayDeltaRotation;
    uint32 maxNumStepsPerRay;
    float radius;
    float strength;
    float seed;
};

#define HBAO_RS \
    "RootFlags(0), " \
    "RootConstants(num32BitConstants=10, b0),"  \
    "CBV(b1), " \
    "DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 1) ), " \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"

#define HBAO_RS_CB                  0
#define HBAO_RS_CAMERA              1
#define HBAO_RS_TEXTURES            2



struct hbao_blur_cb
{
    vec2 invDimensions;
};

#define HBAO_BLUR_X_RS \
    "RootFlags(0), " \
    "RootConstants(num32BitConstants=2, b0),"  \
    "DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 2) ), " \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_POINT), " \
    "StaticSampler(s1," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"

#define HBAO_BLUR_Y_RS \
    "RootFlags(0), " \
    "RootConstants(num32BitConstants=2, b0),"  \
    "DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 2) ), " \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_POINT), " \
    "StaticSampler(s1," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"

#define HBAO_BLUR_RS_CB                  0
#define HBAO_BLUR_RS_TEXTURES            1



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
    float A;
    float B;
    float C;
    float D;
    float E;
    float F;
    float invEvaluatedLinearWhite;
    float expExposure;

    vec3 evaluate(vec3 x)
    {
        return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - (E / F);
    }

    vec3 tonemap(vec3 color)
    {
        color *= expExposure;
        return evaluate(color) * invEvaluatedLinearWhite;
    }
};

#define TONEMAP_RS \
    "RootFlags(0), " \
    "RootConstants(num32BitConstants=8, b0),"  \
    "DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 1) )"

#define TONEMAP_RS_CB               0
#define TONEMAP_RS_TEXTURES         1




// ----------------------------------------
// PRESENT
// ----------------------------------------

#define present_sdr 0
#define present_hdr 1


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




// ----------------------------------------
// DEPTH SOBEL
// ----------------------------------------

struct depth_sobel_cb
{
    vec4 projectionParams;
    float threshold;
};

#define DEPTH_SOBEL_RS \
    "RootFlags(0), " \
    "RootConstants(num32BitConstants=8, b0),"  \
    "DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 1) )"

#define DEPTH_SOBEL_RS_CB           0
#define DEPTH_SOBEL_RS_TEXTURES     1

#endif
