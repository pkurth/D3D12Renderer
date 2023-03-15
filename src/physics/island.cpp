#include "pch.h"
#include "island.h"
#include "core/cpu_profiling.h"

void buildIslands(memory_arena& arena, constraint_body_pair* bodyPairs, uint32 numBodyPairs, uint32 numRigidBodies, uint16 dummyRigidBodyIndex, const constraint_offsets& offsets)
{
	CPU_PROFILE_BLOCK("Build islands");

	uint32 islandCapacity = numBodyPairs;
	uint16* allIslands = arena.allocate<uint16>(islandCapacity);

	memory_marker marker = arena.getMarker();

	uint32 count = numRigidBodies + 1; // 1 for the dummy.

	uint16* numConstraintsPerBody = arena.allocate<uint16>(count, true);

	for (uint32 i = 0; i < numBodyPairs; ++i)
	{
		constraint_body_pair pair = bodyPairs[i];
		++numConstraintsPerBody[pair.rbA];
		++numConstraintsPerBody[pair.rbB];
	}

	uint16* offsetToFirstConstraintPerBody = arena.allocate<uint16>(count);

	uint16 currentOffset = 0;
	for (uint32 i = 0; i < count; ++i)
	{
		offsetToFirstConstraintPerBody[i] = currentOffset;
		currentOffset += numConstraintsPerBody[i];
	}

	struct body_pair_reference
	{
		uint16 otherBody;
		uint16 pairIndex;
	};

	body_pair_reference* pairReferences = arena.allocate<body_pair_reference>(numBodyPairs * 2);
	uint16* counter = arena.allocate<uint16>(count);
	memcpy(counter, offsetToFirstConstraintPerBody, sizeof(uint16) * count);

	for (uint32 i = 0; i < numBodyPairs; ++i)
	{
		constraint_body_pair pair = bodyPairs[i];
		pairReferences[counter[pair.rbA]++] = { pair.rbB, (uint16)i };
		pairReferences[counter[pair.rbB]++] = { pair.rbA, (uint16)i };
	}


	uint16* rbStack = arena.allocate<uint16>(count);
	uint32 stackPtr;

	bool* alreadyVisited = arena.allocate<bool>(count, true);
	bool* alreadyOnStack = arena.allocate<bool>(count, true);

	uint32 islandPtr = 0;

	for (uint16 rbIndexOuter = 0; rbIndexOuter < (uint16)numRigidBodies; ++rbIndexOuter)
	{
		if (alreadyVisited[rbIndexOuter] || rbIndexOuter == dummyRigidBodyIndex)
		{
			continue;
		}

		// Reset island.
		uint32 islandStart = islandPtr;

		rbStack[0] = rbIndexOuter;
		alreadyOnStack[rbIndexOuter] = true;

		stackPtr = 1;


		while (stackPtr != 0)
		{
			uint16 rbIndex = rbStack[--stackPtr];

			ASSERT(rbIndex != dummyRigidBodyIndex);
			ASSERT(!alreadyVisited[rbIndex]);
			alreadyVisited[rbIndex] = true;


			// Push connected bodies.
			uint32 startIndex = offsetToFirstConstraintPerBody[rbIndex];
			uint32 count = numConstraintsPerBody[rbIndex];
			for (uint32 i = startIndex; i < startIndex + count; ++i)
			{
				body_pair_reference ref = pairReferences[i];
				uint16 other = ref.otherBody;
				if (!alreadyOnStack[other] && other != dummyRigidBodyIndex) // Don't push dummy to stack. We don't want to grow islands over the dummy.
				{
					alreadyOnStack[other] = true;
					rbStack[stackPtr++] = other;
				}

				if (!alreadyVisited[other])
				{
					// Add constraint to island.
					ASSERT(islandPtr < islandCapacity);
					allIslands[islandPtr++] = ref.pairIndex;
				}

			}
		}

		// Process island.
		uint32 islandSize = islandPtr - islandStart;
		if (islandSize > 0)
		{
			uint16* islandPairs = allIslands + islandStart;
			std::sort(islandPairs, islandPairs + islandSize);


		}
	}


#if 0
	ASSERT(islandPtr == islandCapacity);
	for (uint32 i = 0; i < numRigidBodies; ++i)
	{
		ASSERT(alreadyVisited[i]);
	}
#endif

	arena.resetToMarker(marker);
}
