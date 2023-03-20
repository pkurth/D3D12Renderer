#pragma once

#include "particles.h"
#include "smoke_particle_system.hlsli"
#include "dx/dx_texture.h"
#include "rendering/material.h"


struct smoke_particle_system : particle_system
{
	static void initializePipeline();

	smoke_particle_system() {}
	smoke_particle_system(uint32 maxNumParticles, float emitRate, const std::string& textureFilename, uint32 cols, uint32 rows);
	void initialize(uint32 maxNumParticles, float emitRate, const std::string& textureFilename, uint32 cols, uint32 rows);

	virtual void update(struct dx_command_list* cl, const common_particle_simulation_data& common, float dt) override;
	void render(transparent_render_pass* renderPass);

	float emitRate;
	smoke_particle_settings settings;

private:
	texture_atlas_cb atlasCB;
	dx_texture_atlas atlas;

	static dx_pipeline emitPipeline;
	static dx_pipeline simulatePipeline;
	static dx_pipeline renderPipeline;


	struct smoke_material
	{
		dx_texture_atlas atlas;
		dx_dynamic_constant_buffer cbv;
	};

	struct smoke_pipeline : particle_render_pipeline<smoke_material>
	{
		PIPELINE_SETUP_DECL;
		PARTICLE_PIPELINE_RENDER_DECL(smoke_material);
	};
};

