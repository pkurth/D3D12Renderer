#pragma once

#include "particles.h"
#include "boid_particle_system.hlsli"
#include "dx/dx_texture.h"
#include "rendering/material.h"


struct boid_particle_system : particle_system
{
	static void initializePipeline();

	boid_particle_system() {}
	boid_particle_system(uint32 maxNumParticles, float emitRate);
	void initialize(uint32 maxNumParticles, float emitRate);

	virtual void update(struct dx_command_list* cl, vec3 cameraPosition, float dt) override;
	void render(transparent_render_pass* renderPass);

	float emitRate;
	boid_particle_settings settings;

private:
	static dx_pipeline emitPipeline;
	static dx_pipeline simulatePipeline;
	static dx_pipeline renderPipeline;

	ref<multi_mesh> cartoonMesh;
	dx_vertex_buffer_group_view skinnedVertexBuffer;
	float time = 0.f;

	struct boid_material
	{
		
	};

	struct boid_pipeline : particle_render_pipeline<boid_material>
	{
		using render_data_t = boid_material;

		PIPELINE_SETUP_DECL;
		PARTICLE_PIPELINE_RENDER_DECL;
	};
};

