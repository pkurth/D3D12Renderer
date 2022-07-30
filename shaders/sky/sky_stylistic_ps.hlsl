#include "math.hlsli"
#include "sky_rs.hlsli"


ConstantBuffer<sky_cb> sky : register(b1);

struct ps_input
{
	float3 uv		: TEXCOORDS;
};

struct ps_output
{
	float4 color			: SV_Target0;
	float2 screenVelocity	: SV_Target1;
	uint objectID			: SV_Target2;
};

[RootSignature(SKY_STYLISTIC_RS)]
ps_output main(ps_input IN)
{
	float3 V = normalize(IN.uv);
	float3 L = -sky.sunDirection;

	float LdotV = dot(V, L);

	float3 horizonColor = float3(1.f, 0.6f, 0.2f);
	float3 skyColor = float3(182, 220, 240) / 255.f;

	skyColor = lerp(float3(0.2, 0.25, 0.30) * 0.1, skyColor, saturate(L.y + 0.6f));

	horizonColor = lerp(horizonColor, skyColor * 1.1f, abs(L.y));


	float3 color = lerp(horizonColor, skyColor, saturate(V.y));

	float sunThresh = 0.9985f;
	float3 sunColorHigh = float3(1.f, 0.93f, 0.76f) * 700.f;
	float3 sunColorLow = float3(0.3f, 0.08f, 0.05f) * 50.f;

	float sd = pow(clamp(0.25 + 0.75 * LdotV, 0.0, 1.0), 4.0);
	float3 sunDownColor = lerp(color, sunColorLow * 0.3f, sd * exp(-abs((60.0 - 50.0 * sd) * V.y)));
	color = lerp(color, sunDownColor, square(1.f - abs(L.y)));

	float3 sunColor = lerp(sunColorLow, sunColorHigh, saturate(L.y));

	color = lerp(color, sunColor, smoothstep(sunThresh, sunThresh + 0.001f, LdotV));

	color = lerp(color, float3(0.f, 0.f, 0.f), smoothstep(0.f, 1.f, saturate(-V.y)));


	ps_output OUT;
	OUT.color = float4(max(color, 0.f), 0.f);
	OUT.screenVelocity = float2(0.f, 0.f); // TODO: This is of course not the correct screen velocity for the sky.
	OUT.objectID = 0xFFFFFFFF; // -1.
	return OUT;
}