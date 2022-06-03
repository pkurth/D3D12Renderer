#include "light_probe_rs.hlsli"

ConstantBuffer<light_probe_grid_cb> grid	: register(b1);

struct ps_input
{
	float3 worldPosition	: POSITION;
	float3 worldNormal		: NORMAL;
};

Texture2D<float3> irradiance	: register(t0);
Texture2D<float2> depth			: register(t1);
SamplerState linearSampler		: register(s0);


[RootSignature(LIGHT_PROBE_TEST_SAMPLE_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	float3 i = grid.sampleIrradianceAtPosition(IN.worldPosition, normalize(IN.worldNormal), irradiance, depth, linearSampler);
	return float4(i, 1.f);
}
