#include "camera.hlsli"

struct boid_particle_data
{
	vec3 position;
	uint32 maxLife_life;
	vec3 velocity;
	uint32 color;
};

struct boid_particle_settings
{
	float radius;
};

struct boid_simulation_cb
{
	vec3 emitPosition;
	uint32 frameIndex;
	float radius;
};

#ifdef HLSL
#define particle_data boid_particle_data

#include "material.hlsli"
#endif

#ifdef PARTICLE_SIMULATION

#define USER_PARTICLE_SIMULATION_RS \
	"RootConstants(num32BitConstants=5, b0)"

ConstantBuffer<boid_simulation_cb> cb		: register(b0);

static particle_data emitParticle(uint emitIndex)
{
	uint rng = initRand(emitIndex, cb.frameIndex);

	const float radius = cb.radius;

	float2 offset2D = getRandomPointOnDisk(rng, radius);
	float height = nextRand(rng) * 2.f + 3.f;
	float3 position = cb.emitPosition + float3(offset2D.x, height, offset2D.y);

	float maxLife = nextRand(rng) + 4.5f;

	float2 velocity = getRandomPointOnUnitDisk(rng) * (5.f + nextRand(rng) * 1.5f);

	particle_data particle = {
		position,
		packHalfs(maxLife, maxLife),
		float3(velocity.x, 0.f, velocity.y),
		packColor(0, 0, 255, 255)  //packColor(nextRand(rng), nextRand(rng), nextRand(rng), 1.f)
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
		float3 gravity = float3(0.f, -9.81f * dt, 0.f);
		particle.position = particle.position + 0.5f * gravity * dt + particle.velocity * dt;
		particle.velocity = particle.velocity + gravity;
		if (particle.position.y <= cb.emitPosition.y)
		{
			particle.position.y = cb.emitPosition.y;
			particle.velocity.y = 0.f;
		}

		float maxLife = unpackHalfsLeft(particle.maxLife_life);
		particle.maxLife_life = packHalfs(maxLife, life);

		return true;
	}
}

#endif


#ifdef PARTICLE_RENDERING

#include "brdf.hlsli"

#define USER_PARTICLE_RENDERING_RS \
    "CBV(b0), " \
	"DescriptorTable(SRV(t0, numDescriptors=3), visibility=SHADER_VISIBILITY_PIXEL), " \
	"StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR," \
        "visibility=SHADER_VISIBILITY_PIXEL)"


struct vs_input
{
	float3 position				: POSITION;
	float2 uv					: TEXCOORDS;
	float3 normal				: NORMAL;
	float3 tangent				: TANGENT;
};

struct vs_output
{
	float3 worldPosition		: POSITION;
	float3 normal				: NORMAL;
	nointerpolation uint color	: COLOR;
	float4 position				: SV_Position;
};

ConstantBuffer<camera_cb> camera		: register(b0);

TextureCube<float4> irradianceTexture	: register(t0);
TextureCube<float4> environmentTexture	: register(t1);
Texture2D<float2> brdf					: register(t2);

SamplerState clampSampler				: register(s0);


static vs_output vertexShader(vs_input IN, StructuredBuffer<particle_data> particles, uint index)
{
	uint maxLife_life = particles[index].maxLife_life;
	float life = unpackHalfsRight(maxLife_life);
	float maxLife = unpackHalfsLeft(maxLife_life);
	float relLife = clamp(1.f - life / maxLife, 0.01f, 0.99f);

	float scale = smoothstep(relLife, 0.f, 0.005f);
	scale = min(scale, smoothstep(1.f - relLife, 0.f, 0.005f));


	float3 backward = particles[index].velocity;
	backward.y = 0.f;
	backward = normalize(backward);
	float3 up = float3(0.f, 1.f, 0.f);
	float3 right = cross(up, backward);

	float3 localPosition = IN.position;
	float3 localNormal = IN.normal;

	float3 rotatedPosition = localPosition.x * right + localPosition.y * up + localPosition.z * backward;
	float3 rotatedNormal = localNormal.x * right + localNormal.y * up + localNormal.z * backward;

	float3 pos = particles[index].position + rotatedPosition * scale;

	vs_output OUT;
	OUT.worldPosition = pos;
	OUT.position = mul(camera.viewProj, float4(pos, 1.f));
	OUT.normal = rotatedNormal;
	OUT.color = particles[index].color;
	return OUT;
}

static float4 pixelShader(vs_output IN)
{
	surface_info surface;

	surface.albedo = unpackColor(IN.color);
	surface.N = normalize(IN.normal);

	surface.roughness = 0.6f;
	surface.roughness = clamp(surface.roughness, 0.01f, 0.99f);
	surface.metallic = 0.f;

	surface.emission = float3(0.f, 0.f, 0.f);

	surface.P = IN.worldPosition;
	float3 camToP = surface.P - camera.position.xyz;
	surface.V = -normalize(camToP);

	surface.inferRemainingProperties();


	float3 L = normalize(float3(1.f, 0.5f, -1.f));
	float3 radiance = float3(1.f, 1.f, 1.f) * 20.f;

	light_info light;
	light.initialize(surface, L, radiance);


	light_contribution totalLighting = { float3(0.f, 0.f, 0.f), float3(0.f, 0.f, 0.f) };
	totalLighting.add(calculateDirectLighting(surface, light), 1.f);


	ambient_factors factors = getAmbientFactors(surface);
	totalLighting.diffuse += diffuseIBL(factors.kd, surface, irradianceTexture, clampSampler);
	totalLighting.specular += specularIBL(factors.ks, surface, environmentTexture, brdf, clampSampler);


	return totalLighting.evaluate(surface.albedo);
}

#endif


// Simulation.
#define BOID_PARTICLE_SYSTEM_COMPUTE_RS_CBV			0

// Rendering.
#define BOID_PARTICLE_SYSTEM_RENDERING_RS_CAMERA	0
#define BOID_PARTICLE_SYSTEM_RENDERING_RS_PBR		1
