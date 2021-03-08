#pragma once

#include "math.h"
#include "random.h"
#include "dx_texture.h"
#include "material.h"
#include "render_pass.h"

struct particle_material : material_base
{
	void prepareForRendering(struct dx_command_list* cl);
	static void setupTransparentPipeline(dx_command_list* cl, const common_material_info& info);
};

struct particle_system
{
	void initialize(uint32 maxNumParticles, float emitRate);
	void update(float dt);

	void testRender(transparent_render_pass* renderPass);

private:
	void setResources(struct dx_command_list* cl);
	uint32 getAliveListOffset(uint32 alive);
	uint32 getDeadListOffset();

	uint32 maxNumParticles;
	float emitRate;
	uint32 currentAlive = 0;

	ref<dx_buffer> particlesBuffer;
	ref<dx_buffer> listBuffer; // Counters, dead, alive 0, alive 1.
	ref<dx_buffer> dispatchBuffer;

	dx_mesh mesh;

	ref<particle_material> material;

	uint32 index;
};



void initializeParticlePipeline();

