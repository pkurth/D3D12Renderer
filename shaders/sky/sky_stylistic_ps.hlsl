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

float getStars(float3 V) 
{
	const float scale = 60.f;
	float3 id = floor(V * scale);
	float d = length(scale * V - (id + 0.5f));

	float2 uv = id.xy + vec2(37.f, 17.f) * id.z;
	float rnd = random(uv + 0.5f);

	// https://www.shadertoy.com/view/ttScDc
	float stars = sqrt(0.075f / max(d, 1e-7f)) * (rnd.x > 0.92f && d < 0.15f);

	return stars;
}

float getCloudValue(float2 p, float intensity)
{
	const float density = 0.5f;
	const float sharpness = 0.1f;
	const float scale = 1.f / 0.05f;

	float noise = fbm(p);

	noise = saturate(1.f - exp(-(noise - density) * sharpness)) * scale;
	return noise * intensity;
}

float3 getClouds(float3 V)
{
	const float height = 1000.f;

	float ndotd = -V.y;
	if (abs(ndotd) < 1e-6f)
	{
		return 0.f;
	}

	float t = -height / ndotd;
	if (t < 0.f)
	{
		return 0.f;
	}

	float3 hit = t * V;
	float l = length(hit.xz);

	float intensity = 1.f - smoothstep(0.f, 30000.f, l);

	float2 p = hit.xz * 0.0007f;
	float value = getCloudValue(p, intensity);

#if 0
	const float delta = 0.1f;
	const float invDelta = 1.f / delta;

	float nx = getCloudValue(p + float2(delta, 0.f), intensity);
	float nz = getCloudValue(p + float2(0.f, delta), intensity);

	float3 N = normalize(float3(
		-(nx - value) * invDelta,
		-1.f,
		-(nz - value) * invDelta
	));

#endif
	return value;
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
	float3 skycolor = float3(0.2f, 0.4f, 0.8f) * max(0.2f, L.y);
	float3 suncolor = saturate(lerp(float3(0.99f, 0.3f, 0.1f), float3(1.f, 1.f, 0.8f), L.y));
	float3 sunhalo =
		lerp(
			max(0.f, (1.f - max(0.f, (1.f - L.y * 3.f)) * V.y * 4.f)),
			0.f,
			L.y)
		* saturate(pow(0.5f * LdotV + 0.5f, (8.f - L.y * 5.f)))
		* suncolor;


	float3 color = skycolor;
	//color += saturate(2.f * pow(saturate(LdotV), 800.f)) * (suncolor + 0.4f.xxx);
	color += saturate(2.f * pow(saturate(LdotV), 2500.f)) * (suncolor + 0.4f.xxx) * 300.f;
	color += square(sunhalo);
	color += square(1.f - V.y) * suncolor * 0.5f;

	color += (getStars(V * 1.5f) + getStars(V * 3.f)) * saturate(-0.3f - L.y);

	
	// Clouds.
	float3 cloudcolor = getClouds(V);

	color += cloudcolor;

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
