
#define WATER_RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
    "RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX), " \
    "DescriptorTable(SRV(t0, numDescriptors=2), visibility=SHADER_VISIBILITY_PIXEL)," \


Texture2D<float4> opaqueColor   : register(t0);
Texture2D<float> opaqueDepth    : register(t1);


struct ps_input
{
    float4 screenPosition	: SV_POSITION;
};


[RootSignature(WATER_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	return opaqueColor[uint2(IN.screenPosition.xy)] * float4(0.f, 0.f, 1.f, 1.f);
}
