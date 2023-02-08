#include "grass_rs.hlsli"
#include "camera.hlsli"
#include "lighting.hlsli"
#include "normal.hlsli"

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


struct ps_output
{
	float4 hdrColor				: SV_Target0;
	float4 worldNormalRoughness	: SV_Target1;
};

[RootSignature(GRASS_RS)]
ps_output main(ps_input IN)
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

	float roughness = 0.7f;

	ps_output OUT;
	OUT.hdrColor = float4(totalIntensity * albedo, 1.f);
	OUT.worldNormalRoughness = float4(packNormal(N), roughness, 0.f);
	return OUT;

}

