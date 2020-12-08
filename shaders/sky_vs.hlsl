#include "sky_rs.hlsli"

ConstantBuffer<sky_cb> sky : register(b0);

struct vs_input
{
	float3 position : POSITION;
};

struct vs_output
{
	float3 uv		: TEXCOORDS;
	float4 position : SV_Position;
};

vs_output main(vs_input IN)
{
	vs_output OUT;

	OUT.uv = IN.position;
	OUT.position = mul(sky.vp, float4(IN.position, 1.f));
	OUT.position.z = OUT.position.w - 1e-6f;

	return OUT;
}
