#ifndef LIGHTING_HLSLI
#define LIGHTING_HLSLI

#include "light_source.hlsli"
#include "light_probe.hlsli"

struct lighting_cb
{
	directional_light_cb sun;
	light_probe_grid_cb lightProbeGrid;

	vec2 shadowMapTexelSize;
	float globalIlluminationIntensity;
	uint32 useRaytracedGlobalIllumination;
};

#endif
