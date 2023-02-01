#pragma once

#include "terrain/terrain.h"


struct proc_placement_layer_desc
{
	float footprint;

	ref<multi_mesh> meshes[4] = {};
};

struct proc_placement_component
{
	proc_placement_component(uint32 chunksPerDim, const std::vector<proc_placement_layer_desc>& layers);
	void generate(const render_camera& camera, const terrain_component& terrain, vec3 positionOffset);
	void render(struct ldr_render_pass* renderPass);

private:

	struct placement_layer
	{
		float footprint;
		
		uint32 globalMeshOffset;
		uint32 numMeshes;

		ref<multi_mesh> meshes[4];
	};


	std::vector<placement_layer> layers;

	ref<dx_buffer> placementPointBuffer;
	ref<dx_buffer> transformBuffer;
	ref<dx_buffer> meshCountBuffer;
	ref<dx_buffer> meshOffsetBuffer;
	ref<dx_buffer> submeshToMeshBuffer;
	ref<dx_buffer> drawIndirectBuffer;
};


void initializeProceduralPlacementPipelines();

