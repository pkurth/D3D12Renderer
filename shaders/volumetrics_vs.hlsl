#include "volumetrics_rs.hlsl"
#include "camera.hlsl"

ConstantBuffer<volumetrics_transform_cb> transform	: register(b0);

struct vs_input
{
	float3 position : POSITION;
};

struct vs_output
{
	float boxBackfaceDepth : DEPTH;
	float4 position : SV_Position;
};

vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.position = mul(transform.mvp, float4(IN.position, 1.f));
	OUT.boxBackfaceDepth = -mul(transform.mv, float4(IN.position, 1.f)).z;
	return OUT;
}
