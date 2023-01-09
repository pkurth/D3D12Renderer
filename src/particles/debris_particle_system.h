#pragma once

#include "particles.h"
#include "debris_particle_system.hlsli"
#include "dx/dx_texture.h"
#include "rendering/material.h"


struct debris_particle_system : particle_system
{
	static void initializePipeline();

	debris_particle_system() {}
	debris_particle_system(uint32 maxNumParticles);
	void initialize(uint32 maxNumParticles);

	void burst(vec3 position);

	virtual void update(struct dx_command_list* cl, const common_particle_simulation_data& common, float dt) override;
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
