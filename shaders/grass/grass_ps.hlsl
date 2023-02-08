#include "grass_rs.hlsli"
#include "camera.hlsli"
#include "lighting.hlsli"

ConstantBuffer<camera_cb> camera		: register(b1);
ConstantBuffer<lighting_cb> lighting	: register(b2);

struct ps_input
{
	float2 uv				: TEXCOORDS;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float3 worldPosition	: POSITION;

	float4 screenPosition	: SV_POSITION;
	bool isFrontFace		: SV_IsFrontFace;
};

[RootSignature(GRASS_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	float3 L = -lighting.sun.direction;

	float3 N = IN.normal;
	float3 T = IN.tangent;
	if (!IN.isFrontFace)
	{
		N = -N;
	}
	N = normalize(N + T * IN.uv.x);



	float3 albedo = lerp(pow(float3(121, 208, 33) / 255, 2.2), pow(float3(193, 243, 118) / 255, 2.2), IN.uv.y);

	float totalIntensity = 0.f;


	// Standard lighting.
	{
		float NdotL = saturate(dot(N, L));
		totalIntensity += NdotL;
	}


	// Subsurface scattering.
	{
		float3 camToP = IN.worldPosition - camera.position.xyz;
		float3 V = -normalize(camToP);

		// https://www.alanzucconi.com/2017/08/30/fast-subsurface-scattering-1/
		const float distortion = 0.3f;
		float3 sssH = L + N * distortion;
		float sssVdotH = saturate(dot(V, -sssH));

		float sssIntensity = sssVdotH;
		totalIntensity += sssIntensity;
	}

	return float4(totalIntensity * albedo, 1.f);

}

