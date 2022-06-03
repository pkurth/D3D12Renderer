#include "light_probe_rs.hlsli"

struct ps_input
{
	float2 uvOffset			: UV_OFFSET;
	float2 uvScale			: UV_SCALE;
	float3 normal			: NORMAL;
};

Texture2D<float3> irradiance	: register(t0);
SamplerState linearSampler		: register(s0);



[RootSignature(LIGHT_PROBE_GRID_VISUALIZATION_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	const float octScale = (float)LIGHT_PROBE_RESOLUTION / (float)LIGHT_PROBE_TOTAL_RESOLUTION;

	float2 oct = encodeOctahedral(normalize(IN.normal));
	oct *= octScale;
	float2 uv = (IN.uvOffset + oct * 0.5f + 0.5f) * IN.uvScale;

	return float4(irradiance.Sample(linearSampler, uv), 1.f);
}
