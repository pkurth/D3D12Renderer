#ifndef SSR_RS_HLSLI
#define SSR_RS_HLSLI

#define SSR_BLOCK_SIZE 16


struct ssr_raycast_cb
{
    vec2 dimensions;
    vec2 invDimensions;
    uint32 frameIndex;
    uint32 numSteps;

    float maxDistance;

    float strideCutoff;
    float minStride;
    float maxStride;

    float thicknessOffset;
    float thicknessBias;
};

#ifndef HLSL
static ssr_raycast_cb defaultSSRParameters()
{
    ssr_raycast_cb result;
    result.numSteps = 400;
    result.maxDistance = 1000.f;
    result.strideCutoff = 100.f;
    result.minStride = 1.f;
    result.maxStride = 10.f;
    result.thicknessOffset = 0.f;
    result.thicknessBias = 1.f;
    return result;
}
#endif


#define SSR_RAYCAST_RS \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 12), " \
    "CBV(b1), " \
    "DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 5) )," \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR), " \
    "StaticSampler(s1," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_POINT)"


#define SSR_RAYCAST_RS_CB           0
#define SSR_RAYCAST_RS_CAMERA       1
#define SSR_RAYCAST_RS_TEXTURES     2

#endif
