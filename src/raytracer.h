#pragma once

#include "raytracing.h"
#include "raytracing_tlas.h"
#include "raytracing_binding_table.h"
#include "dx_descriptor_allocation.h"

#include "dx_command_list.h"

#include "material.h"

struct dx_raytracer
{
	virtual void render(dx_command_list* cl, const raytracing_tlas& tlas,
		const ref<dx_texture>& output,
		const common_material_info& materialInfo) = 0;

protected:
	void fillOutRayTracingRenderDesc(const ref<dx_buffer>& bindingTableBuffer,
		D3D12_DISPATCH_RAYS_DESC& raytraceDesc,
		uint32 renderWidth, uint32 renderHeight, uint32 renderDepth,
		uint32 numRayTypes, uint32 numHitGroups);

	template <typename input_resources, typename output_resources>
	dx_gpu_descriptor_handle copyGlobalResourcesToDescriptorHeap(const input_resources& in, const output_resources& out);

	template <typename input_resources, typename output_resources>
	void allocateDescriptorHeapSpaceForGlobalResources(dx_pushable_resource_descriptor_heap& descriptorHeap);

	dx_raytracing_pipeline pipeline;

	
	dx_cpu_descriptor_handle resourceCPUBase[NUM_BUFFERED_FRAMES];
	dx_gpu_descriptor_handle resourceGPUBase[NUM_BUFFERED_FRAMES];
};

template<typename input_resources, typename output_resources>
inline dx_gpu_descriptor_handle dx_raytracer::copyGlobalResourcesToDescriptorHeap(const input_resources& in, const output_resources& out)
{
	dx_cpu_descriptor_handle cpuHandle = resourceCPUBase[dxContext.bufferedFrameID];
	dx_gpu_descriptor_handle gpuHandle = resourceGPUBase[dxContext.bufferedFrameID];

	const uint32 numInputResources = sizeof(input_resources) / sizeof(dx_cpu_descriptor_handle);
	const uint32 numOutputResources = sizeof(output_resources) / sizeof(dx_cpu_descriptor_handle);
	const uint32 totalNumResources = numInputResources + numOutputResources;

	D3D12_CPU_DESCRIPTOR_HANDLE handles[totalNumResources];
	memcpy(handles, &in, sizeof(input_resources));
	memcpy((D3D12_CPU_DESCRIPTOR_HANDLE*)handles + numInputResources, &out, sizeof(output_resources));

	dxContext.device->CopyDescriptors(
		1, (D3D12_CPU_DESCRIPTOR_HANDLE*)&cpuHandle, &totalNumResources,
		totalNumResources, handles, nullptr,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	);

	return gpuHandle;
}

template<typename input_resources, typename output_resources>
inline void dx_raytracer::allocateDescriptorHeapSpaceForGlobalResources(dx_pushable_resource_descriptor_heap& descriptorHeap)
{
	const uint32 numInputResources = sizeof(input_resources) / sizeof(dx_cpu_descriptor_handle);
	const uint32 numOutputResources = sizeof(output_resources) / sizeof(dx_cpu_descriptor_handle);
	const uint32 totalNumResources = numInputResources + numOutputResources;

	for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; ++i)
	{
		resourceCPUBase[i] = descriptorHeap.currentCPU;
		resourceGPUBase[i] = descriptorHeap.currentGPU;

		for (uint32 j = 0; j < totalNumResources; ++j)
		{
			descriptorHeap.push();
		}
	}
}
