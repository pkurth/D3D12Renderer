#pragma once

#include "raytracing.h"
#include "raytracing_tlas.h"
#include "raytracing_binding_table.h"

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

	dx_raytracing_pipeline pipeline;
};
