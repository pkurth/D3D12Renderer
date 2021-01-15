#pragma once

#include "raytracing.h"

struct raytracing_instance_handle
{
	uint32 instanceIndex;
};

struct raytracing_tlas
{
	void initialize(acceleration_structure_rebuild_mode rebuildMode = acceleration_structure_rebuild);

	// Call these each frame to rebuild the structure.
	void reset();
	raytracing_instance_handle instantiate(raytracing_object_type type, const trs& transform);
	void build();


	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> allInstances;

	acceleration_structure_rebuild_mode rebuildMode;

	ref<dx_buffer> scratch;
	ref<dx_buffer> tlas;
};
