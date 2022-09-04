#include "pch.h"
#include "collision_broad.h"
#include "scene/scene.h"
#include "physics.h"
#include "core/cpu_profiling.h"

#include "bounding_volumes_simd.h"

struct sap_endpoint
{
	float value;
	entity_handle entity = entt::null;
	bool start;
	uint16 colliderIndex; // Set each frame.

	sap_endpoint(entity_handle entity, bool start) : entity(entity), start(start) { }
	sap_endpoint(const sap_endpoint&) = default;
};

struct sap_context
{
	std::vector<sap_endpoint> endpoints;
	uint32 sortingAxis = 0;
};


void addColliderToBroadphase(scene_entity entity)
{
	sap_context& context = createOrGetContextVariable<sap_context>(*entity.registry);

	sap_endpoint_indirection_component endpointIndirection;

	endpointIndirection.startEndpoint = (uint16)context.endpoints.size();
	context.endpoints.emplace_back(entity.handle, true);

	endpointIndirection.endEndpoint = (uint16)context.endpoints.size();
	context.endpoints.emplace_back(entity.handle, false);

	entity.addComponent<sap_endpoint_indirection_component>(endpointIndirection);
}

static void removeEndpoint(uint16 endpointIndex, entt::registry& registry, sap_context& context)
{
	sap_endpoint last = context.endpoints.back();
	context.endpoints[endpointIndex] = last;

	// Point moved entity to correct slot.
	sap_endpoint_indirection_component& in = registry.get<sap_endpoint_indirection_component>(last.entity);
		
	if (last.start) 
	{ 
		in.startEndpoint = endpointIndex; 
	}
	else 
	{ 
		in.endEndpoint = endpointIndex; 
	}

	context.endpoints.pop_back();
}

void removeColliderFromBroadphase(scene_entity entity)
{
	sap_endpoint_indirection_component& endpointIndirection = entity.getComponent<sap_endpoint_indirection_component>();

	sap_context& context = getContextVariable<sap_context>(*entity.registry);

	removeEndpoint(endpointIndirection.startEndpoint, *entity.registry, context);
	removeEndpoint(endpointIndirection.endEndpoint, *entity.registry, context);

	if (entity.hasComponent<sap_endpoint_indirection_component>())
	{
		entity.removeComponent<sap_endpoint_indirection_component>();
	}
}

void clearBroadphase(game_scene& scene)
{
	auto c = scene.registry.ctx();
	if (sap_context* context = c.find<sap_context>())
	{
		context->endpoints.clear();
		context->sortingAxis = 0;
	}
}

static uint32 determineOverlapsScalar(const sap_endpoint* endpoints, uint32 numEndpoints, const bounding_box* worldSpaceAABBs, uint32 numColliders, memory_arena& arena,
	collider_pair* outCollisions)
{
	CPU_PROFILE_BLOCK("Determine overlaps");

	uint32 numCollisions = 0;

#define CACHE_AABBS 1


	uint32 activeListCapacity = numColliders; // Conservative estimate.

	uint32 numActive = 0;
	uint16* activeList = arena.allocate<uint16>(activeListCapacity);

#if CACHE_AABBS
	bounding_box* activeBBs = arena.allocate<bounding_box>(activeListCapacity);
#endif

	uint16* positionInActiveList = arena.allocate<uint16>(numColliders);

	uint32 maxNumActive = 0;

	for (uint32 i = 0; i < numEndpoints; ++i)
	{
		sap_endpoint ep = endpoints[i];
		if (ep.start)
		{
			const bounding_box& a = worldSpaceAABBs[ep.colliderIndex];

			for (uint32 active = 0; active < numActive; ++active)
			{
#if CACHE_AABBS
				const bounding_box& b = activeBBs[active];
#else
				const bounding_box& b = worldSpaceAABBs[activeList[active]];
#endif

				if (aabbVsAABB(a, b))
				{
					outCollisions[numCollisions++] = { ep.colliderIndex, activeList[active] };
				}
			}

			assert(ep.colliderIndex < numColliders);
			positionInActiveList[ep.colliderIndex] = numActive;

#if CACHE_AABBS
			activeBBs[numActive] = worldSpaceAABBs[ep.colliderIndex];
#endif

			activeList[numActive++] = ep.colliderIndex;

			maxNumActive = max(maxNumActive, numActive);
		}
		else
		{
			uint16 pos = positionInActiveList[ep.colliderIndex];

			--numActive;

			uint16 lastColliderInActiveList = activeList[numActive];
			positionInActiveList[lastColliderInActiveList] = pos;

			activeList[pos] = activeList[numActive];

#if CACHE_AABBS
			activeBBs[pos] = activeBBs[numActive];
#endif
		}
	}

	assert(numActive == 0);

	CPU_PROFILE_STAT("Max num active in SAP", maxNumActive);

	return numCollisions;

#undef CACHE_AABBS
}

static uint32 determineOverlapsSIMD(const sap_endpoint* endpoints, uint32 numEndpoints, const bounding_box* worldSpaceAABBs, uint32 numColliders, memory_arena& arena,
	collider_pair* outCollisions)
{
	CPU_PROFILE_BLOCK("Determine overlaps SIMD");

#define COLLISION_SIMD_WIDTH 8u

#if COLLISION_SIMD_WIDTH == 4
	typedef w4_float w_float;
	typedef w4_int w_int;
#elif COLLISION_SIMD_WIDTH == 8 && defined(SIMD_AVX_2)
	typedef w8_float w_float;
	typedef w8_int w_int;
#endif

	typedef wN_vec2<w_float> w_vec2;
	typedef wN_vec3<w_float> w_vec3;
	typedef wN_vec4<w_float> w_vec4;
	typedef wN_quat<w_float> w_quat;
	typedef wN_mat2<w_float> w_mat2;
	typedef wN_mat3<w_float> w_mat3;

	typedef wN_bounding_box<w_float> w_bounding_box;

	struct soa_bounding_box
	{
		float minX[COLLISION_SIMD_WIDTH];
		float minY[COLLISION_SIMD_WIDTH];
		float minZ[COLLISION_SIMD_WIDTH];
		float maxX[COLLISION_SIMD_WIDTH];
		float maxY[COLLISION_SIMD_WIDTH];
		float maxZ[COLLISION_SIMD_WIDTH];
	};

	uint32 numCollisions = 0;


	uint32 activeListCapacity = alignTo(numColliders, COLLISION_SIMD_WIDTH); // Conservative estimate.

	uint32 numActive = 0;
	uint16* activeList = arena.allocate<uint16>(activeListCapacity);

	soa_bounding_box* activeBBs = arena.allocate<soa_bounding_box>(activeListCapacity / COLLISION_SIMD_WIDTH);

	uint16* positionInActiveList = arena.allocate<uint16>(numColliders);

	uint32 maxNumActive = 0;

	for (uint32 i = 0; i < numEndpoints; ++i)
	{
		sap_endpoint ep = endpoints[i];
		if (ep.start)
		{
			const bounding_box& a = worldSpaceAABBs[ep.colliderIndex];

			w_bounding_box wA = { w_vec3(a.minCorner.x, a.minCorner.y, a.minCorner.z), w_vec3(a.maxCorner.x, a.maxCorner.y, a.maxCorner.z) };
			uint32 count = bucketize(numActive, COLLISION_SIMD_WIDTH);

			for (uint32 active = 0; active < count; ++active)
			{
				const soa_bounding_box& soaBB = activeBBs[active];
				const w_bounding_box& wB = { w_vec3(soaBB.minX, soaBB.minY, soaBB.minZ), w_vec3(soaBB.maxX, soaBB.maxY, soaBB.maxZ) };

				uint32 numValidLanes = clamp(numActive - active * COLLISION_SIMD_WIDTH, 0u, COLLISION_SIMD_WIDTH);
				uint32 validLanesMask = (1 << numValidLanes) - 1;

				auto overlap = aabbVsAABB(wA, wB);
				int32 mask = toBitMask(overlap) & validLanesMask;

				for (uint32 k = 0; k < COLLISION_SIMD_WIDTH; ++k)
				{
					if (mask & (1 << k))
					{
						outCollisions[numCollisions++] = { ep.colliderIndex, activeList[active * COLLISION_SIMD_WIDTH + k] };
					}
				}
			}

			assert(ep.colliderIndex < numColliders);
			positionInActiveList[ep.colliderIndex] = numActive;

			soa_bounding_box& outBB = activeBBs[numActive / COLLISION_SIMD_WIDTH];
			uint32 outBBSlot = numActive % COLLISION_SIMD_WIDTH;
			outBB.minX[outBBSlot] = a.minCorner.x;
			outBB.minY[outBBSlot] = a.minCorner.y;
			outBB.minZ[outBBSlot] = a.minCorner.z;
			outBB.maxX[outBBSlot] = a.maxCorner.x;
			outBB.maxY[outBBSlot] = a.maxCorner.y;
			outBB.maxZ[outBBSlot] = a.maxCorner.z;


			activeList[numActive++] = ep.colliderIndex;

			maxNumActive = max(maxNumActive, numActive);
		}
		else
		{
			uint16 pos = positionInActiveList[ep.colliderIndex];

			--numActive;

			uint16 lastColliderInActiveList = activeList[numActive];
			positionInActiveList[lastColliderInActiveList] = pos;

			activeList[pos] = activeList[numActive];

			soa_bounding_box& outBB = activeBBs[pos / COLLISION_SIMD_WIDTH];
			uint32 outBBSlot = pos % COLLISION_SIMD_WIDTH;
			const soa_bounding_box& fromBB = activeBBs[numActive / COLLISION_SIMD_WIDTH];
			uint32 fromBBSlot = numActive % COLLISION_SIMD_WIDTH;

			outBB.minX[outBBSlot] = fromBB.minX[fromBBSlot];
			outBB.minY[outBBSlot] = fromBB.minY[fromBBSlot];
			outBB.minZ[outBBSlot] = fromBB.minZ[fromBBSlot];
			outBB.maxX[outBBSlot] = fromBB.maxX[fromBBSlot];
			outBB.maxY[outBBSlot] = fromBB.maxY[fromBBSlot];
			outBB.maxZ[outBBSlot] = fromBB.maxZ[fromBBSlot];
		}
	}

	assert(numActive == 0);

	CPU_PROFILE_STAT("Max num active in SAP", maxNumActive);

	return numCollisions;

#undef COLLISION_SIMD_WIDTH
}

uint32 broadphase(game_scene& scene, bounding_box* worldSpaceAABBs, memory_arena& arena, collider_pair* outCollisions, bool simd)
{
	CPU_PROFILE_BLOCK("Broad phase");

	uint32 numColliders = scene.numberOfComponentsOfType<collider_component>();
	if (numColliders == 0)
	{
		return 0;
	}

	sap_context& context = scene.getContextVariable<sap_context>();
	auto& endpoints = context.endpoints;

	uint32 numEndpoints = numColliders * 2;

	assert(numEndpoints == endpoints.size());

	uint32 numCollisions = 0;

#if 0
	// Disable broadphase.

	uint16 collider0Index = 0;
	for (auto [entityHandle0, collider0] : scene.view<collider_component>().each())
	{
		uint16 collider1Index = 0;
		for (auto [entityHandle1, collider1] : scene.view<collider_component>().each())
		{
			if (entityHandle0 == entityHandle1)
			{
				break;
			}
			if (collider0.parentEntity != collider1.parentEntity)
			{
				outCollisions[numCollisions++] = { collider0Index, collider1Index };
			}

			++collider1Index;
		}
		++collider0Index;
	}
	return numCollisions;

#endif

	vec3 s(0.f, 0.f, 0.f);
	vec3 s2(0.f, 0.f, 0.f);

	uint32 sortingAxis = context.sortingAxis;

	CPU_PROFILE_STAT("Broadphase sorting axis", sortingAxis);

	{
		CPU_PROFILE_BLOCK("Update endpoints");

		// Index of each collider in the scene. 
		// We iterate over the endpoint indirections, which are sorted the exact same way as the colliders.
		uint16 index = 0;

		for (auto [entityHandle, indirection] : scene.view<sap_endpoint_indirection_component>().each())
		{
			const bounding_box& aabb = worldSpaceAABBs[index];

			uint16 start = indirection.startEndpoint;
			uint16 end = indirection.endEndpoint;

			float lo = aabb.minCorner.data[sortingAxis];
			float hi = aabb.maxCorner.data[sortingAxis];
			endpoints[start].value = lo;
			endpoints[end].value = hi;

			endpoints[start].colliderIndex = index;
			endpoints[end].colliderIndex = index;

			assert(endpoints[start].entity == entityHandle);
			assert(endpoints[end].entity == entityHandle);

			vec3 center = aabb.getCenter();
			s += center;
			s2 += center * center;

			++index;
		}
	}

	{
		CPU_PROFILE_BLOCK("Sort endpoints");

#if 1
		// Insertion sort.
		for (uint32 i = 1; i < numEndpoints; ++i)
		{
			sap_endpoint key = endpoints[i];
			uint32 j = i - 1;

			while (j != UINT32_MAX && endpoints[j].value > key.value)
			{
				endpoints[j + 1] = endpoints[j];
				j = j - 1;
			}
			endpoints[j + 1] = key;
		}
#else
		std::sort(endpoints, endpoints + numEndpoints, [](sap_endpoint a, sap_endpoint b) { return a.value < b.value; });
#endif
	}


	memory_marker marker = arena.getMarker();


	if (simd)
	{
		numCollisions = determineOverlapsSIMD(endpoints.data(), numEndpoints, worldSpaceAABBs, numColliders, arena, outCollisions);
	}
	else
	{
		numCollisions = determineOverlapsScalar(endpoints.data(), numEndpoints, worldSpaceAABBs, numColliders, arena, outCollisions);
	}

	
	arena.resetToMarker(marker);


	// Fix up indirections.
	{
		CPU_PROFILE_BLOCK("Fix up indirections");

		for (uint32 i = 0; i < numEndpoints; ++i)
		{
			sap_endpoint ep = endpoints[i];
			scene_entity entity = { ep.entity, scene };
			sap_endpoint_indirection_component& in = entity.getComponent<sap_endpoint_indirection_component>();

			if (ep.start)
			{
				in.startEndpoint = i;
			}
			else
			{
				in.endEndpoint = i;
			}
		}
	}


	vec3 variance = s2 - s * s / (float)numColliders;
	context.sortingAxis = (variance.x > variance.y) ? ((variance.x > variance.z) ? 0 : 2) : ((variance.y > variance.z) ? 1 : 2);

	return numCollisions;
}
