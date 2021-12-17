#ifndef LIGHT_SOURCE_H
#define LIGHT_SOURCE_H

#define M_SQRT_PI 1.77245385090f

// Used for point and spot lights, because I dislike very high numbers.
#define LIGHT_RADIANCE_SCALE 1000.f

#define MAX_NUM_SUN_SHADOW_CASCADES 4

static float getAttenuation(float distance, float maxDistance)
{
	// https://imdoingitwrong.wordpress.com/2011/02/10/improved-light-attenuation/
	float relDist = min(distance / maxDistance, 1.f);
	float d = distance / (1.f - relDist * relDist);
	
	float att =  1.f / (d * d + 1.f);
	return att;
}

struct directional_light_cb
{
	mat4 viewProjs[MAX_NUM_SUN_SHADOW_CASCADES];
	vec4 viewports[MAX_NUM_SUN_SHADOW_CASCADES];

	vec4 cascadeDistances;
	vec4 bias;

	vec3 direction;
	uint32 numShadowCascades;
	vec3 radiance;
	uint32 padding;
	vec4 blendDistances;
};

struct point_light_cb
{
	vec3 position;
	float radius; // Maximum distance.
	vec3 radiance;
	int shadowInfoIndex; // -1, if light casts no shadows.


	void initialize(vec3 position_, vec3 radiance_, float radius_, int shadowInfoIndex_ = -1)
	{
		position = position_;
		radiance = radiance_;
		radius = radius_;
		shadowInfoIndex = shadowInfoIndex_;
	}

#ifndef HLSL
	point_light_cb() {}

	point_light_cb(vec3 position_, vec3 radiance_, float radius_, int shadowInfoIndex_ = -1)
	{
		initialize(position_, radiance_, radius_, shadowInfoIndex_);
	}
#endif


};

struct spot_light_cb
{
	vec3 position;
	int innerAndOuterCutoff; // cos(innerAngle) << 16 | cos(outerAngle). Both are packed into 16 bit signed ints.
	vec3 direction;
	float maxDistance;
	vec3 radiance;
	int shadowInfoIndex; // -1, if light casts no shadows.


	void initialize(vec3 position_, vec3 direction_, vec3 radiance_, float innerAngle_, float outerAngle_, float maxDistance_, int shadowInfoIndex_ = -1)
	{
		position = position_;
		direction = direction_;
		radiance = radiance_;
		
		int inner = (int)(cos(innerAngle_) * ((1 << 15) - 1));
		int outer = (int)(cos(outerAngle_) * ((1 << 15) - 1));
		innerAndOuterCutoff = (inner << 16) | outer;

		maxDistance = maxDistance_;
		shadowInfoIndex = shadowInfoIndex_;
	}

#ifndef HLSL
	spot_light_cb() {}

	spot_light_cb(vec3 position_, vec3 direction_, vec3 radiance_, float innerAngle_, float outerAngle_, float maxDistance_, int shadowInfoIndex_ = -1)
	{
		initialize(position_, direction_, radiance_, innerAngle_, outerAngle_, maxDistance_, shadowInfoIndex_);
	}
#endif

	float getInnerCutoff()
#ifndef HLSL
		const
#endif
	{
		return (innerAndOuterCutoff >> 16) / float((1 << 15) - 1);
	}

	float getOuterCutoff()
#ifndef HLSL
		const
#endif
	{
		return (innerAndOuterCutoff & 0xFFFF) / float((1 << 15) - 1);
	}
};

struct spot_shadow_info
{
	mat4 viewProj;

	vec4 viewport;

	float bias;
	float padding0[3];
};

struct point_shadow_info
{
	vec4 viewport0;
	vec4 viewport1;
};

struct spherical_harmonics
{
	vec4 coefficients[7];

	void initialize(float r[9], float g[9], float b[9])
	{
		// Pack the SH coefficients in a way that makes applying the lighting use the least shader instructions
		// This has the diffuse convolution coefficients baked in. See "Stupid Spherical Harmonics (SH) Tricks", Appendix A10.
		const float c0 = 1.f / (2.f * M_SQRT_PI);
		const float c1 = sqrt(3.f) / (3.f * M_SQRT_PI);
		const float c2 = sqrt(15.f) / (8.f * M_SQRT_PI);
		const float c3 = sqrt(5.f) / (16.f * M_SQRT_PI);
		const float c4 = 0.5f * c2;

		coefficients[0].x = -c1 * r[3];
		coefficients[0].y = -c1 * r[1];
		coefficients[0].z = c1 * r[2];
		coefficients[0].w = c0 * r[0] - c3 * r[6];

		coefficients[1].x = -c1 * g[3];
		coefficients[1].y = -c1 * g[1];
		coefficients[1].z = c1 * g[2];
		coefficients[1].w = c0 * g[0] - c3 * g[6];

		coefficients[2].x = -c1 * b[3];
		coefficients[2].y = -c1 * b[1];
		coefficients[2].z = c1 * b[2];
		coefficients[2].w = c0 * b[0] - c3 * b[6];

		coefficients[3].x = c2 * r[4];
		coefficients[3].y = -c2 * r[5];
		coefficients[3].z = 3.f * c3 * r[6];
		coefficients[3].w = -c2 * r[7];

		coefficients[4].x = c2 * g[4];
		coefficients[4].y = -c2 * g[5];
		coefficients[4].z = 3.f * c3 * g[6];
		coefficients[4].w = -c2 * g[7];

		coefficients[5].x = c2 * b[4];
		coefficients[5].y = -c2 * b[5];
		coefficients[5].z = 3.f * c3 * b[6];
		coefficients[5].w = -c2 * b[7];

		coefficients[6].x = c4 * r[8];
		coefficients[6].y = c4 * g[8];
		coefficients[6].z = c4 * b[8];
		coefficients[6].w = 1.f;
	}

#ifndef HLSL
	spherical_harmonics() {}

	spherical_harmonics(float r[9], float g[9], float b[9])
	{
		initialize(r, g, b);
	}
#endif

#ifdef HLSL
	vec3 evaluate(vec3 normal)
	{
		// See "Stupid Spherical Harmonics (SH) Tricks", Appendix A10.
		vec4 N = vec4(normal, 1.f);

		vec3 x1, x2, x3;

		// Linear + constant polynomial terms.
		x1.x = dot(coefficients[0], N);
		x1.y = dot(coefficients[1], N);
		x1.z = dot(coefficients[2], N);

		// 4 of the quadratic polynomials.
		vec4 vB = N.xyzz * N.yzzx;

		x2.x = dot(coefficients[3], vB);
		x2.y = dot(coefficients[4], vB);
		x2.z = dot(coefficients[5], vB);

		// Final quadratic polynomial.
		float vC = N.x * N.x - N.y * N.y;
		x3 = coefficients[6].xyz * vC;

		return max(0, x1 + x2 + x3);
	}
#endif
};

struct spherical_harmonics_basis
{
	float v[9];
};

static spherical_harmonics_basis getSHBasis(vec3 dir)
{
	spherical_harmonics_basis result;
	result.v[0] = 0.282095f;
	result.v[1] = -0.488603f * dir.y;
	result.v[2] = 0.488603f * dir.z;
	result.v[3] = -0.488603f * dir.x;
	result.v[4] = 1.092548f * dir.x * dir.y;
	result.v[5] = -1.092548f * dir.y * dir.z;
	result.v[6] = 0.315392f * (3.f * dir.z * dir.z - 1.f);
	result.v[7] = -1.092548f * dir.z * dir.x;
	result.v[8] = 0.546274f * (dir.x * dir.x - dir.y * dir.y);
	return result;
}


#ifdef HLSL
static float sampleShadowMapSimple(float4x4 vp, float3 worldPosition, 
	Texture2D<float> shadowMap, float4 viewport,
	SamplerComparisonState shadowMapSampler, float bias)
{
	float4 lightProjected = mul(vp, float4(worldPosition, 1.f));
	lightProjected.xyz /= lightProjected.w;

	float2 lightUV = lightProjected.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

	// This case is not handled by a border sampler because we are using a shadow map atlas.
	if (any(lightUV < 0.f || lightUV > 1.f))
	{
		return 1.f;
	}

	lightUV = lightUV * viewport.zw + viewport.xy;

	float visibility = shadowMap.SampleCmpLevelZero(
		shadowMapSampler,
		lightUV,
		lightProjected.z - bias);

	return visibility;
}

static float sampleShadowMapPCF(float4x4 vp, float3 worldPosition, 
	Texture2D<float> shadowMap, float4 viewport,
	SamplerComparisonState shadowMapSampler, 
	float2 texelSize, float bias, float pcfRadius = 1.5f, float numPCFSamples = 16.f)
{
	float4 lightProjected = mul(vp, float4(worldPosition, 1.f));
	lightProjected.xyz /= lightProjected.w;

	float2 lightUV = lightProjected.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

	// This case is not handled by a border sampler because we are using a shadow map atlas.
	if (any(lightUV < 0.f || lightUV > 1.f))
	{
		return 1.f;
	}

	lightUV = lightUV * viewport.zw + viewport.xy;

	float visibility = 0.f;
	for (float y = -pcfRadius; y <= pcfRadius + 0.01f; y += 1.f)
	{
		for (float x = -pcfRadius; x <= pcfRadius + 0.01f; x += 1.f)
		{
			visibility += shadowMap.SampleCmpLevelZero(
				shadowMapSampler,
				lightUV + float2(x, y) * texelSize,
				lightProjected.z - bias);
		}
	}
	visibility /= numPCFSamples;

	return visibility;
}

static float samplePointLightShadowMapPCF(float3 worldPosition, float3 lightPosition,
	Texture2D<float> shadowMap, 
	float4 viewport, float4 viewport2,
	SamplerComparisonState shadowMapSampler, 
	float2 texelSize, float maxDistance, float pcfRadius = 1.5f, float numPCFSamples = 16.f)
{
	float3 L = worldPosition - lightPosition;
	float l = length(L);
	L /= l;

	float flip = L.z > 0.f ? 1.f : -1.f;
	float4 vp = L.z > 0.f ? viewport : viewport2;

	L.z *= flip;
	L.xy /= L.z + 1.f;

	float2 lightUV = L.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

	lightUV = lightUV * vp.zw + vp.xy;

	float compareDistance = l / maxDistance;

	float bias = -0.001f * flip;

	float visibility = 0.f;
	for (float y = -pcfRadius; y <= pcfRadius + 0.01f; y += 1.f)
	{
		for (float x = -pcfRadius; x <= pcfRadius + 0.01f; x += 1.f)
		{
			visibility += shadowMap.SampleCmpLevelZero(
				shadowMapSampler,
				lightUV + float2(x, y) * texelSize,
				compareDistance - bias);
		}
	}
	visibility /= numPCFSamples;

	return visibility;
}

static float sampleCascadedShadowMapSimple(float4x4 vp[4], float3 worldPosition, 
	Texture2D<float> shadowMap, float4 viewports[4],
	SamplerComparisonState shadowMapSampler,
	float pixelDepth, uint numCascades, float4 cascadeDistances, float4 bias, float4 blendDistances)
{
	float blendArea = blendDistances.x;

	float4 comparison = pixelDepth.xxxx > cascadeDistances;

	int currentCascadeIndex = dot(float4(numCascades > 0, numCascades > 1, numCascades > 2, numCascades > 3), comparison);
	currentCascadeIndex = min(currentCascadeIndex, numCascades - 1);

	int nextCascadeIndex = min(numCascades - 1, currentCascadeIndex + 1);

	float visibility = sampleShadowMapSimple(vp[currentCascadeIndex], worldPosition, 
		shadowMap, viewports[currentCascadeIndex],
		shadowMapSampler, bias[currentCascadeIndex]);

	float blendEnd = cascadeDistances[currentCascadeIndex];
	float blendStart = blendEnd - blendDistances[currentCascadeIndex];
	float alpha = smoothstep(blendStart, blendEnd, pixelDepth);

	float nextCascadeVisibility = visibility;
	
	[branch]
	if (currentCascadeIndex != nextCascadeIndex && alpha != 0.f)
	{
		nextCascadeVisibility = sampleShadowMapSimple(vp[nextCascadeIndex], worldPosition,
			shadowMap, viewports[nextCascadeIndex],
			shadowMapSampler, bias[nextCascadeIndex]);
	}

	visibility = lerp(visibility, nextCascadeVisibility, alpha);
	return visibility;
}

static float sampleCascadedShadowMapPCF(float4x4 vp[4], float3 worldPosition, 
	Texture2D<float> shadowMap, float4 viewports[4], 
	SamplerComparisonState shadowMapSampler, 
	float2 texelSize, float pixelDepth, uint numCascades, float4 cascadeDistances, float4 bias, float4 blendDistances)
{
	float4 comparison = pixelDepth.xxxx > cascadeDistances;

	int currentCascadeIndex = dot(float4(numCascades > 0, numCascades > 1, numCascades > 2, numCascades > 3), comparison);
	currentCascadeIndex = min(currentCascadeIndex, numCascades - 1);

	int nextCascadeIndex = min(numCascades - 1, currentCascadeIndex + 1);

	static const float pcfRadius[4] = {
		1.5f, 1.f, 0.5f, 0.f,
	};

	static const float numPCFSamples[4] = {
		16.f, 9.f, 4.f, 1.f,
	};

	float visibility = sampleShadowMapPCF(vp[currentCascadeIndex], worldPosition, 
		shadowMap, viewports[currentCascadeIndex],
		shadowMapSampler, texelSize, bias[currentCascadeIndex], pcfRadius[currentCascadeIndex], numPCFSamples[currentCascadeIndex]);

	float blendEnd = cascadeDistances[currentCascadeIndex];
	float blendStart = blendEnd - blendDistances[currentCascadeIndex];
	float alpha = smoothstep(blendStart, blendEnd, pixelDepth);

	float nextCascadeVisibility = visibility;

	[branch]
	if (currentCascadeIndex != nextCascadeIndex && alpha != 0.f)
	{
		nextCascadeVisibility = sampleShadowMapPCF(vp[nextCascadeIndex], worldPosition,
			shadowMap, viewports[nextCascadeIndex],
			shadowMapSampler, texelSize, bias[nextCascadeIndex], pcfRadius[nextCascadeIndex], numPCFSamples[nextCascadeIndex]);
	}

	visibility = lerp(visibility, nextCascadeVisibility, alpha);
	return visibility;
}

#endif

#endif
