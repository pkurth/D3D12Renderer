#include "material.hlsli"
#include "camera.hlsli"

struct fire_particle_data
{
	vec3 position;
	uint32 maxLife_life;
	vec3 velocity;
	uint32 sinAngle_cosAngle;
};

defineSpline(float, 4)
defineSpline(float, 8)

struct fire_particle_cb
{
	// Simulation.
	vec3 emitPosition;
	uint32 frameIndex; 
	spline(float, 8) lifeScaleFromDistance;

	// Rendering.
	spline(float, 4) intensityOverLifetime;
	spline(float, 4) atlasProgressionOverLifetime;
	texture_atlas_cb atlas;
};

#ifdef HLSL
#define particle_data fire_particle_data
#endif

#ifdef PARTICLE_SIMULATION

#define USER_PARTICLE_SIMULATION_RS \
	"CBV(b0)"

ConstantBuffer<fire_particle_cb> cb		: register(b0);

static particle_data emitParticle(uint emitIndex)
{
	uint rng = initRand(emitIndex, cb.frameIndex);

	const float radius = 4.f;

	float2 offset2D = getRandomPointOnDisk(rng, radius);
	float3 offset = float3(offset2D.x, nextRand(rng) * 0.5f, offset2D.y);
	float3 position = cb.emitPosition + offset;
	float3 velocity = float3(nextRand(rng), nextRand(rng) * 3.f + 4.f, nextRand(rng));

	float distance = saturate(length(offset.xz) / radius);
	float lifeScale = cb.lifeScaleFromDistance.evaluate(8, distance);

	float maxLife = nextRand(rng) * 5.f * lifeScale + 3.f;

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

static bool simulateParticle(inout particle_data particle, float dt)
{
	float life = unpackHalfsRight(particle.maxLife_life);
	life -= dt;
	if (life <= 0)
	{
		return false;
	}
	else
	{
		float3 gravity = float3(0.f, -1.f * dt, 0.f);
		particle.position = particle.position + 0.5f * gravity * dt + particle.velocity * dt;
		particle.velocity = particle.velocity + gravity;
		float maxLife = unpackHalfsLeft(particle.maxLife_life);
		particle.maxLife_life = packHalfs(maxLife, life);

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
	float2 uv				: TEXCOORDS;
	float4 position			: SV_Position;
};

ConstantBuffer<fire_particle_cb> cb		: register(b0);
ConstantBuffer<camera_cb> camera		: register(b1);

Texture2D<float4> tex					: register(t0);
SamplerState texSampler					: register(s0);


static vs_output vertexShader(vs_input IN, StructuredBuffer<particle_data> particles, uint index)
{
	float3 pos = particles[index].position;

	uint maxLife_life = particles[index].maxLife_life;
	float life = unpackHalfsRight(maxLife_life);
	float maxLife = unpackHalfsLeft(maxLife_life);
	float relLife = clamp(1.f - life / maxLife, 0.01f, 0.99f);

	float2 rotation; // Sin(angle), cos(angle).
	unpackHalfs(particles[index].sinAngle_cosAngle, rotation.x, rotation.y);

	float2 localPosition = IN.position.xy * 1.f;
	localPosition = float2(dot(localPosition, float2(rotation.y, -rotation.x)), dot(localPosition, rotation));
	pos += localPosition.x * camera.right.xyz + localPosition.y * camera.up.xyz;

	texture_atlas_cb atlas = cb.atlas;
	uint atlasIndex = cb.atlasProgressionOverLifetime.evaluate(4, relLife) * (atlas.getTotalNumCells() - 1);
	uint x = atlas.getX(atlasIndex);
	uint y = atlas.getY(atlasIndex);

	float invCols = atlas.getInvNumCols();
	float invRows = atlas.getInvNumRows();
	float2 uv0 = float2(x * invCols, y * invRows);
	float2 uv1 = float2((x + 1) * invCols, (y + 1) * invRows);


	vs_output OUT;
	OUT.position = mul(camera.viewProj, float4(pos, 1.f));
	OUT.uv = lerp(uv0, uv1, IN.position.xy * 0.5f + 0.5f);
	OUT.intensity = cb.intensityOverLifetime.evaluate(4, relLife);
	return OUT;
}

static float4 pixelShader(vs_output IN)
{
	float4 color = tex.Sample(texSampler, IN.uv);
	return color * color.a * IN.intensity;
}

#endif


// Simulation.
#define FIRE_PARTICLE_SYSTEM_COMPUTE_RS_CBV			0

// Rendering.
#define FIRE_PARTICLE_SYSTEM_RENDERING_RS_CBV		0
#define FIRE_PARTICLE_SYSTEM_RENDERING_RS_CAMERA	1
#define FIRE_PARTICLE_SYSTEM_RENDERING_RS_TEXTURE	2


