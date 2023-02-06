#pragma once

#include "rendering/render_pass.h"
#include "terrain/terrain.h"

struct grass_settings
{
	float bladeHeight = 1.f;
	float bladeWidth = 0.2f;
	float footprint = 0.15f;
};

struct grass_component
{
	grass_component();

	void generate(const render_camera& camera, const terrain_component& terrain, vec3 positionOffset);
	void render(struct ldr_render_pass* renderPass);

	grass_settings settings;

private:
	ref<dx_buffer> drawBuffer;
	ref<dx_buffer> countBuffer;
	ref<dx_buffer> bladeBufferLOD0;
	ref<dx_buffer> bladeBufferLOD1;
};

void initializeGrassPipelines();
