#pragma once

#include "particles.h"
#include "fire_particle_system.hlsli"
#include "dx/dx_texture.h"
#include "rendering/material.h"


struct fire_particle_system : particle_system
{
	static void initializePipeline();

	fire_particle_system() {}
	fire_particle_system(uint32 maxNumParticles, float emitRate, const std::string& textureFilename, uint32 cols, uint32 rows);
	void initialize(uint32 maxNumParticles, float emitRate, const std::string& textureFilename, uint32 cols, uint32 rows);

	virtual void update(struct dx_command_list* cl, vec3 cameraPosition, float dt) override;
	void render(transparent_render_pass* renderPass);

	float emitRate;
	fire_particle_settings settings;

private:
	dx_texture_atlas atlas;
	texture_atlas_cb atlasCB;

	static dx_pipeline emitPipeline;
	static dx_pipeline simulatePipeline;
	static dx_pipeline renderPipeline;


	struct fire_material
	{
		dx_texture_atlas atlas;
		dx_dynamic_constant_buffer cbv;
	};

	struct fire_pipeline : particle_render_pipeline<fire_material>
	{
		using render_data_t = fire_material;

		PIPELINE_SETUP_DECL;
		PARTICLE_PIPELINE_RENDER_DECL;
	};
};
