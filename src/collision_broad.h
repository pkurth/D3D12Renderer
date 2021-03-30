#pragma once

#include "bounding_volumes.h"


struct broadphase_collision
{
	// Indices of the colliders in the scene.
	uint16 colliderA;
	uint16 colliderB;
};

uint32 broadphase(struct scene& appScene, uint32 sortingAxis, bounding_box* worldSpaceAABBs, broadphase_collision* outOverlaps, void* scratchMemory);






// Internal.
struct sap_endpoint_indirection_component
{
	uint16 startEndpoint;
	uint16 endEndpoint;
};

void onColliderAdded(entt::registry& registry, entt::entity entity);
void onColliderRemoved(entt::registry& registry, entt::entity entity);
