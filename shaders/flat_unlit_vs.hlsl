#define RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"DENY_HULL_SHADER_ROOT_ACCESS |" \
"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
"DENY_PIXEL_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=16, b0, visibility=SHADER_VISIBILITY_VERTEX)"


struct transform_cb
{
	float4x4 mvp;
};

ConstantBuffer<transform_cb> transform : register(b0);

struct vs_input
{
	float3 position		: POSITION;
};

struct vs_output
{
	float4 position			: SV_POSITION;
};

[RootSignature(RS)]
vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.position = mul(transform.mvp, float4(IN.position, 1.f));
	return OUT;
}
