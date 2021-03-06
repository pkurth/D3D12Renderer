#include "pch.h"
#include "particles.h"
#include "dx_pipeline.h"
#include "dx_renderer.h"
#include "camera.h"
#include "dx_context.h"
#include "material.hlsli"

static dx_pipeline flatPipeline;

static D3D12_INPUT_ELEMENT_DESC inputLayout[] = 
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

	{ "INSTANCEPOSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
	{ "INSTANCECOLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
	// We are using the semantic index here to append 0 or 1 to the name.
	{ "INSTANCETEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
	{ "INSTANCETEXCOORDS", 1, DXGI_FORMAT_R32G32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
};

struct particle_vertex
{
	vec3 position;
	vec2 uv;
};

struct particle_instance_data
{
	vec3 position;
	uint32 color;
	vec2 uv0;
	vec2 uv1;
};


void particle_system::initialize(uint32 numParticles)
{
	particles.reserve(numParticles);
	particleSpawnAccumulator = 0.f;

	rng = { 512839 };

	material = make_ref<particle_material>();
}

void particle_system::update(float dt)
{
	uint32 numParticles = (uint32)particles.size();
	for (uint32 i = 0; i < numParticles; ++i)
	{
		particle_data& p = particles[i];
		p.timeAlive += dt;
		if (p.timeAlive >= p.maxLifetime)
		{
			std::swap(p, particles[numParticles - 1]);
			--numParticles;
			--i;
		}
	}
	particles.resize(numParticles);

	vec3 gravity(0.f, -9.81f * gravityFactor * dt, 0.f);
	//uint32 textureSlices = textureAtlas.slicesX * textureAtlas.slicesY;
	for (uint32 i = 0; i < numParticles; ++i)
	{
		particle_data& p = particles[i];
		p.position = p.position + 0.5f * gravity * dt + p.velocity * dt;
		p.velocity = p.velocity + gravity;
		float relLifetime = p.timeAlive / p.maxLifetime;
		p.color = color.interpolate(relLifetime, p.color);
		//if (textureAtlas.texture)
		//{
		//	uint32 index = (uint32)(relLifetime * textureSlices);
		//	index = min(index, textureSlices - 1);
		//	auto [uv0, uv1] = textureAtlas.getUVs(index);
		//	p.uv0 = uv0;
		//	p.uv1 = uv1;
		//}
	}

	particleSpawnAccumulator += spawnRate * dt;
	uint32 spaceLeft = (uint32)particles.capacity() - numParticles;
	uint32 numNewParticles = min((uint32)particleSpawnAccumulator, spaceLeft);

	particleSpawnAccumulator -= numNewParticles;

	for (uint32 i = 0; i < numNewParticles; ++i)
	{
		particle_data p;
		p.position = spawnPosition;
		p.timeAlive = 0.f;
		p.velocity = startVelocity.start(rng);
		p.color = color.start(rng);
		p.maxLifetime = maxLifetime.start(rng);
		//if (textureAtlas.texture)
		//{
		//	auto [uv0, uv1] = textureAtlas.getUVs(0, 0);
		//	p.uv0 = uv0;
		//	p.uv1 = uv1;
		//}

		particles.push_back(p);
	}
}

void particle_system::render(const render_camera& camera, transparent_render_pass* renderPass)
{
	std::vector<particle_instance_data> instanceData(particles.size());
	for (uint32 i = 0; i < (uint32)particles.size(); ++i)
	{
		instanceData[i].position = particles[i].position;
		instanceData[i].color = packColor(particles[i].color);
		instanceData[i].uv0 = particles[i].uv0;
		instanceData[i].uv1 = particles[i].uv1;
	}

	float size = 0.1f;
	vec3 right = camera.rotation * vec3(0.5f * size, 0.f, 0.f);
	vec3 up = camera.rotation * vec3(0.f, 0.5f * size, 0.f);

	particle_vertex vertices[] =
	{
		{ -right - up, vec2(0.f, 0.f) },
		{  right - up, vec2(1.f, 0.f) },
		{ -right + up, vec2(0.f, 1.f) },
		{  right + up, vec2(1.f, 1.f) },
	};

	if (particles.size())
	{
		dx_dynamic_vertex_buffer tmpVertexBuffer = dxContext.createDynamicVertexBuffer(vertices, arraysize(vertices));
		dx_dynamic_vertex_buffer tmpInstanceBuffer = dxContext.createDynamicVertexBuffer(instanceData.data(), (uint32)instanceData.size());

		renderPass->renderParticles(tmpVertexBuffer, tmpInstanceBuffer, mat4::identity, material, (uint32)particles.size());
	}
}

void particle_material::initializePipeline()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout)
		.renderTargets(dx_renderer::transparentLightPassFormats, arraysize(dx_renderer::transparentLightPassFormats), dx_renderer::hdrDepthStencilFormat)
		.additiveBlending(0)
		.depthSettings(true, false);

	flatPipeline = createReloadablePipeline(desc, { "particles_vs", "particles_flat_ps" });
}

void particle_material::prepareForRendering(dx_command_list* cl)
{
}

void particle_material::setupTransparentPipeline(dx_command_list* cl, const common_material_info& info)
{
	cl->setPipelineState(*flatPipeline.pipeline);
	cl->setGraphicsRootSignature(*flatPipeline.rootSignature);
}
