#pragma once

#include "rendering/render_pass.h"
#include "terrain/terrain.h"

struct grass_settings
{
	static inline bool depthPrepass = true;

	float bladeHeight = 1.f;
	float bladeWidth = 0.05f; 
	uint32 numGrassBladesPerChunkDim = 350;

	float lodChangeStartDistance = 50.f;
	float lodChangeTransitionDistance = 75.f;

	float cullStartDistance = 350.f;
	float cullTransitionDistance = 50.f;
};

struct grass_component
{
	grass_component(grass_settings settings = {});

	void generate(struct compute_pass* computePass, const render_camera& camera, const terrain_component& terrain, vec3 positionOffset, float dt);
	void render(struct opaque_render_pass* renderPass, uint32 entityID = -1);

	grass_settings settings;

private:
	ref<dx_buffer> drawBuffer;
	ref<dx_buffer> countBuffer;
	ref<dx_buffer> bladeBufferLOD0;
	ref<dx_buffer> bladeBufferLOD1;

	float time = 0.f;
	float prevTime = 0.f;
	vec2 windDirection = normalize(vec2(1.f, 1.f));
};

void initializeGrassPipelines();
