#pragma once

#include "math.h"

struct shadow_map_viewport
{
	int32 cpuVP[4]; // x, y, width, height.
	vec4 shaderVP;
};

struct shadow_map_light_info
{
	shadow_map_viewport viewport; // Previous frame viewport. Gets updated by cache.
	bool lightMovedOrAppeared;
	bool geometryInRangeMoved;
	//uint64 dynamicGeometryHash;
	//uint64 prevFrameDynamicGeometryHash;
	//uint64 lightTransformHash;
	//uint64 prevFrameLightTransformHash;
};

void testShadowMapCache(shadow_map_light_info* infos, uint32 numInfos);
