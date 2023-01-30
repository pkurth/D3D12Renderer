#include "pch.h"
#include "heightmap_collider.h"


heightmap_collider_component::heightmap_collider_component(uint32 chunksPerDim, float chunkSize, physics_material material)
	: chunksPerDim(chunksPerDim), chunkSize(chunkSize), invChunkSize(1.f / chunkSize), material(material)
{
	colliders.resize(chunksPerDim * chunksPerDim);

	uint32 numSegmentsPerDim = (TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1);
	chunkScale = chunkSize / numSegmentsPerDim;
}

void heightmap_collider_component::update(vec3 minCorner, float amplitudeScale)
{
	this->minCorner = minCorner;
	this->invAmplitudeScale = 1.f / amplitudeScale;
	this->heightScale = amplitudeScale / UINT16_MAX;
}

float heightmap_collider_component::getHeightAt(vec2 coord) const
{
	coord -= vec2(this->minCorner.x, this->minCorner.z);

	coord *= invChunkSize;

	if (coord.x < 0.f || coord.y < 0.f || coord.x >= chunksPerDim || coord.y >= chunksPerDim)
	{
		return -FLT_MAX;
	}

	uint32 chunkX = (uint32)coord.x;
	uint32 chunkZ = (uint32)coord.y;

	auto& col = collider(chunkX, chunkZ);

	coord = frac(coord);
	coord *= (TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1);

	return col.getHeightAt(coord, heightScale, this->minCorner.y);
}

void heightmap_collider_chunk::setHeights(uint16* heights)
{
	this->heights = heights;

	uint32 numSegments = TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1;
	uint32 numMips = log2(numSegments) + 1;

	mips.resize(numMips);




	{
		uint32 readStride = TERRAIN_LOD_0_VERTICES_PER_DIMENSION;

		auto& mip = mips.front();
		mip.resize(numSegments * numSegments);
		for (uint32 z = 0; z < numSegments; ++z)
		{
			for (uint32 x = 0; x < numSegments; ++x)
			{
				uint32 aIndex = (readStride * z + x);
				uint32 bIndex = (readStride * (z + 1) + x);
				uint32 cIndex = (readStride * z + x + 1);
				uint32 dIndex = (readStride * (z + 1) + x + 1);

				uint16 a = heights[aIndex];
				uint16 b = heights[bIndex];
				uint16 c = heights[cIndex];
				uint16 d = heights[dIndex];

				heightmap_min_max v =
				{
					min(a, min(b, min(c, d))),
					max(a, max(b, max(c, d))),
				};

				mip[numSegments * z + x] = v;
			}
		}
	}

	for (uint32 i = 1; i < numMips; ++i)
	{
		uint32 readStride = numSegments;

		numSegments >>= 1;

		auto& readMip = mips[i - 1];
		auto& writeMip = mips[i];
		writeMip.resize(numSegments * numSegments);
		for (uint32 z = 0; z < numSegments; ++z)
		{
			for (uint32 x = 0; x < numSegments; ++x)
			{
				uint32 x0 = x * 2;
				uint32 x1 = x * 2 + 1;
				uint32 z0 = z * 2;
				uint32 z1 = z * 2 + 1;

				heightmap_min_max a = readMip[readStride * z0 + x0];
				heightmap_min_max b = readMip[readStride * z1 + x0];
				heightmap_min_max c = readMip[readStride * z0 + x1];
				heightmap_min_max d = readMip[readStride * z1 + x1];

				heightmap_min_max v =
				{
					min(a.min, min(b.min, min(c.min, d.min))),
					max(a.max, max(b.max, max(c.max, d.max))),
				};

				writeMip[numSegments * z + x] = v;
			}
		}
	}

	assert(mips.back().size() == 1);
}

float heightmap_collider_chunk::getHeightAt(vec2 coord, float heightScale, float heightOffset) const
{
	if (!heights)
	{
		return -FLT_MAX;
	}

	assert(coord.x >= 0.f && coord.x <= TERRAIN_LOD_0_VERTICES_PER_DIMENSION && coord.y >= 0.f && coord.y <= TERRAIN_LOD_0_VERTICES_PER_DIMENSION);

	uint32 x = (uint32)coord.x;
	uint32 z = (uint32)coord.y;

	float relX = coord.x - x;
	float relZ = coord.y - z;


	uint32 stride = TERRAIN_LOD_0_VERTICES_PER_DIMENSION;

	uint32 aIndex = (stride * z + x);
	uint32 bIndex = (stride * (z + 1) + x);
	uint32 cIndex = (stride * z + x + 1);
	uint32 dIndex = (stride * (z + 1) + x + 1);

	float a = heights[aIndex] * heightScale;
	float b = heights[bIndex] * heightScale;
	float c = heights[cIndex] * heightScale;
	float d = heights[dIndex] * heightScale;

	float h = lerp(lerp(a, c, relX), lerp(b, d, relX), relZ) + heightOffset;

	return h;
}
