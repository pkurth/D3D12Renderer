#include "visualization_rs.hlsli"
#include "camera.hlsli"

ConstantBuffer<visualization_textured_cb> cb	: register(b1);
ConstantBuffer<camera_cb> camera				: register(b2);

Texture2D<float4> tex							: register(t0);
SamplerState texSampler							: register(s0);

struct ps_input
{
	float3 worldPosition	: WORLD_POSITION;
	float2 uv				: TEXCOORDS;
	float3 worldNormal		: WORLD_NORMAL;
};

[RootSignature(FLAT_SIMPLE_TEXTURED_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	float ndotv = saturate(dot(normalize(camera.position.xyz - IN.worldPosition), normalize(IN.worldNormal))) * 0.8 + 0.2;
	float2 uv = lerp(cb.uv0, cb.uv1, IN.uv);
	return ndotv * cb.color * tex.Sample(texSampler, uv);
}
