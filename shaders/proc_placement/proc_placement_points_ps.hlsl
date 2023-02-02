
#define RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
    "CBV(b0, visibility=SHADER_VISIBILITY_VERTEX), " \
    "SRV(t0, visibility=SHADER_VISIBILITY_VERTEX), " \
    "DescriptorTable(SRV(t0, numDescriptors=1, space=1), visibility=SHADER_VISIBILITY_PIXEL), " \
    "StaticSampler(s0, space=1," \
	    "addressU = TEXTURE_ADDRESS_WRAP," \
	    "addressV = TEXTURE_ADDRESS_WRAP," \
	    "addressW = TEXTURE_ADDRESS_WRAP," \
	    "filter = FILTER_MIN_MAG_MIP_LINEAR)"

Texture2D<float4> albedo    : register(t0, space1);
SamplerState wrapSampler    : register(s0, space1);


[RootSignature(RS)]
float4 main(float2 uv : TEXCOORDS) : SV_TARGET
{
	return albedo.Sample(wrapSampler, uv);
}
