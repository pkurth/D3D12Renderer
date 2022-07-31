#ifndef STYLISTIC_SKY_HLSLI
#define STYLISTIC_SKY_HLSLI

#include "random.hlsli"
#include "math.hlsli"


static float getStars(float3 V)
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

static float getCloudValue(float2 p, float intensity)
{
	const float density = 0.5f;
	const float sharpness = 0.1f;
	const float scale = 1.f / 0.05f;

	float noise = fbm(p);

	noise = saturate(1.f - exp(-(noise - density) * sharpness)) * scale;
	return noise * intensity;
}

static float3 getClouds(float3 V)
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

static float3 stylisticSky(float3 V, float3 L)
{
	float LdotV = dot(V, L);

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

	color += getClouds(V);

	return color;
}

static float3 stylisticSkySimple(float3 V, float3 L)
{
	float LdotV = dot(V, L);

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
	color += square(sunhalo);
	color += square(1.f - V.y) * suncolor * 0.5f;

	color += getClouds(V);

	return color;
}


#endif
