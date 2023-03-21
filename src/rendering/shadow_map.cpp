#include "pch.h"
#include "shadow_map.h"
#include "render_resources.h"
#include "shadow_map_cache.h"

#include "depth_only_rs.hlsli"


shadow_render_command determineSunShadowInfo(directional_light& sun, bool invalidateCache)
{
	bool staticCacheAvailable = !invalidateCache;

	uint64 movementHash = getLightMovementHash(sun);

	shadow_render_command result;

	for (uint32 i = 0; i < sun.numShadowCascades; ++i)
	{
		auto [vp, cache] = assignShadowMapViewport(i, movementHash, sun.shadowDimensions);

		sun.shadowMapViewports[i] = vec4(vp.x, vp.y, vp.size, vp.size) / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);
		result.viewports[i] = vp;

		staticCacheAvailable &= cache;
	}

	result.renderStaticGeometry = !staticCacheAvailable;
	result.renderDynamicGeometry = true;

	return result;
}

std::pair<shadow_render_command, spot_shadow_info> determineSpotShadowInfo(const spot_light_cb& spotLight, uint32 lightID, uint32 resolution, bool invalidateCache)
{
	uint64 uniqueID = ((uint64)(lightID + 1) << 32);

	uint64 movementHash = getLightMovementHash(spotLight);

	shadow_render_command result;

	auto [vp, staticCacheAvailable] = assignShadowMapViewport(uniqueID, movementHash, resolution);
	result.viewports[0] = vp;

	result.renderStaticGeometry = !staticCacheAvailable || invalidateCache;
	result.renderDynamicGeometry = true;

	spot_shadow_info si;
	si.viewport = vec4(vp.x, vp.y, vp.size, vp.size) / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);
	si.viewProj = getSpotLightViewProjectionMatrix(spotLight);
	si.bias = 0.00002f;
	return { result, si };
}

std::pair<shadow_render_command, point_shadow_info> determinePointShadowInfo(const point_light_cb& pointLight, uint32 lightID, uint32 resolution, bool invalidateCache)
{
	uint64 uniqueID = ((uint64)(lightID + 1) << 32);

	uint64 movementHash = getLightMovementHash(pointLight);

	shadow_render_command result;

	auto [vp0, staticCacheAvailable0] = assignShadowMapViewport(uniqueID, movementHash, resolution);
	auto [vp1, staticCacheAvailable1] = assignShadowMapViewport(uniqueID + 1, movementHash, resolution);
	result.viewports[0] = vp0;
	result.viewports[1] = vp1;

	result.renderStaticGeometry = !staticCacheAvailable0 || !staticCacheAvailable1 || invalidateCache;
	result.renderDynamicGeometry = true;

	point_shadow_info si;
	si.viewport0 = vec4(vp0.x, vp0.y, vp0.size, vp0.size) / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);
	si.viewport1 = vec4(vp1.x, vp1.y, vp1.size, vp1.size) / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);
	return { result, si };
}












static dx_pipeline shadowPipeline;
static dx_pipeline doubleSidedShadowPipeline;

static dx_pipeline pointLightShadowPipeline;
static dx_pipeline doubleSidedPointLightShadowPipeline;

void initializeShadowPipelines()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.renderTargets(0, 0, shadowDepthFormat)
		.inputLayout(inputLayout_position)
		//.cullFrontFaces()
		;

	shadowPipeline = createReloadablePipeline(desc, { "shadow_vs" }, rs_in_vertex_shader);
	pointLightShadowPipeline = createReloadablePipeline(desc, { "shadow_point_light_vs", "shadow_point_light_ps" }, rs_in_vertex_shader);

	desc.cullingOff();
	doubleSidedShadowPipeline = createReloadablePipeline(desc, { "shadow_vs" }, rs_in_vertex_shader);
	doubleSidedPointLightShadowPipeline = createReloadablePipeline(desc, { "shadow_point_light_vs", "shadow_point_light_ps" }, rs_in_vertex_shader);
}

PIPELINE_SETUP_IMPL(shadow_pipeline::single_sided)
{
	cl->setPipelineState(*shadowPipeline.pipeline);
	cl->setGraphicsRootSignature(*shadowPipeline.rootSignature);

	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

PIPELINE_SETUP_IMPL(shadow_pipeline::double_sided)
{
	cl->setPipelineState(*doubleSidedShadowPipeline.pipeline);
	cl->setGraphicsRootSignature(*doubleSidedShadowPipeline.rootSignature);

	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

PIPELINE_RENDER_IMPL(shadow_pipeline, shadow_render_data)
{
	cl->setRootGraphicsSRV(SHADOW_RS_TRANSFORMS, data.transformPtr);
	cl->setGraphics32BitConstants(SHADOW_RS_VIEWPROJ, viewProj);

	cl->setVertexBuffer(0, data.vertexBuffer);
	cl->setIndexBuffer(data.indexBuffer);

	cl->drawIndexed(data.submesh.numIndices, data.numInstances, data.submesh.firstIndex, data.submesh.baseVertex, 0);
}




PIPELINE_SETUP_IMPL(point_shadow_pipeline::single_sided)
{
	cl->setPipelineState(*pointLightShadowPipeline.pipeline);
	cl->setGraphicsRootSignature(*pointLightShadowPipeline.rootSignature);

	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

PIPELINE_SETUP_IMPL(point_shadow_pipeline::double_sided)
{
	cl->setPipelineState(*doubleSidedPointLightShadowPipeline.pipeline);
	cl->setGraphicsRootSignature(*doubleSidedPointLightShadowPipeline.rootSignature);

	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

PIPELINE_RENDER_IMPL(point_shadow_pipeline, shadow_render_data)
{
	cl->setRootGraphicsSRV(SHADOW_RS_TRANSFORMS, data.transformPtr);

	cl->setVertexBuffer(0, data.vertexBuffer);
	cl->setIndexBuffer(data.indexBuffer);

	cl->drawIndexed(data.submesh.numIndices, data.numInstances, data.submesh.firstIndex, data.submesh.baseVertex, 0);
}







