#define RS \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 1), " \
    "DescriptorTable( SRV(t0, numDescriptors = 1, flags = DESCRIPTORS_VOLATILE) )," \
    "DescriptorTable( UAV(u0, numDescriptors = 1, flags = DESCRIPTORS_VOLATILE) )," \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_WRAP," \
        "addressV = TEXTURE_ADDRESS_WRAP," \
        "addressW = TEXTURE_ADDRESS_WRAP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"

#include "cs.hlsli"
#include "brdf.hlsli"
#include "light_source.hlsli"

cbuffer cubemap_to_irradiance_sh_cb : register(b0)
{
	uint mipLevel;
};

TextureCube<float4> srcTexture : register(t0);
SamplerState linearRepeatSampler : register(s0);

RWStructuredBuffer<spherical_harmonics> outSphericalHarmonics : register(u0);

#define BLOCK_SIZE 8


struct spherical_harmonics_vector
{
	float v[9];

	inline void initialize(float color, spherical_harmonics_basis basis)
	{
		[unroll]
		for (uint i = 0; i < 9; ++i)
		{
			v[i] = color * basis.v[i];
		}
	}

	inline void add(spherical_harmonics_vector other)
	{
		[unroll]
		for (uint i = 0; i < 9; ++i)
		{
			v[i] += other.v[i];
		}
	}
};

struct spherical_harmonics_rgb
{
	spherical_harmonics_vector r;
	spherical_harmonics_vector g;
	spherical_harmonics_vector b;

	inline void initialize(float3 dir, float3 color)
	{
		spherical_harmonics_basis basis = getSHBasis(dir);
		r.initialize(color.r, basis);
		g.initialize(color.g, basis);
		b.initialize(color.b, basis);
	}

	inline void add(spherical_harmonics_rgb other)
	{
		r.add(other.r);
		g.add(other.g);
		b.add(other.b);
	}
};

groupshared spherical_harmonics_rgb g_sh[BLOCK_SIZE * BLOCK_SIZE];


[RootSignature(RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(cs_input IN)
{
	const uint sampleCount = BLOCK_SIZE * BLOCK_SIZE;

	const float uniformSampleSolidAngle = 4.f * M_PI / sampleCount;
	const float3 dir = uniformSampleSphere((float2(IN.dispatchThreadID.xy) + 0.5f) / float2(BLOCK_SIZE, BLOCK_SIZE)).xyz;

	const uint linearIndex = BLOCK_SIZE * IN.dispatchThreadID.y + IN.dispatchThreadID.x;


	float3 radiance = srcTexture.SampleLevel(linearRepeatSampler, dir, mipLevel).rgb;
	float3 weightedRadiance = radiance * uniformSampleSolidAngle;

	g_sh[linearIndex].initialize(dir, weightedRadiance);
	GroupMemoryBarrierWithGroupSync();

	if (linearIndex < 32) {	g_sh[linearIndex].add(g_sh[linearIndex + 32]); }
	GroupMemoryBarrierWithGroupSync();

	if (linearIndex < 16) {	g_sh[linearIndex].add(g_sh[linearIndex + 16]); }
	GroupMemoryBarrierWithGroupSync();

	// From here on we don't need to synchronize, because this is the minimum warp size commonly present.
	if (linearIndex < 8) { g_sh[linearIndex].add(g_sh[linearIndex + 8]); }
	if (linearIndex < 4) { g_sh[linearIndex].add(g_sh[linearIndex + 4]); }
	if (linearIndex < 2) { g_sh[linearIndex].add(g_sh[linearIndex + 2]); }
	
	if (linearIndex < 1)
	{
		g_sh[linearIndex].add(g_sh[linearIndex + 1]);

		outSphericalHarmonics[0].initialize(g_sh[0].r, g_sh[0].g, g_sh[0].b);
	}
}


