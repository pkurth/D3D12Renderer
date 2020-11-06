#ifndef BRDF_HLSLI
#define BRDF_HLSLI

// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// We should really make an optimization pass over this code. Many terms are computed multiple times.



#ifndef M_PI
#define M_PI  3.14159265359f
#endif

#ifndef M_ONE_OVER_PI
#define M_ONE_OVER_PI (1.f / M_PI)
#endif


static float3 fresnelSchlick(float LdotH, float3 F0)
{
	return F0 + (float3(1.f, 1.f, 1.f) - F0) * pow(1.f - LdotH, 5.f);
}

static float3 fresnelSchlickRoughness(float LdotH, float3 F0, float roughness)
{
	float v = 1.f - roughness;
	return F0 + (max(float3(v, v, v), F0) - F0) * pow(1.f - LdotH, 5.f);
}

static float distributionGGX(float3 N, float3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.f);
	float NdotH2 = NdotH * NdotH;

	float nom = a2;
	float denom = (NdotH2 * (a2 - 1.f) + 1.f);
	denom = M_PI * denom * denom;

	return nom / max(denom, 0.001f);
}

static float geometrySchlickGGX(float NdotV, float roughness)
{
	//float r = (roughness + 1.f);
	//float k = (r * r) * 0.125;
	float a = roughness;
	float k = (a * a) / 2.f;

	float nom = NdotV;
	float denom = NdotV * (1.f - k) + k;

	return nom / denom;
}

static float geometrySmith(float3 N, float3 V, float3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.f);
	float NdotL = max(dot(N, L), 0.f);
	float ggx2 = geometrySchlickGGX(NdotV, roughness);
	float ggx1 = geometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

static float radicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

static float2 hammersley(uint i, uint N)
{
	return float2(float(i) / float(N), radicalInverse_VdC(i));
}

static float3 importanceSampleGGX(float2 Xi, float3 N, float roughness)
{
	float a = roughness * roughness;

	float phi = 2.f * M_PI * Xi.x;
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

static float3 calculateDirectLighting(float3 albedo, float3 radiance, float3 N, float3 L, float3 V, float3 F0, float roughness, float metallic)
{
	float3 H = normalize(V + L);
	float NdotV = max(dot(N, V), 0.f);

	// Cook-Torrance BRDF.
	float NDF = distributionGGX(N, H, roughness);
	float G = geometrySmith(N, V, L, roughness);
	float3 F = fresnelSchlick(max(dot(H, V), 0.f), F0);

	float3 kS = F;
	float3 kD = float3(1.f, 1.f, 1.f) - kS;
	kD *= 1.f - metallic;

	float NdotL = max(dot(N, L), 0.f);
	float3 numerator = NDF * G * F;
	float denominator = 4.f * NdotV * NdotL;
	float3 specular = numerator / max(denominator, 0.001f);

	return (kD * albedo * M_ONE_OVER_PI + specular) * radiance * NdotL;
}

#endif
