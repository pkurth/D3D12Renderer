
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
	float3 color		: COLOR;
	float4 position		: SV_POSITION;
};

vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.position = mul(transform.mvp, float4(IN.position, 1.f));
	OUT.color = float3(1.f, 1.f, 1.f);
	return OUT;
}
