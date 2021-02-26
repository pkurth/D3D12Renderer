#include "pch.h"
#include "shadow_map_cache.h"
#include "dx_renderer.h"

//#define STB_RECT_PACK_IMPLEMENTATION
#include <imgui/imstb_rectpack.h>



struct tree_node
{
	uint32 x, y;
};

static constexpr uint32 maximumSize = 2048;
static constexpr uint32 minimumSize = 128;
static_assert(SHADOW_MAP_WIDTH % maximumSize == 0, "");
static_assert(SHADOW_MAP_HEIGHT % maximumSize == 0, "");
static_assert(isPowerOfTwo(maximumSize), "");
static_assert(isPowerOfTwo(minimumSize), "");

static constexpr uint32 numBuckets = indexOfLeastSignificantSetBit(maximumSize) - indexOfLeastSignificantSetBit(minimumSize) + 1;
static std::vector<tree_node> nodes[numBuckets];

static void initialize()
{
	for (uint32 y = 0; y < SHADOW_MAP_HEIGHT / maximumSize; ++y)
	{
		for (uint32 x = 0; x < SHADOW_MAP_WIDTH / maximumSize; ++x)
		{
			nodes[0].push_back({ x * maximumSize, y * maximumSize });
		}
	}
}

static uint32 getIndex(uint32 size)
{
	uint32 index = indexOfLeastSignificantSetBit(size) - indexOfLeastSignificantSetBit(minimumSize);
	index = numBuckets - index - 1;
	return index;
}

static tree_node insert(uint32 size)
{
	assert(size <= maximumSize);
	assert(size >= minimumSize);
	assert(isPowerOfTwo(size));

	uint32 index = getIndex(size);

	uint32 insertIndex = index;
	for (; insertIndex != -1 && nodes[insertIndex].size() == 0; --insertIndex);

	assert(insertIndex != -1);
	assert(nodes[insertIndex].size() > 0);


	for (uint32 i = insertIndex; i < index; ++i)
	{
		uint32 nodeSize = 1 << (indexOfLeastSignificantSetBit(maximumSize) - i);
		uint32 halfSize = nodeSize / 2;

		tree_node n = nodes[i].back();
		nodes[i].pop_back();

		nodes[i + 1].push_back({ n.x, n.y });
		nodes[i + 1].push_back({ n.x + halfSize, n.y });
		nodes[i + 1].push_back({ n.x, n.y + halfSize });
		nodes[i + 1].push_back({ n.x + halfSize, n.y + halfSize });
	}

	tree_node result = nodes[index].back();
	nodes[index].pop_back();
	return result;
}

static void free(tree_node remove, uint32 size)
{
	if (size == maximumSize)
	{
		nodes[0].push_back(remove);
		return;
	}


	uint32 index = getIndex(size);
	std::vector<tree_node>& list = nodes[index];
	if (list.size() == 0)
	{
		list.push_back(remove);
		return;
	}


	uint32 doubleSize = size * 2;
	uint32 shift = indexOfLeastSignificantSetBit(doubleSize);

	uint32 parentX = remove.x >> shift;
	uint32 parentY = remove.y >> shift;
	uint32 parentWidth = SHADOW_MAP_WIDTH >> shift;
	uint32 parentIndex = parentY * parentWidth + parentX;


	uint32 nodesWithThisParent = 0;
	auto insertIt = list.end();

	for (auto it = list.begin(); it != list.end(); ++it)
	{
		tree_node node = *it;

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

	assert(nodesWithThisParent < 4);

	if (nodesWithThisParent == 3)
	{
		auto eraseIt = insertIt - 3;
		list.erase(eraseIt, insertIt);
		free(tree_node{ parentX * doubleSize, parentY * doubleSize }, doubleSize);
	}
	else
	{
		list.insert(insertIt, remove);
	}
}















static stbrp_node packingNodes[SHADOW_MAP_WIDTH]; // stb_rectpack recommends the number of nodes to be >= width.

void testShadowMapCache(shadow_map_light_info* infos, uint32 numInfos)
{
	initialize();
	auto r = insert(512);
	auto r2 = insert(1024);
	free(r, 512);

	stbrp_rect* rects = (stbrp_rect*)alloca(sizeof(stbrp_rect) * numInfos);

	for (uint32 i = 0; i < numInfos; ++i)
	{
		shadow_map_light_info& info = infos[i];
		stbrp_rect& rect = rects[i];

		rect.w = info.viewport.cpuVP[2];
		rect.h = info.viewport.cpuVP[3];
		rect.was_packed = false;

		if (info.lightMovedOrAppeared || info.viewport.cpuVP[3] < 0.f)
		{
			// Render static geometry.
			// Cache result.
			// Render dynamic geometry.
		}
		else
		{
			if (info.geometryInRangeMoved)
			{
				// Copy static cache to active.
				// Render dynamic geometry.
			}
			else
			{
				// Shadow map exists.
				// Copy it from back to front buffer?
			}
		}
	}
	
	
	stbrp_context packContext;
	stbrp_init_target(&packContext, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, packingNodes, arraysize(packingNodes));

	int result = stbrp_pack_rects(&packContext, rects, numInfos);
	assert(result);

	for (uint32 i = 0; i < numInfos; ++i)
	{
		shadow_map_light_info& info = infos[i];
		stbrp_rect& rect = rects[i];

		info.viewport.cpuVP[0] = rect.x;
		info.viewport.cpuVP[1] = rect.y;
		info.viewport.cpuVP[2] = rect.w;
		info.viewport.cpuVP[3] = rect.h;
		info.viewport.shaderVP.x = (float)rect.x / SHADOW_MAP_WIDTH;
		info.viewport.shaderVP.y = (float)rect.y / SHADOW_MAP_WIDTH;
		info.viewport.shaderVP.z = (float)rect.w / SHADOW_MAP_WIDTH;
		info.viewport.shaderVP.w = (float)rect.h / SHADOW_MAP_WIDTH;
	}
}
