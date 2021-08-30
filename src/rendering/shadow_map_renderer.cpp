#include "pch.h"
#include "shadow_map_renderer.h"
#include "render_resources.h"
#include "render_utils.h"
#include "render_algorithms.h"
#include "dx/dx_context.h"
#include "dx/dx_command_list.h"

const sun_shadow_render_pass* shadow_map_renderer::sunShadowRenderPasses[MAX_NUM_SUN_LIGHT_SHADOW_PASSES];
const spot_shadow_render_pass* shadow_map_renderer::spotLightShadowRenderPasses[MAX_NUM_SPOT_LIGHT_SHADOW_PASSES];
const point_shadow_render_pass* shadow_map_renderer::pointLightShadowRenderPasses[MAX_NUM_POINT_LIGHT_SHADOW_PASSES];
uint32 shadow_map_renderer::numSunLightShadowRenderPasses;
uint32 shadow_map_renderer::numSpotLightShadowRenderPasses;
uint32 shadow_map_renderer::numPointLightShadowRenderPasses;

void shadow_map_renderer::beginFrame()
{
	numSunLightShadowRenderPasses = 0;
	numSpotLightShadowRenderPasses = 0;
	numPointLightShadowRenderPasses = 0;
}

void shadow_map_renderer::endFrame()
{
	dx_command_list* cl = dxContext.getFreeRenderCommandList();

	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cl->transitionBarrier(render_resources::shadowMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	shadowPasses(cl, sunShadowRenderPasses, numSunLightShadowRenderPasses,
		spotLightShadowRenderPasses, numSpotLightShadowRenderPasses,
		pointLightShadowRenderPasses, numPointLightShadowRenderPasses);

	cl->transitionBarrier(render_resources::shadowMap, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	dxContext.executeCommandList(cl);
}
