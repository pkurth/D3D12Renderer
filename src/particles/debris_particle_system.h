#pragma once

#include "particles.h"
#include "debris_particle_system.hlsli"
#include "dx/dx_texture.h"
#include "rendering/material.h"


struct debris_particle_system : particle_system
{
	static void initializePipeline();

	void initialize(uint32 maxNumParticles);

	void burst(vec3 position);

	void update(vec3 cameraPosition, float dt);
	void render(transparent_render_pass* renderPass);

private:
	static dx_pipeline emitPipeline;
	static dx_pipeline simulatePipeline;
	static dx_pipeline renderPipeline;


	struct debris_material
	{
	};

	struct debris_pipeline : particle_render_pipeline<debris_material>
	{
		using render_data_t = debris_material;

		PIPELINE_SETUP_DECL;
		PARTICLE_PIPELINE_RENDER_DECL;
	};


	struct debris_burst
	{
		vec3 position;
	};

	std::vector<debris_burst> bursts;
};
