#pragma once

#include "core/math.h"
#include "dx/dx_texture.h"

#include "rendering/material.h"
#include "rendering/render_command.h"

struct terrain_chunk
{
	vec2 minCorner;
	ref<dx_texture> heightmap;
};

struct terrain_component
{
	terrain_component(float chunkSize, float amplitudeScale);
	terrain_component(terrain_component&&) = default;
	terrain_component& operator=(terrain_component&&) = default;

	std::vector<terrain_chunk> chunks;
	float chunkSize;
	float amplitudeScale;
};


struct terrain_render_data
{
	vec2 minCorner;
	int32 lod;
	float chunkSize;
	float amplitudeScale;

	int32 lod_negX;
	int32 lod_posX;
	int32 lod_negZ;
	int32 lod_posZ;
};

struct terrain_pipeline
{
	using render_data_t = terrain_render_data;

	static void initialize();

	PIPELINE_SETUP_DECL;
	PIPELINE_RENDER_DECL;
};



