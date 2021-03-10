#pragma once

#include "math.h"
#include "random.h"
#include "dx_texture.h"
#include "material.h"
#include "render_pass.h"
#include "dx_pipeline.h"

struct dx_command_list;

struct particle_system
{
	void initialize(const std::string& shaderName, uint32 maxNumParticles, float emitRate);
	void update(float dt, const std::function<void(dx_command_list* cl)>& setUserResourcesFunction);

	void render(transparent_render_pass* renderPass, const trs& transform);

private:
	void setResources(dx_command_list* cl, const struct particle_sim_cb& cb, uint32 offset, const std::function<void(dx_command_list* cl)>& setUserResourcesFunction);
	uint32 getAliveListOffset(uint32 alive);
	uint32 getDeadListOffset();

	uint32 maxNumParticles;
	float emitRate;
	uint32 currentAlive = 0;

	ref<dx_buffer> particlesBuffer;
	ref<dx_buffer> listBuffer; // Counters, dead, alive 0, alive 1.
	ref<dx_buffer> dispatchBuffer;

	dx_mesh mesh;

	ref<struct particle_material> material;

	dx_pipeline emitPipeline;
	dx_pipeline simulatePipeline;

	uint32 index;
};



// Internal.
void initializeParticlePipeline();

