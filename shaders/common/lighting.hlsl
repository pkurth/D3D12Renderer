#ifndef LIGHTING_H
#define LIGHTING_H

struct light_attenuation
{
	float c;
	float l;
	float q;
};

struct directional_light
{
	float4x4 vp[4];
	float4 cascadeDistances;
	float4 bias;

	float4 worldSpaceDirection;
	float4 color;

	uint numShadowCascades;
	float blendArea;
	float texelSize;
	uint shadowMapDimensions;
};

struct spot_light
{
	float4x4 vp;

	float4 worldSpacePosition;
	float4 worldSpaceDirection;
	float4 color;

	light_attenuation attenuation;

	float innerAngle;
	float outerAngle;
	float innerCutoff;
	float outerCutoff;
	float texelSize;
	float bias;
};

struct point_light
{
	float3 position;
	float radius;
	float4 color;
};

static float getAttenuation(light_attenuation a, float distance)
{
	return 1.f / (a.c + a.l * distance + a.q * distance * distance);
}

static float sampleShadowMap(float4x4 vp, float3 worldPosition, Texture2D<float> shadowMap, SamplerComparisonState shadowMapSampler,
	float texelSize, float bias)
{
	float4 lightProjected = mul(vp, float4(worldPosition, 1.f));
	lightProjected.xyz /= lightProjected.w;

	float2 lightUV = lightProjected.xy * 0.5f + float2(0.5f, 0.5f);
	lightUV.y = 1.f - lightUV.y;

	float visibility = 0.f;

	for (float y = -1.5f; y <= 1.5f; y += 1.f)
	{
		for (float x = -1.5f; x <= 1.5f; x += 1.f)
		{
			visibility += shadowMap.SampleCmpLevelZero(shadowMapSampler, lightUV + float2(x, y) * texelSize, lightProjected.z - bias);
		}
	}
	visibility /= 16.f;
	return visibility;
}

#endif