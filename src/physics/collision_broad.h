#pragma once

#include "bounding_volumes.h"
#include "scene/scene.h"


struct broadphase_collision
{
	// Indices of the colliders in the scene.
	uint16 colliderA;
	uint16 colliderB;
};

uint32 broadphase(struct game_scene& scene, uint32 sortingAxis, bounding_box* worldSpaceAABBs, broadphase_collision* outOverlaps);






// Internal.
struct sap_endpoint_indirection_component
{
	uint16 startEndpoint;
	uint16 endEndpoint;
};

void onColliderAdded(entt::registry& registry, entt::entity entity);
void onColliderRemoved(entt::registry& registry, entt::entity entity);
