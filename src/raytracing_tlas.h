#pragma once

#include "raytracing.h"

struct raytracing_instance_handle
{
	uint32 instanceIndex;
};

struct raytracing_tlas
{
	void initialize(acceleration_structure_rebuild_mode rebuildMode = acceleration_structure_rebuild);

	raytracing_instance_handle instantiate(raytracing_object_handle type, const trs& transform);
	void updateInstanceTransform(raytracing_instance_handle handle, const trs& transform);

	void build();

	dx_cpu_descriptor_handle getHandle() const { return tlasSRVs[tlasDescriptorIndex]; }

private:
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> allInstances;

	acceleration_structure_rebuild_mode rebuildMode;

	uint32 tlasDescriptorIndex = 0;

	dx_cpu_descriptor_handle tlasSRVs[NUM_BUFFERED_FRAMES];

	ref<dx_buffer> scratch;
	ref<dx_buffer> tlas;
};
