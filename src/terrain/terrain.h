#pragma once

#include "core/math.h"
#include "core/camera.h"

#include "dx/dx_texture.h"

#include "rendering/material.h"
#include "rendering/render_command.h"

struct terrain_chunk
{
	bool active;

	float minHeight, maxHeight;

	ref<dx_texture> heightmap;
	ref<dx_texture> normalmap;
};

struct terrain_component
{
	terrain_component(uint32 chunksPerDim, float chunkSize, float amplitudeScale);
	terrain_component(const terrain_component&) = default;
	terrain_component(terrain_component&&) = default;
	terrain_component& operator=(const terrain_component&) = default;
	terrain_component& operator=(terrain_component&&) = default;

	terrain_chunk& chunk(uint32 x, uint32 z) { return chunks[z * chunksPerDim + x]; }
	const terrain_chunk& chunk(uint32 x, uint32 z) const { return chunks[z * chunksPerDim + x]; }

	uint32 chunksPerDim;
	float chunkSize;
	float amplitudeScale;


	void render(const render_camera& camera, struct opaque_render_pass* renderPass, vec3 positionOffset);

private:
	std::vector<terrain_chunk> chunks;
};


struct terrain_render_data
{
	vec3 minCorner;
	int32 lod;
	float chunkSize;

	int32 lod_negX;
	int32 lod_posX;
	int32 lod_negZ;
	int32 lod_posZ;

	ref<dx_texture> heightmap;
	ref<dx_texture> normalmap;
};

struct terrain_pipeline
{
	using render_data_t = terrain_render_data;

	static void initialize();

	PIPELINE_SETUP_DECL;
	PIPELINE_RENDER_DECL;
};



