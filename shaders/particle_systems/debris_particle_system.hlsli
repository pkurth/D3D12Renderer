#include "material.hlsli"
#include "camera.hlsli"
#include "particles.hlsli"

struct debris_particle_data
{
	vec3 position;
	uint32 maxLife_life;
	vec3 velocity;
	uint32 sinAngle_cosAngle;
};

struct debris_simulation_cb
{
	vec4 emitPositions[4];

	vec3 cameraPosition;
	uint32 frameIndex;
};

#ifdef HLSL
#define particle_data debris_particle_data
#endif


#ifdef PARTICLE_SIMULATION

#define REQUIRES_SORTING

#define USER_PARTICLE_SIMULATION_RS \
	"RootConstants(num32BitConstants=20, b0)"

ConstantBuffer<debris_simulation_cb> cb		: register(b0);

static particle_data emitParticle(uint emitIndex)
{
	uint emitBatch = emitIndex / 256;
	emitIndex = emitIndex % 256;

	float i = saturate((float)emitIndex / 256.f) * 2.f * M_PI;
	float sinAngle, cosAngle;
	sincos(i, sinAngle, cosAngle);

	float3 position = float3(cosAngle, 0.f, sinAngle) * 0.3f;
	float maxLife = 1.5f;

	float3 velocity = normalize(position);

	position += cb.emitPositions[emitBatch].xyz;

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
		sortKey = 0;
		return false;
	}
	else
	{
		float maxLife = unpackHalfsLeft(particle.maxLife_life);
		float relLife = getRelLife(life, maxLife);

		particle.velocity.y -= 9.81f * dt;
		particle.position += particle.velocity * dt;

		if (particle.position.y < 2.f && particle.velocity.y < 0.f)
		{
			particle.position.y = 2.f;
			particle.velocity.y *= -0.5f;
		}

		particle.maxLife_life = packHalfs(maxLife, life);

		float3 V = particle.position - cb.cameraPosition;
		sortKey = dot(V, V);

		return true;
	}
}

#endif




#ifdef PARTICLE_RENDERING

#define USER_PARTICLE_RENDERING_RS \
    "CBV(b0, visibility=SHADER_VISIBILITY_VERTEX)"


struct vs_input
{
	float3 position			: POSITION;
};

struct vs_output
{
	float3 color			: COLOR;
	float4 position			: SV_Position;
};

ConstantBuffer<camera_cb> camera		: register(b0);

static vs_output vertexShader(vs_input IN, StructuredBuffer<particle_data> particles, uint index)
{
	float3 pos = particles[index].position;

	float2 rotation; // Sin(angle), cos(angle).
	unpackHalfs(particles[index].sinAngle_cosAngle, rotation.x, rotation.y);

	float size = 0.1f;
	float2 localPosition = IN.position.xy * size;
	localPosition = float2(dot(localPosition, float2(rotation.y, -rotation.x)), dot(localPosition, rotation));
	pos += localPosition.x * camera.right.xyz + localPosition.y * camera.up.xyz;

	vs_output OUT;
	OUT.position = mul(camera.viewProj, float4(pos, 1.f));
	OUT.color = particles[index].velocity;
	return OUT;
}

static float4 pixelShader(vs_output IN)
{
	return float4(IN.color, 1.f);
}

#endif


// Simulation.
#define DEBRIS_PARTICLE_SYSTEM_COMPUTE_RS_CBV				0

// Rendering.
#define DEBRIS_PARTICLE_SYSTEM_RENDERING_RS_CAMERA			0



