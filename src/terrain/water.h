#pragma once

#include "core/math.h"
#include "core/camera.h"

struct water_settings
{
	float normalStrength = 1.f;
};

struct water_component
{
	void render(const render_camera& camera, struct transparent_render_pass* renderPass, vec3 positionOffset, uint32 entityID = -1);

	water_settings settings;
};

void initializeWaterPipelines();
