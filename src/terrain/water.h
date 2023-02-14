#pragma once

#include "core/math.h"
#include "core/camera.h"

struct water_settings
{
	vec3 deepWaterColor = vec3(0.184f, 0.184f, 0.838f);
	vec3 shallowWaterColor = vec3(0.748f, 0.790f, 0.885f);
	float transition = 3.f;
	float normalStrength = 1.f;
};

struct water_component
{
	void render(const render_camera& camera, struct transparent_render_pass* renderPass, vec3 positionOffset, uint32 entityID = -1);

	water_settings settings;
};

void initializeWaterPipelines();
