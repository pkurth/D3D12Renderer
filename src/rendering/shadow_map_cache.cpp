#include "pch.h"
#include "shadow_map_cache.h"
#include "dx/dx_texture.h"
#include "dx/dx_command_list.h"
#include "core/hash.h"
#include "render_resources.h"

#include "light_source.h"
#include "core/camera.h"

#include "core/imgui.h"


#define DEBUG_VISUALIZATION 0


bool enableStaticShadowMapCaching = true;


static constexpr uint16 maximumSize = 2048;
static constexpr uint16 minimumSize = 128;
static_assert(SHADOW_MAP_WIDTH % maximumSize == 0, "");
static_assert(SHADOW_MAP_HEIGHT % maximumSize == 0, "");
static_assert(isPowerOfTwo(maximumSize), "");
static_assert(isPowerOfTwo(minimumSize), "");

static constexpr uint32 numBuckets = indexOfLeastSignificantSetBit(maximumSize) - indexOfLeastSignificantSetBit(minimumSize) + 1;
static std::vector<shadow_map_viewport> nodes[numBuckets];

struct shadow_map_cache
{
	shadow_map_viewport viewport;
	uint64 lightMovementHash;
};

static std::unordered_map<uint64, shadow_map_cache> cachedAllocations;

static ref<dx_texture> visTexture;

static void initialize()
{
	for (uint32 y = 0; y < SHADOW_MAP_HEIGHT / maximumSize; ++y)
	{
		for (uint32 x = 0; x < SHADOW_MAP_WIDTH / maximumSize; ++x)
		{
			nodes[0].push_back({ (uint16)(x * maximumSize), (uint16)(y * maximumSize), maximumSize });
		}
	}
}

static uint32 getIndex(uint32 size)
{
	uint32 index = indexOfLeastSignificantSetBit(size) - indexOfLeastSignificantSetBit(minimumSize);
	index = numBuckets - index - 1;
	return index;
}

static shadow_map_viewport allocateShadowViewport(uint32 size)
{
	ASSERT(size <= maximumSize);
	ASSERT(size >= minimumSize);
	ASSERT(isPowerOfTwo(size));

	uint32 index = getIndex(size);

	uint32 insertIndex = index;
	for (; insertIndex != -1 && nodes[insertIndex].size() == 0; --insertIndex);

	ASSERT(insertIndex != -1); // TODO: Handle error.
	ASSERT(nodes[insertIndex].size() > 0);


	for (uint32 i = insertIndex; i < index; ++i)
	{
		uint16 nodeSize = 1u << (indexOfLeastSignificantSetBit(maximumSize) - i);
		uint16 halfSize = nodeSize / 2;

		shadow_map_viewport n = nodes[i].back();
		nodes[i].pop_back();

		nodes[i + 1].push_back({ n.x, n.y, halfSize });
		nodes[i + 1].push_back({ (uint16)(n.x + halfSize), n.y, halfSize });
		nodes[i + 1].push_back({ n.x, (uint16)(n.y + halfSize), halfSize });
		nodes[i + 1].push_back({ (uint16)(n.x + halfSize), (uint16)(n.y + halfSize), halfSize });
	}

	shadow_map_viewport result = nodes[index].back();
	nodes[index].pop_back();
	return result;
}

static void freeShadowViewport(shadow_map_viewport remove)
{
	if (remove.size == maximumSize)
	{
		nodes[0].push_back(remove);
		return;
	}


	uint32 index = getIndex(remove.size);
	std::vector<shadow_map_viewport>& list = nodes[index];
	if (list.size() == 0)
	{
		list.push_back(remove);
		return;
	}


	uint32 doubleSize = remove.size * 2;
	uint32 shift = indexOfLeastSignificantSetBit(doubleSize);

	uint32 parentX = remove.x >> shift;
	uint32 parentY = remove.y >> shift;
	uint32 parentWidth = SHADOW_MAP_WIDTH >> shift;
	uint32 parentIndex = parentY * parentWidth + parentX;


	uint32 nodesWithThisParent = 0;
	auto insertIt = list.end();

	for (auto it = list.begin(); it != list.end(); ++it)
	{
		shadow_map_viewport node = *it;

		uint32 pX = node.x >> shift;
		uint32 pY = node.y >> shift;
		uint32 pi = pY * parentWidth + pX;

		if (pi == parentIndex)
		{
			++nodesWithThisParent;
		}
		else if (pi > parentIndex)
		{
			insertIt = it;
			break;
		}
	}

	ASSERT(nodesWithThisParent < 4);

	if (nodesWithThisParent == 3)
	{
		auto eraseIt = insertIt - 3;
		list.erase(eraseIt, insertIt);
		freeShadowViewport(shadow_map_viewport{ (uint16)(parentX * doubleSize), (uint16)(parentY * doubleSize), (uint16)doubleSize });
	}
	else
	{
		list.insert(insertIt, remove);
	}
}

static void visualize(dx_command_list* cl, ref<dx_texture> texture)
{
	for (uint32 i = 0; i < numBuckets; ++i)
	{
		std::vector<shadow_map_viewport>& nodesInLevel = nodes[i];

		for (shadow_map_viewport vp : nodesInLevel)
		{
			clear_rect rect = { vp.x, vp.y, vp.size, vp.size };
			cl->clearRTV(texture, 0.f, 1.f, 0.f, 1.f, &rect, 1);
		}
	}

	for (auto a : cachedAllocations)
	{
		auto vp = a.second.viewport;
		clear_rect rect = { vp.x, vp.y, vp.size, vp.size };
		cl->clearRTV(texture, 1.f, 0.f, 0.f, 1.f, &rect, 1);
	}
}


static bool init = false;

std::pair<shadow_map_viewport, bool> assignShadowMapViewport(uint64 uniqueLightID, uint64 lightMovementHash, uint32 size)
{
	if (!init)
	{
		initialize();
#if DEBUG_VISUALIZATION
		visTexture = createTexture(0, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, DXGI_FORMAT_R8G8B8A8_UNORM, false, true, false);
#endif
		init = true;
	}


	bool staticCacheAvailable = false;
	shadow_map_viewport vp;

	auto it = cachedAllocations.find(uniqueLightID);
	if (it == cachedAllocations.end())
	{
		// New light.
		vp = allocateShadowViewport(size);
		it = cachedAllocations.insert({ uniqueLightID, { vp, lightMovementHash } }).first;
	}
	else
	{
		if (it->second.viewport.size != size)
		{
			// Reallocate.
			freeShadowViewport(it->second.viewport);
			vp = allocateShadowViewport(size);
			it->second.viewport = vp;
		}
		else
		{
			if (it->second.lightMovementHash == lightMovementHash)
			{
				staticCacheAvailable = true;
			}

			vp = it->second.viewport;
		}

		it->second.lightMovementHash = lightMovementHash;
	}

	staticCacheAvailable &= enableStaticShadowMapCaching;

	return { vp, staticCacheAvailable };
}

uint64 getLightMovementHash(const directional_light& dl)
{
	size_t seed = 0;
	for (uint32 i = 0; i < dl.numShadowCascades; ++i)
	{
		hash_combine(seed, dl.viewProjs[i]);
	}
	return seed;
}

uint64 getLightMovementHash(const spot_light_cb& sl)
{
	size_t seed = 0;
	hash_combine(seed, sl.direction);
	hash_combine(seed, sl.position);
	hash_combine(seed, sl.getOuterCutoff());
	hash_combine(seed, sl.maxDistance);
	return seed;
}

uint64 getLightMovementHash(const point_light_cb& pl)
{
	size_t seed = 0;
	hash_combine(seed, pl.position);
	hash_combine(seed, pl.radius);
	return seed;
}

void updateShadowMapAllocationVisualization()
{
#if DEBUG_VISUALIZATION
	if (updateVisualization)
	{
		dx_command_list* cl = dxContext.getFreeRenderCommandList();
		visualize(cl, visTexture);
		dxContext.executeCommandList(cl);

		ImGui::Begin("Settings");
		ImGui::Image(visTexture, 512, 512);
		ImGui::End();
	}
#endif
}
