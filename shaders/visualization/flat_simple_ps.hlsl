#include "visualization_rs.hlsli"
#include "camera.hlsli"

ConstantBuffer<visualization_cb> cb	: register(b1);
ConstantBuffer<camera_cb> camera	: register(b2);

struct ps_input
{
	float3 worldPosition	: WORLD_POSITION;
	float3 worldNormal		: WORLD_NORMAL;
};

[RootSignature(FLAT_SIMPLE_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	float ndotv = saturate(dot(normalize(camera.position.xyz - IN.worldPosition), normalize(IN.worldNormal))) * 0.8 + 0.2;
	return ndotv * cb.color;
}
