#include "material.hlsli"
#include "camera.hlsli"

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
	// Simulation.
	vec3 emitPosition;
	uint32 frameIndex; 
	spline(float, 8) lifeScaleFromDistance;

	// Rendering.
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
	float relLife			: RELLIFE;
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

	float relLife = 1.f - saturate(particles[index].life / particles[index].maxLife);
	relLife = sqrt(relLife);

	texture_atlas_cb atlas = cb.atlas;
	uint atlasIndex = relLife * atlas.getTotalNumCells();
	uint x = atlas.getX(atlasIndex);
	uint y = atlas.getY(atlasIndex);

	float2 localPosition = IN.position.xy / saturate(smoothstep(relLife, 0.f, 0.3f) + 0.8f);
	pos += localPosition.x * camera.right.xyz + localPosition.y * camera.up.xyz;

	float invCols = atlas.getInvNumCols();
	float invRows = atlas.getInvNumRows();
	float2 uv0 = float2(x * invCols, y * invRows);
	float2 uv1 = float2((x + 1) * invCols, (y + 1) * invRows);


	vs_output OUT;
	OUT.position = mul(camera.viewProj, float4(pos, 1.f));
	OUT.uv = lerp(uv0, uv1, IN.position.xy * 0.5f + 0.5f);
	OUT.relLife = relLife;
	return OUT;
}

static float4 pixelShader(vs_output IN)
{
	float4 color = tex.Sample(texSampler, IN.uv);
	return color * color.a * smoothstep(IN.relLife, 0.f, 0.1f) * 1.f;
}

#endif


// Simulation.
#define FIRE_PARTICLE_SYSTEM_COMPUTE_RS_CBV			0

// Rendering.
#define FIRE_PARTICLE_SYSTEM_RENDERING_RS_CBV		0
#define FIRE_PARTICLE_SYSTEM_RENDERING_RS_CAMERA	1
#define FIRE_PARTICLE_SYSTEM_RENDERING_RS_TEXTURE	2


