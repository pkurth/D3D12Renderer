#pragma once

#include "bounding_volumes.h"
#include "scene/scene.h"
#include "core/memory.h"


struct collider_pair
{
	// Indices of the colliders in the scene.
	uint16 colliderA;
	uint16 colliderB;
};

uint32 broadphase(struct game_scene& scene, bounding_box* worldSpaceAABBs, memory_arena& arena, collider_pair* outOverlaps);






// Internal.
struct sap_endpoint_indirection_component
{
	uint16 startEndpoint;
	uint16 endEndpoint;
};
