#ifndef BRDF_HLSLI
#define BRDF_HLSLI

#include "math.hlsli"
#include "random.hlsli"

// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// TODO: We should make an optimization pass over this code. Many terms are computed multiple times.



// ----------------------------------------
// FRESNEL (surface becomes more reflective when seen from a grazing angle)
// ----------------------------------------

static float3 fresnelSchlick(float LdotH, float3 F0)
{
	return F0 + (float3(1.f, 1.f, 1.f) - F0) * pow(1.f - LdotH, 5.f);
}

static float3 fresnelSchlickRoughness(float LdotH, float3 F0, float roughness)
{
	float v = 1.f - roughness;
	return F0 + (max(float3(v, v, v), F0) - F0) * pow(1.f - LdotH, 5.f);
}




// ----------------------------------------
// DISTRIBUTION (Microfacets orientation based on roughness)
// ----------------------------------------

#if 0
static float distributionGGX(float NdotH, float roughness)
{
	float a2 = roughness * roughness;
	float d = (NdotH2 * (a2 - 1.f) + 1.f);
	return a2 / max(d * d * pi, 0.001f);
}
#else
static float distributionGGX(float NdotH, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH2 = NdotH * NdotH;

	float d = (NdotH2 * (a2 - 1.f) + 1.f);
	return a2 / max(d * d * pi, 0.001f);
}
#endif



// ----------------------------------------
// GEOMETRIC MASKING (Microfacets may shadow each-other).
// ----------------------------------------

static float geometrySmith(float NdotL, float NdotV, float roughness)
{
	float k = (roughness * roughness) / 2.f;

	float ggx2 = NdotV / (NdotV * (1.f - k) + k);
	float ggx1 = NdotL / (NdotL * (1.f - k) + k);

	return ggx1 * ggx2;
}




// When using this function to sample, the probability density is:
//      pdf = D * NdotH / (4 * HdotV)
float3 importanceSampleGGX(inout uint randSeed, float3 N, float roughness)
{
	// Get our uniform random numbers.
	float2 randVal = float2(nextRand(randSeed), nextRand(randSeed));

	// Get an orthonormal basis from the normal.
	float3 B = getPerpendicularVector(N);
	float3 T = cross(B, N);

	// GGX NDF sampling.
	float a2 = roughness * roughness;
	float cosThetaH = sqrt(max(0.f, (1.f - randVal.x) / ((a2 - 1.f) * randVal.x + 1.f)));
	float sinThetaH = sqrt(max(0.f, 1.f - cosThetaH * cosThetaH));
	float phiH = randVal.y * pi * 2.f;

	// Get our GGX NDF sample (i.e., the half vector).
	return T * (sinThetaH * cos(phiH)) +
		B * (sinThetaH * sin(phiH)) +
		N * cosThetaH;
}

static float3 importanceSampleGGX(float2 Xi, float3 N, float roughness)
{
	float a = roughness * roughness;

	float phi = 2.f * pi * Xi.x;
	float cosTheta = sqrt((1.f - Xi.y) / (1.f + (a * a - 1.f) * Xi.y));
	float sinTheta = sqrt(1.f - cosTheta * cosTheta);

	// From spherical coordinates to cartesian coordinates.
	float3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// From tangent-space vector to world-space sample vector.
	float3 up = abs(N.z) < 0.999 ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
	float3 tangent = normalize(cross(up, N));
	float3 bitangent = cross(N, tangent);

	float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}

static float3 calculateAmbientLighting(float3 albedo, float3 irradiance,
	TextureCube<float4> environmentTexture, Texture2D<float4> brdf, SamplerState clampSampler,
	float3 N, float3 V, float3 F0, float roughness, float metallic, float ao)
{
	// Common.
	float NdotV = saturate(dot(N, V));
	float3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
	float3 kS = F;
	float3 kD = float3(1.f, 1.f, 1.f) - kS;
	kD *= 1.f - metallic;

	// Diffuse.
	float3 diffuse = irradiance * albedo;

	// Specular.
	float3 R = reflect(-V, N);
	uint width, height, numMipLevels;
	environmentTexture.GetDimensions(0, width, height, numMipLevels);
	float lod = roughness * float(numMipLevels - 1);

	float3 prefilteredColor = environmentTexture.SampleLevel(clampSampler, R, lod).rgb;
	float2 envBRDF = brdf.SampleLevel(clampSampler, float2(roughness, NdotV), 0).rg;
	float3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

	float3 ambient = (kD * diffuse + specular) * ao;

	return ambient;
}

static float3 calculateAmbientLighting(float3 albedo,
	TextureCube<float4> irradianceTexture, TextureCube<float4> environmentTexture, Texture2D<float4> brdf, SamplerState clampSampler,
	float3 N, float3 V, float3 F0, float roughness, float metallic, float ao)
{
	float3 irradiance = irradianceTexture.SampleLevel(clampSampler, N, 0).rgb;
	return calculateAmbientLighting(albedo, irradiance, environmentTexture, brdf, clampSampler, N, V, F0, roughness, metallic, ao);
}

static float3 calculateDirectLighting(float3 albedo, float3 radiance, float3 N, float3 L, float3 V, float3 F0, float roughness, float metallic)
{
	float3 H = normalize(V + L);
	float NdotV = saturate(dot(N, V));
	float NdotH = saturate(dot(N, H));
	float NdotL = saturate(dot(N, L));
	float HdotV = saturate(dot(H, V));

	// Cook-Torrance BRDF.
	float NDF = distributionGGX(NdotH, roughness);
	float G = geometrySmith(NdotL, NdotV, roughness);
	float3 F = fresnelSchlick(HdotV, F0);

	float3 kS = F;
	float3 kD = float3(1.f, 1.f, 1.f) - kS;
	kD *= 1.f - metallic;

	float3 numerator = NDF * G * F;
	float denominator = 4.f * NdotV * NdotL;
	float3 specular = numerator / max(denominator, 0.001f);

	return (kD * albedo * invPI + specular) * radiance * NdotL;
}

inline float luminance(float3 rgb)
{
	return dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
}

float probabilityToSampleDiffuse(float3 kD, float3 kS)
{
	float lumDiffuse = max(0.01f, luminance(kD));
	float lumSpecular = max(0.01f, luminance(kS));
	return lumDiffuse / (lumDiffuse + lumSpecular);
}

#endif
