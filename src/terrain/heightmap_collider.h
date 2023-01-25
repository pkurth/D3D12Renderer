#pragma once

#include "core/math.h"
#include "core/memory.h"
#include "physics/bounding_volumes.h"

#ifndef TERRAIN_LOD_0_VERTICES_PER_DIMENSION
// Must match terrain_rs.hlsli
#define TERRAIN_LOD_0_VERTICES_PER_DIMENSION 129
#endif

struct heightmap_collider_chunk
{
	void setHeights(uint16* heights);

	template <typename callback_func>
	void iterateTrianglesInVolume(uint32 volMinX, uint32 volMinZ, uint32 volMaxX, uint32 volMaxZ,
		uint32 volMinY, uint32 volMaxY, float chunkScale, float heightScale, vec3 chunkMinCorner, memory_arena& arena, const callback_func& func);

private:
	uint16* heights = 0;


	struct heightmap_min_max
	{
		uint16 min, max;
	};

	std::vector<std::vector<heightmap_min_max>> mips;
};

template <typename callback_func>
void heightmap_collider_chunk::iterateTrianglesInVolume(uint32 volMinX, uint32 volMinZ, uint32 volMaxX, uint32 volMaxZ,
	uint32 volMinY, uint32 volMaxY, float chunkScale, float heightScale, vec3 chunkMinCorner, memory_arena& arena, const callback_func& func)
{
	if (!heights)
	{
		return;
	}

	memory_marker marker = arena.getMarker();

	struct stack_entry
	{
		uint16 mipLevel;
		uint16 x, z;
	};

	stack_entry* stack = arena.allocate<stack_entry>(1024);
	uint32 stackSize = 0;


	uint32 numMips = (uint32)mips.size();

	stack[stackSize++] = { (uint16)(numMips - 1), 0, 0 };

	while (stackSize > 0)
	{
		stack_entry entry = stack[--stackSize];

		uint32 minX = entry.x << entry.mipLevel;
		uint32 minZ = entry.z << entry.mipLevel;
		uint32 maxX = ((entry.x + 1) << entry.mipLevel) - 1;
		uint32 maxZ = ((entry.z + 1) << entry.mipLevel) - 1;

		if (maxX < volMinX || minX > volMaxX) continue;
		if (maxZ < volMinZ || minZ > volMaxZ) continue;


		uint32 numSegmentsPerDim = (TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1) >> entry.mipLevel;
		heightmap_min_max minmax = mips[entry.mipLevel][entry.z * numSegmentsPerDim + entry.x];
		if (minmax.max < volMinY || minmax.min > volMaxY) continue;


		if (entry.mipLevel == 0)
		{
			uint32 stride = TERRAIN_LOD_0_VERTICES_PER_DIMENSION;

			uint32 aIndex = (stride * entry.z + entry.x);
			uint32 bIndex = (stride * (entry.z + 1) + entry.x);
			uint32 cIndex = (stride * entry.z + entry.x + 1);
			uint32 dIndex = (stride * (entry.z + 1) + entry.x + 1);

			vec2 a = vec2((float)(entry.x), (float)(entry.z)) * chunkScale;
			vec2 b = vec2((float)(entry.x), (float)(entry.z + 1)) * chunkScale;
			vec2 c = vec2((float)(entry.x + 1), (float)(entry.z)) * chunkScale;
			vec2 d = vec2((float)(entry.x + 1), (float)(entry.z + 1)) * chunkScale;

			float heightA = heights[aIndex] * heightScale;
			float heightB = heights[bIndex] * heightScale;
			float heightC = heights[cIndex] * heightScale;
			float heightD = heights[dIndex] * heightScale;

			vec3 posA = vec3(a.x, heightA, a.y) + chunkMinCorner;
			vec3 posB = vec3(b.x, heightB, b.y) + chunkMinCorner;
			vec3 posC = vec3(c.x, heightC, c.y) + chunkMinCorner;
			vec3 posD = vec3(d.x, heightD, d.y) + chunkMinCorner;


			func(posA, posB, posC);
			func(posC, posB, posD);
		}
		else
		{
			stack[stackSize++] = { (uint16)(entry.mipLevel - 1), (uint16)(2 * entry.x + 0), (uint16)(2 * entry.z + 0) };
			stack[stackSize++] = { (uint16)(entry.mipLevel - 1), (uint16)(2 * entry.x + 0), (uint16)(2 * entry.z + 1) };
			stack[stackSize++] = { (uint16)(entry.mipLevel - 1), (uint16)(2 * entry.x + 1), (uint16)(2 * entry.z + 0) };
			stack[stackSize++] = { (uint16)(entry.mipLevel - 1), (uint16)(2 * entry.x + 1), (uint16)(2 * entry.z + 1) };
		}
	}

	arena.resetToMarker(marker);
}






struct heightmap_collider_component
{
	heightmap_collider_component(uint32 chunksPerDim, float chunkSize);

	void update(vec3 minCorner, float amplitudeScale);

	template <typename callback_func>
	void iterateTrianglesInVolume(bounding_box volume, memory_arena& arena, const callback_func& func);

	heightmap_collider_chunk& collider(uint32 x, uint32 z) { return colliders[z * chunksPerDim + x]; }
	const heightmap_collider_chunk& collider(uint32 x, uint32 z) const { return colliders[z * chunksPerDim + x]; }

private:
	vec3 minCorner;
	float invAmplitudeScale = 1.f;
	float chunkSize;
	float invChunkSize;
	float chunkScale;
	float heightScale = 0.f;

	uint32 chunksPerDim;
	std::vector<heightmap_collider_chunk> colliders;
};



template<typename callback_func>
inline void heightmap_collider_component::iterateTrianglesInVolume(bounding_box volume, memory_arena& arena, const callback_func& func)
{
	volume.minCorner -= this->minCorner;
	volume.maxCorner -= this->minCorner;

	volume.minCorner.x *= invChunkSize;
	volume.minCorner.z *= invChunkSize;
	volume.maxCorner.x *= invChunkSize;
	volume.maxCorner.z *= invChunkSize;

	// volume.xz is now in chunk space, i.e. [0, 1] for chunk 0, [1, 2] for chunk 1 and so on.
	// volume.y is still in meters, but relative to minCorner.

	uint32 minX = max((int32)volume.minCorner.x, 0);
	uint32 minZ = max((int32)volume.minCorner.z, 0);
	uint32 maxX = (uint32)clamp((int32)volume.maxCorner.x, 0, (int32)chunksPerDim - 1);
	uint32 maxZ = (uint32)clamp((int32)volume.maxCorner.z, 0, (int32)chunksPerDim - 1);

	// Convert y to uint16

	volume.minCorner.y *= invAmplitudeScale;
	volume.maxCorner.y *= invAmplitudeScale;

	uint16 minHeight = (uint16)(saturate(volume.minCorner.y) * UINT16_MAX);
	uint16 maxHeight = (uint16)(saturate(volume.maxCorner.y) * UINT16_MAX);


	for (uint32 z = minZ; z <= maxZ; ++z)
	{
		for (uint32 x = minX; x <= maxX; ++x)
		{
			float relMinX = max(volume.minCorner.x - x, 0.f);
			float relMinZ = max(volume.minCorner.z - z, 0.f);

			assert(relMinX <= 1.f);
			assert(relMinZ <= 1.f);

			float relMaxX = (volume.maxCorner.x > (x + 1)) ? 1.f : frac(volume.maxCorner.x);
			float relMaxZ = (volume.maxCorner.z > (z + 1)) ? 1.f : frac(volume.maxCorner.z);


			uint32 chunkSpaceMinX = (uint32)(relMinX * TERRAIN_LOD_0_VERTICES_PER_DIMENSION);
			uint32 chunkSpaceMinZ = (uint32)(relMinZ * TERRAIN_LOD_0_VERTICES_PER_DIMENSION);
			uint32 chunkSpaceMaxX = (uint32)(relMaxX * TERRAIN_LOD_0_VERTICES_PER_DIMENSION);
			uint32 chunkSpaceMaxZ = (uint32)(relMaxZ * TERRAIN_LOD_0_VERTICES_PER_DIMENSION);

			vec3 chunkMinCorner = vec3(x * chunkSize, 0.f, z * chunkSize) + this->minCorner;

			collider(x, z).iterateTrianglesInVolume(chunkSpaceMinX, chunkSpaceMinZ, chunkSpaceMaxX, chunkSpaceMaxZ, 
				minHeight, maxHeight, chunkScale, heightScale, chunkMinCorner, arena, func);
		}
	}
}
