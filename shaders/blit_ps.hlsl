
#define BLIT_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"DENY_VERTEX_SHADER_ROOT_ACCESS |" \
"DENY_HULL_SHADER_ROOT_ACCESS |" \
"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
"StaticSampler(s0," \
	"addressU = TEXTURE_ADDRESS_CLAMP," \
	"addressV = TEXTURE_ADDRESS_CLAMP," \
	"addressW = TEXTURE_ADDRESS_CLAMP," \
	"filter = FILTER_MIN_MAG_LINEAR_MIP_POINT,"	\
	"visibility=SHADER_VISIBILITY_PIXEL)," \
"DescriptorTable(SRV(t0, numDescriptors=1, flags = DESCRIPTORS_VOLATILE), visibility=SHADER_VISIBILITY_PIXEL)"

struct ps_input
{
	float2 uv	: TEXCOORDS;
};

SamplerState texSampler				: register(s0);
Texture2D<float4> tex				: register(t0);

[RootSignature(BLIT_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	return tex.Sample(texSampler, IN.uv);
}
