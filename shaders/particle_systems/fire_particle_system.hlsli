
struct fire_particle_data
{
	vec3 position;
	float life;
	vec3 velocity;
	float maxLife;
};

defineSpline(float, 8)

struct fire_particle_cb
{
	vec3 emitPosition;
	uint32 frameIndex;
	spline(float, 8) lifeScaleFromDistance;
};

#define USER_PARTICLES_RS \
	"CBV(b0)"

#define FIRE_PARTICLE_SYSTEM_RS_CBV		0

#ifdef HLSL

#define particle_data fire_particle_data

ConstantBuffer<fire_particle_cb> cb		: register(b0);

static particle_data emitParticle(uint emitIndex)
{
	uint rng = initRand(emitIndex, cb.frameIndex);

	const float radius = 4.f;

	float3 offset = float3(nextRand(rng), 0.f, nextRand(rng)) * (radius * 2.f) - radius;
	offset.y = nextRand(rng) * 0.5f;
	float3 position = cb.emitPosition + offset;
	float3 velocity = float3(nextRand(rng), nextRand(rng) * 2.f + 4.f, nextRand(rng));

	float distance = saturate(length(offset.xz) / radius);
	float lifeScale = cb.lifeScaleFromDistance.evaluate(8, distance);

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

