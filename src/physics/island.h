#pragma once

#include "constraints.h"

struct constraint_offsets
{
	// Must be in order.
	uint32 constraintOffsets[constraint_type_count];
};

void buildIslands(memory_arena& arena, constraint_body_pair* bodyPairs, uint32 numBodyPairs, uint32 numRigidBodies, uint16 dummyRigidBodyIndex, const constraint_offsets& offsets);
