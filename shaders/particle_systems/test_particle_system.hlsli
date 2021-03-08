


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
		float3(rng * 3, 0.f, 0.f),
		5.f,
		float3(0.f, 0.f, 0.f),
		rng
	};

	return particle;
}

static void simulateParticle(inout particle_data particle, float dt)
{
	float3 gravity = float3(0.f, -9.81f * dt, 0.f);
	particle.position = particle.position + 0.5f * gravity * dt + particle.velocity * dt;
	particle.velocity = particle.velocity + gravity;
}

