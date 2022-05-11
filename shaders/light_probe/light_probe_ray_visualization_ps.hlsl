#include "light_probe_rs.hlsli"

struct ps_input
{
	float3 color : COLOR;
	float distance : DISTANCE;
};


[RootSignature(LIGHT_PROBE_RAY_VISUALIZATION_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	return float4(IN.color, 1.f);
	//return (IN.distance < 100.f) ? float4(0.f, 1.f, 0.f, 1.f) : float4(1.f, 0.f, 0.f, 1.f);
}
