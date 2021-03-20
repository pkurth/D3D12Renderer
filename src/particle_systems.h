#pragma once

#include "particles.h"
#include "fire_particle_system.hlsli"
#include "boid_particle_system.hlsli"
#include "dx_texture.h"
#include "material.h"


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


	struct fire_material : material_base
	{
		dx_texture_atlas atlas;
		dx_dynamic_constant_buffer settingsCBV;

		static void setupTransparentPipeline(dx_command_list* cl, const common_material_info& materialInfo);
		void prepareForRendering(dx_command_list* cl);
	};

	ref<fire_material> material;
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

	struct boid_material : material_base
	{
		dx_dynamic_constant_buffer settingsCBV;

		static void setupTransparentPipeline(dx_command_list* cl, const common_material_info& materialInfo);
		void prepareForRendering(dx_command_list* cl);
	};

	ref<boid_material> material;
};
