#include "material.hlsli"
#include "camera.hlsli"

struct fire_particle_data
{
	vec3 position;
	uint32 maxLife_life;
	vec3 startVelocity;
	uint32 sinAngle_cosAngle;
};

defineSpline(float, 4)
defineSpline(float, 8)

struct fire_particle_cb
{
	// Simulation.
	vec3 emitPosition;
	uint32 frameIndex; 
	vec3 cameraPosition;
	uint32 padding;

	// Rendering.
	spline(float, 4) sizeOverLifetime;
	spline(float, 4) intensityOverLifetime;
	spline(float, 4) atlasProgressionOverLifetime;
	texture_atlas_cb atlas;
};

#ifdef HLSL
#define particle_data fire_particle_data

static float getRelLife(float life, float maxLife)
{
	float relLife = saturate((maxLife - life) / maxLife);
	return relLife;
}

#endif

#ifdef PARTICLE_SIMULATION

#define REQUIRES_SORTING

#define USER_PARTICLE_SIMULATION_RS \
	"CBV(b0)"

ConstantBuffer<fire_particle_cb> cb		: register(b0);

static particle_data emitParticle(uint emitIndex)
{
	uint rng = initRand(emitIndex, cb.frameIndex);

	float radiusAtDistanceOne = 0.1f;
	float2 diskPoint = getRandomPointOnDisk(rng, radiusAtDistanceOne);

	float3 position = cb.emitPosition;
	float3 velocity = normalize(float3(1.f, diskPoint.x, diskPoint.y)) * 2.f;
	velocity.z *= 3.f;

	float maxLife = nextRandBetween(rng, 1.5f, 2.5f);

	float angle = nextRand(rng) * 2.f * pi;
	float sinAngle, cosAngle;
	sincos(angle, sinAngle, cosAngle);

	particle_data particle = {
		position,
		packHalfs(maxLife, maxLife),
		velocity,
		packHalfs(sinAngle, cosAngle)
	};

	return particle;
}

static bool simulateParticle(inout particle_data particle, float dt, out float sortKey)
{
	float life = unpackHalfsRight(particle.maxLife_life);
	life -= dt;
	if (life <= 0)
	{
		return false;
	}
	else
	{
		float maxLife = unpackHalfsLeft(particle.maxLife_life);
		float relLife = getRelLife(life, maxLife);

		float3 velocity = particle.startVelocity;
		velocity.y += smoothstep(0.45f, 0.6f, relLife) * 4.f;
		velocity.x += smoothstep(0.8f, 0.f, relLife) * 8.f;

		particle.position = particle.position + velocity * dt;
		particle.maxLife_life = packHalfs(maxLife, life);

		float3 V = particle.position - cb.cameraPosition;
		sortKey = dot(V, V);

		return true;
	}
}

#endif




#ifdef PARTICLE_RENDERING

#define USER_PARTICLE_RENDERING_RS \
    "CBV(b0, visibility=SHADER_VISIBILITY_VERTEX), " \
    "CBV(b1, visibility=SHADER_VISIBILITY_VERTEX), " \
	"DescriptorTable(SRV(t0, numDescriptors=1), visibility=SHADER_VISIBILITY_PIXEL), " \
	"StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_WRAP," \
        "addressV = TEXTURE_ADDRESS_WRAP," \
        "addressW = TEXTURE_ADDRESS_WRAP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR," \
        "visibility=SHADER_VISIBILITY_PIXEL)"


struct vs_input
{
	float3 position			: POSITION;
};

struct vs_output
{
	float intensity			: INTENSITY;
	float2 uv0				: TEXCOORDS0;
	float2 uv1				: TEXCOORDS1;
	float uvBlend			: BLEND;
	float alphaScale		: ALPHASCALE;
	float4 position			: SV_Position;
};

ConstantBuffer<fire_particle_cb> cb		: register(b0);
ConstantBuffer<camera_cb> camera		: register(b1);

Texture2D<float4> tex					: register(t0);
SamplerState texSampler					: register(s0);

static float2 getUVs(texture_atlas_cb atlas, uint atlasIndex, float2 originalUV)
{
	uint x = atlas.getX(atlasIndex);
	uint y = atlas.getY(atlasIndex);

	float invCols = atlas.getInvNumCols();
	float invRows = atlas.getInvNumRows();
	float2 uv0 = float2(x * invCols, y * invRows);
	float2 uv1 = float2((x + 1) * invCols, (y + 1) * invRows);

	return lerp(uv0, uv1, originalUV);
}

static vs_output vertexShader(vs_input IN, StructuredBuffer<particle_data> particles, uint index)
{
	float3 pos = particles[index].position;

	uint maxLife_life = particles[index].maxLife_life;
	float life, maxLife;
	unpackHalfs(maxLife_life, maxLife, life);
	float relLife = getRelLife(life, maxLife);

	float2 rotation; // Sin(angle), cos(angle).
	unpackHalfs(particles[index].sinAngle_cosAngle, rotation.x, rotation.y);

	float size = cb.sizeOverLifetime.evaluate(4, relLife);
	float2 localPosition = IN.position.xy * size;
	localPosition = float2(dot(localPosition, float2(rotation.y, -rotation.x)), dot(localPosition, rotation));
	pos += localPosition.x * camera.right.xyz + localPosition.y * camera.up.xyz;

	texture_atlas_cb atlas = cb.atlas;
	float atlasProgression = cb.atlasProgressionOverLifetime.evaluate(4, relLife);
	float atlasIndex = atlasProgression * (atlas.getTotalNumCells() - 1);
	float2 originalUV = IN.position.xy * 0.5f + 0.5f;

	vs_output OUT;
	OUT.position = mul(camera.viewProj, float4(pos, 1.f));
	OUT.intensity = 20 * cb.intensityOverLifetime.evaluate(4, relLife);

	OUT.uv0 = getUVs(atlas, (uint)atlasIndex, originalUV);
	OUT.uv1 = getUVs(atlas, (uint)atlasIndex + 1, originalUV);
	OUT.uvBlend = 1.f - frac(atlasIndex);

	OUT.alphaScale = 1.f - smoothstep(0.9f, 1.f, atlasProgression);

	return OUT;
}

static float4 pixelShader(vs_output IN)
{
	float4 color = lerp(tex.Sample(texSampler, IN.uv0), tex.Sample(texSampler, IN.uv1), IN.uvBlend);
	float3 emission = color.rgb * float3(0.75f, 0.25f, 0.09f) * IN.intensity;
	return mergeAlphaBlended(color.rgb, (float3)0.f, emission, color.a * IN.alphaScale);
}

#endif


// Simulation.
#define FIRE_PARTICLE_SYSTEM_COMPUTE_RS_CBV			0

// Rendering.
#define FIRE_PARTICLE_SYSTEM_RENDERING_RS_CBV		0
#define FIRE_PARTICLE_SYSTEM_RENDERING_RS_CAMERA	1
#define FIRE_PARTICLE_SYSTEM_RENDERING_RS_TEXTURE	2


