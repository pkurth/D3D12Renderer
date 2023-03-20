#pragma once

#include "core/math.h"
#include "core/camera.h"

struct water_settings
{
	vec4 deepWaterColor = vec4(0.f, 0.241f, 0.799f, 0.76f);
	vec4 shallowWaterColor = vec4(0.120f, 0.546f, 0.941f, 0.176f);
	float uvScale = 0.5f;
	float shallowDepth = 3.f;
	float transitionStrength = 0.07f;
	float normalStrength = 1.f;
};

struct water_component
{
	void render(const render_camera& camera, struct transparent_render_pass* renderPass, vec3 positionOffset, vec2 scale, float dt, uint32 entityID = -1);

	water_settings settings;

private:
	float time = 0.f;
};

void initializeWaterPipelines();
