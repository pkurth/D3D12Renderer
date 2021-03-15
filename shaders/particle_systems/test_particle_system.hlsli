
struct test_particle_data
{
	vec3 position;
	float life;
	vec3 velocity;
	float maxLife;
};

struct test_particle_cb
{
	vec3 emitPosition;
	uint32 frameIndex;
};

#define USER_PARTICLES_RS \
	"RootConstants(num32BitConstants=4, b0)"

#define TEST_PARTICLE_SYSTEM_RS_CBV		0

#ifdef HLSL

#define particle_data test_particle_data

ConstantBuffer<test_particle_cb> cb		: register(b0);

static particle_data emitParticle(uint emitIndex)
{
	uint rng = initRand(emitIndex, cb.frameIndex);

	const float radius = 4.f;

	float3 offset = float3(nextRand(rng), 0.f, nextRand(rng)) * (radius * 2.f) - radius;
	offset.y = nextRand(rng) * 0.5f;
	float3 position = cb.emitPosition + offset;
	float3 velocity = float3(nextRand(rng), nextRand(rng) * 2.f + 4.f, nextRand(rng));

	float distance = saturate(1.f - length(offset.xz) / radius);
	float lifeScale = distance * distance * distance;

	float maxLife = nextRand(rng) * 5.f * lifeScale + 3.f;

	particle_data particle = {
		position,
		maxLife,
		velocity,
		maxLife
	};

	return particle;
}

static bool simulateParticle(inout particle_data particle, float dt)
{
	particle.life -= dt;
	if (particle.life <= 0)
	{
		return false;
	}
	else
	{
		float3 gravity = float3(0.f, -1.f * dt, 0.f);
		particle.position = particle.position + 0.5f * gravity * dt + particle.velocity * dt;
		particle.velocity = particle.velocity + gravity;

		return true;
	}
}

#endif

