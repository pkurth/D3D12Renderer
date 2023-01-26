#pragma once

#include "collision_narrow.h"
#include "collision_broad.h"
#include "terrain/heightmap_collider.h"


narrowphase_result heightmapCollision(const heightmap_collider_component& heightmap, 
	const collider_union* worldSpaceColliders, const bounding_box* worldSpaceAABBs, uint32 numColliders,
	collision_contact* outContacts, constraint_body_pair* outBodyPairs, // result.numContacts many.
	collider_pair* outColliderPairs, uint8* outContactCountPerCollision, // result.numCollisions many.
	memory_arena& arena, uint16 dummyRigidBodyIndex);

