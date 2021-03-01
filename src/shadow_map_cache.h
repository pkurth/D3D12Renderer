#pragma once

#include "math.h"

struct shadow_map_viewport
{
	uint16 x, y;
	uint16 size;
};

enum shadow_map_command
{
	shadow_map_command_use_cached,
	shadow_map_command_use_static_cache,
	shadow_map_command_from_scratch,
};

// Call after light view-projection-matrices are computed.
uint64 getLightMovementHash(const struct directional_light& dl); // Whole light.
uint64 getLightMovementHash(const struct directional_light& dl, uint32 cascadeIndex);
uint64 getLightMovementHash(const struct spot_light_cb& sl);
uint64 getLightMovementHash(const struct point_light_cb& pl);


std::pair<shadow_map_viewport, shadow_map_command> assignShadowMapViewport(uint32 uniqueLightID, uint64 lightMovementHash, uint64 geometryMovementHash, uint32 size);
void updateShadowMapAllocationVisualization();
