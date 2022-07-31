#include "math.hlsli"
#include "sky_rs.hlsli"
#include "random.hlsli"


ConstantBuffer<sky_cb> cb : register(b1);

struct ps_input
{
	float3 uv				: TEXCOORDS;
	float3 ndc				: NDC;
	float3 prevFrameNDC		: PREV_FRAME_NDC;
};

struct ps_output
{
	float4 color			: SV_Target0;
	float2 screenVelocity	: SV_Target1;
	uint objectID			: SV_Target2;
};

// https://www.shadertoy.com/view/ttScDc
float getGlow(float dist, float radius, float intensity) 
{
	dist = max(dist, 1e-7);
	return pow(radius / dist, intensity);
}

float getStars(float3 rayDir) 
{
	float scale = 60.0;
	float3 id = floor(rayDir * scale);
	float d = length(scale * rayDir - (id + 0.5));

	float stars = 0.0;

	float2 uv = id.xy + vec2(37.0, 17.0) * id.z;
	float rnd = random(uv + 0.5f);

	if (rnd.x > 0.92 && d < 0.15) {
		stars = getGlow(d, 0.075, 2.5 - 2.0);
	}
	return stars;
}

[RootSignature(SKY_STYLISTIC_RS)]
ps_output main(ps_input IN)
{
	float3 V = normalize(IN.uv);
	float3 L = -cb.sunDirection;

	float LdotV = dot(V, L);


#if 0
	float3 horizonColor = float3(1.f, 0.6f, 0.2f);
	float3 skyColor = float3(233, 241, 247) / 255.f;

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

#else
	// https://www.shadertoy.com/view/tt3cDl
	float3 skycolor = vec3(0.2, 0.4, 0.8) * max(0.2, L.y);
	float3 suncolor = saturate(lerp(vec3(0.99, 0.3, 0.1), vec3(1.0, 1.0, 0.8), L.y));
	float3 sunhalo =
		lerp(
			max(0.0, (1.0 - max(0.0, (1.0 - L.y * 3.0)) * V.y * 4.0)),
			0.0,
			L.y)
		* saturate(pow(0.5 * LdotV + 0.5, (8.0 - L.y * 5.0)))
		* suncolor;


	float3 color = skycolor;
	//color += saturate(2.f * pow(saturate(LdotV), 800.f)) * (suncolor + 0.4f.xxx);
	color += saturate(2.f * pow(saturate(LdotV), 2500.f)) * (suncolor + 0.4f.xxx) * 300.f;
	color += pow(sunhalo, 2.f);
	color += pow(1.f - V.y, 2.0) * suncolor * 0.5f;

	color += (getStars(V * 1.5f) + getStars(V * 3.f)) * saturate(-0.3f - L.y);


#endif

	float2 ndc = (IN.ndc.xy / IN.ndc.z) - cb.jitter;
	float2 prevNDC = (IN.prevFrameNDC.xy / IN.prevFrameNDC.z) - cb.prevFrameJitter;

	float2 motion = (prevNDC - ndc) * float2(0.5f, -0.5f);

	color *= cb.intensity;


	ps_output OUT;
	OUT.color = float4(max(color, 0.f), 0.f);
	OUT.screenVelocity = motion;
	OUT.objectID = 0xFFFFFFFF; // -1.
	return OUT;
}
