#pragma once

#include "rendering/particles.h"
#include "fire_particle_system.hlsli"
#include "smoke_particle_system.hlsli"
#include "boid_particle_system.hlsli"
#include "dx/dx_texture.h"
#include "rendering/material.h"


void loadAllParticleSystemPipelines();



struct fire_particle_system : particle_system
{
	static void initializePipeline();

	void initialize(uint32 maxNumParticles, float emitRate, const std::string& textureFilename, uint32 cols, uint32 rows);

	void update(float dt);
	void render(transparent_render_pass* renderPass);

	fire_particle_cb settings;

protected:
	void setSimulationParameters(dx_command_list* cl) override;

private:
	static dx_pipeline emitPipeline;
	static dx_pipeline simulatePipeline;
	static dx_pipeline renderPipeline;


	struct fire_material
	{
		dx_texture_atlas atlas;
		dx_dynamic_constant_buffer settingsCBV;
	};

	struct fire_pipeline : particle_render_pipeline<fire_pipeline>
	{
		using material_t = fire_material;

		static void setupCommon(dx_command_list* cl, const common_material_info& materialInfo);
		static void render(dx_command_list* cl, const mat4& viewProj, const particle_render_command<fire_pipeline>& rc);
	};

	fire_material material;
};



struct smoke_particle_system : particle_system
{
	static void initializePipeline();

	void initialize(uint32 maxNumParticles, float emitRate, const std::string& textureFilename, uint32 cols, uint32 rows);

	void update(float dt);
	void render(transparent_render_pass* renderPass);

	smoke_particle_cb settings;

protected:
	void setSimulationParameters(dx_command_list* cl) override;

private:
	static dx_pipeline emitPipeline;
	static dx_pipeline simulatePipeline;
	static dx_pipeline renderPipeline;


	struct smoke_material
	{
		dx_texture_atlas atlas;
		dx_dynamic_constant_buffer settingsCBV;
	};

	struct smoke_pipeline : particle_render_pipeline<smoke_pipeline>
	{
		using material_t = smoke_material;

		static void setupCommon(dx_command_list* cl, const common_material_info& materialInfo);
		static void render(dx_command_list* cl, const mat4& viewProj, const particle_render_command<smoke_pipeline>& rc);
	};

	smoke_material material;
};



struct boid_particle_system : particle_system
{
	static void initializePipeline();

	void initialize(uint32 maxNumParticles, float emitRate);

	void update(float dt);
	void render(transparent_render_pass* renderPass);

	boid_particle_cb settings;

protected:
	void setSimulationParameters(dx_command_list* cl) override;

private:
	static dx_pipeline emitPipeline;
	static dx_pipeline simulatePipeline;
	static dx_pipeline renderPipeline;

	ref<composite_mesh> cartoonMesh;
	vertex_buffer_group skinnedVertexBuffer;
	float time = 0.f;

	struct boid_material
	{
		dx_dynamic_constant_buffer settingsCBV;
	};

	struct boid_pipeline : particle_render_pipeline<boid_pipeline>
	{
		using material_t = boid_material;

		static void setupCommon(dx_command_list* cl, const common_material_info& materialInfo);
		static void render(dx_command_list* cl, const mat4& viewProj, const particle_render_command<boid_pipeline>& rc);
	};

	boid_material material;
};
