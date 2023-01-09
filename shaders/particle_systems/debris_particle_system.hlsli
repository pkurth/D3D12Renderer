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

	mat4 cameraVP;
	vec4 cameraProjectionParams;

	vec3 cameraPosition;
	uint32 frameIndex;
};

#ifdef HLSL
#define particle_data debris_particle_data
#endif


#ifdef PARTICLE_SIMULATION

#include "normal.hlsli"
#include "random.hlsli"

#define REQUIRES_SORTING

#define USER_PARTICLE_SIMULATION_RS \
	"CBV(b0), " \
	"DescriptorTable(SRV(t0, numDescriptors=2)), " \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_BORDER," \
        "addressV = TEXTURE_ADDRESS_BORDER," \
        "addressW = TEXTURE_ADDRESS_BORDER," \
		"borderColor = STATIC_BORDER_COLOR_OPAQUE_WHITE," \
        "filter = FILTER_MIN_MAG_MIP_POINT)"

ConstantBuffer<debris_simulation_cb> cb	: register(b0);
Texture2D<float> depthBuffer			: register(t0);
Texture2D<float2> sceneNormals			: register(t1);
SamplerState pointSampler				: register(s0);

static particle_data emitParticle(uint emitIndex)
{
	uint rng = initRand(emitIndex, cb.frameIndex);

	uint emitBatch = emitIndex / 256;
	emitIndex = emitIndex % 256;

	float2 randomOnDisk = getRandomPointOnUnitDisk(rng) * 0.3f;
	float3 position = float3(randomOnDisk.x, 0.f, randomOnDisk.y);
	float maxLife = 1.5f;

	float3 velocity = normalize(position);

	position += cb.emitPositions[emitBatch].xyz;

	float i = (float)emitIndex * 2.f * M_PI;
	float sinAngle, cosAngle;
	sincos(i, sinAngle, cosAngle);

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

		float4 p = mul(cb.cameraVP, float4(particle.position, 1.f));
		p.xyz /= p.w;
		float2 uv = p.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

		float particleDepth = depthBufferDepthToEyeDepth(p.z, cb.cameraProjectionParams);
		float sceneDepth = depthBufferDepthToEyeDepth(isSaturated(p.z) ? depthBuffer.SampleLevel(pointSampler, uv, 0) : 1.f, cb.cameraProjectionParams); // Due to the border sampler, this defaults to 1 (infinite depth) outside the image.


		if (particleDepth > sceneDepth - 0.05f)
		{
			float3 sceneNormal = normalize(unpackNormal(sceneNormals.SampleLevel(pointSampler, uv, 0)));

			float VdotN = dot(particle.velocity, sceneNormal);
			if (VdotN < 0.f)
			{
				particle.velocity = reflect(particle.velocity, sceneNormal) * 0.5f;
				//particle.position += sceneNormal * (particleDepth - sceneDepth);
			}
		}

		particle.position += particle.velocity * dt;


		if (any(isnan(particle.position)))
		{
			life = maxLife;
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
#define DEBRIS_PARTICLE_SYSTEM_COMPUTE_RS_TEXTURES			1

// Rendering.
#define DEBRIS_PARTICLE_SYSTEM_RENDERING_RS_CAMERA			0



