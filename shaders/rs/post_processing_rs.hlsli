#ifndef POST_PROCESSING_RS_HLSLI
#define POST_PROCESSING_RS_HLSLI


struct bloom_cb
{
    vec2 direction; // [1, 0] or [0, 1], scaled by inverse screen dimensions.
    float multiplier;
};

#define BLOOM_RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_VERTEX_SHADER_ROOT_ACCESS |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
    "RootConstants(num32BitConstants=3, b0, visibility=SHADER_VISIBILITY_PIXEL),"  \
    "StaticSampler(s0," \
	    "addressU = TEXTURE_ADDRESS_CLAMP," \
	    "addressV = TEXTURE_ADDRESS_CLAMP," \
	    "addressW = TEXTURE_ADDRESS_CLAMP," \
	    "filter = FILTER_MIN_MAG_LINEAR_MIP_POINT,"	\
	    "visibility=SHADER_VISIBILITY_PIXEL)," \
    "DescriptorTable(SRV(t0, numDescriptors=1), visibility=SHADER_VISIBILITY_PIXEL)"


#define BLOOM_RS_CB             0
#define BLOOM_RS_SRC_TEXTURE    1


struct taa_cb
{
    vec2 dimensions;
};

#define TAA_RS \
    "RootFlags(0), " \
    "RootConstants(num32BitConstants=2, b0),"  \
    "DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 2) ), " \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"

#define TAA_RS_CB               0
#define TAA_RS_TEXTURES         1

#endif
