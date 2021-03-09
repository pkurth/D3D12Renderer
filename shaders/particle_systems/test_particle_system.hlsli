
struct particle_data
{
	vec3 position;
	float life;
	vec3 velocity;
	uint32 random;
};

static particle_data emitParticle(uint rng)
{
	particle_data particle = {
		float3(nextRand(rng), nextRand(rng), nextRand(rng)) * 15.f - 7.5f,
		nextRand(rng) * 5.f,
		float3(nextRand(rng), nextRand(rng), nextRand(rng)) * 15.f - 7.5f,
		rng
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

	float3 gravity = float3(0.f, -9.81f * dt, 0.f);
	particle.position = particle.position + 0.5f * gravity * dt + particle.velocity * dt;
	particle.velocity = particle.velocity + gravity;

	return true;
}

