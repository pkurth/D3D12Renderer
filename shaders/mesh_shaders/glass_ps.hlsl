#include "camera.hlsli"

struct ps_input
{
	float3 worldPos : POSITION;
	float3 normal	: NORMAL;
}; 

ConstantBuffer<camera_cb> camera	: register(b1);

SamplerState texSampler				: register(s0, space1);
TextureCube<float4> tex				: register(t0, space1);

float4 main(ps_input IN) : SV_TARGET
{
	float3 V = normalize(IN.worldPos - camera.position.xyz);
	float3 N = normalize(IN.normal);

	// Compute reflection and refraction vectors.
	const float ETA = 1.12f;
	float c = dot(V, N);
	float d = ETA * c;
	float k = saturate(d * d + (1.f - ETA * ETA));
	float3 reflVec = V - 2.f * c * N;				// reflect(V, N)
	float3 refrVec = ETA * V - (d + sqrt(k)) * N;	// refract(V, N, ETA)

	// Sample and blend.
	float3 refl = tex.Sample(texSampler, reflVec).rgb;
	float3 refr = tex.Sample(texSampler, refrVec).rgb;
	float3 sky = lerp(refl, refr, k);

	// Add a cheap and fake bubble color effect.
	float3 bubbleTint = 0.25f * pow(1.f - k, 5.f) * abs(N);

	return float4(sky + bubbleTint, 1.f);
}
