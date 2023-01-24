#include "pch.h"
#include "shadow_map.h"
#include "render_resources.h"
#include "shadow_map_cache.h"

#include "depth_only_rs.hlsli"

static void renderStaticGeometryToSunShadowMap(sun_shadow_render_pass* renderPass, game_scene& scene)
{
	for (auto [entityHandle, raster, transform] : 
		scene.group(entt::get<raster_component, transform_component>, entt::exclude<dynamic_transform_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (auto& sm : raster.mesh->submeshes)
		{
			shadow_render_data data = { m, mesh.vertexBuffer.positions, mesh.indexBuffer, sm.info };
			renderPass->renderStaticObject<shadow_pipeline::single_sided>(0, data);
		}
	}
}

static void renderDynamicGeometryToSunShadowMap(sun_shadow_render_pass* renderPass, game_scene& scene)
{
	for (auto [entityHandle, raster, transform, dynamic] : 
		scene.group(entt::get<raster_component, transform_component, dynamic_transform_component>, entt::exclude<animation_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (auto& sm : raster.mesh->submeshes)
		{
			shadow_render_data data = { m, mesh.vertexBuffer.positions, mesh.indexBuffer, sm.info };
			renderPass->renderDynamicObject<shadow_pipeline::single_sided>(0, data);
		}
	}

	for (auto [entityHandle, raster, anim, transform, dynamic] :
		scene.group(entt::get<raster_component, animation_component, transform_component, dynamic_transform_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (uint32 i = 0; i < (uint32)raster.mesh->submeshes.size(); ++i)
		{
			auto submesh = raster.mesh->submeshes[i].info;
			submesh.baseVertex -= raster.mesh->submeshes[0].info.baseVertex; // Vertex buffer from skinning already points to first vertex.
			
			shadow_render_data data = { m, anim.currentVertexBuffer.positions, mesh.indexBuffer, submesh };
			renderPass->renderDynamicObject<shadow_pipeline::single_sided>(0, data);
		}
	}
}

static void renderStaticGeometryToSpotShadowMap(spot_shadow_render_pass* renderPass, game_scene& scene)
{
	for (auto [entityHandle, raster, transform] : 
		scene.group(entt::get<raster_component, transform_component>, entt::exclude<dynamic_transform_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (auto& sm : raster.mesh->submeshes)
		{
			shadow_render_data data = { m, mesh.vertexBuffer.positions, mesh.indexBuffer, sm.info };
			renderPass->renderStaticObject<shadow_pipeline::single_sided>(data);
		}
	}
}

static void renderDynamicGeometryToSpotShadowMap(spot_shadow_render_pass* renderPass, game_scene& scene)
{
	for (auto [entityHandle, raster, transform, dynamic] : 
		scene.group(entt::get<raster_component, transform_component, dynamic_transform_component>, entt::exclude<animation_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (auto& sm : raster.mesh->submeshes)
		{
			shadow_render_data data = { m, mesh.vertexBuffer.positions, mesh.indexBuffer, sm.info };
			renderPass->renderDynamicObject<shadow_pipeline::single_sided>(data);
		}
	}

	for (auto [entityHandle, raster, anim, transform, dynamic] : 
		scene.group(entt::get<raster_component, animation_component, transform_component, dynamic_transform_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (uint32 i = 0; i < (uint32)raster.mesh->submeshes.size(); ++i)
		{
			auto submesh = raster.mesh->submeshes[i].info;
			submesh.baseVertex -= raster.mesh->submeshes[0].info.baseVertex; // Vertex buffer from skinning already points to first vertex.

			shadow_render_data data = { m, anim.currentVertexBuffer.positions, mesh.indexBuffer, submesh };
			renderPass->renderDynamicObject<shadow_pipeline::single_sided>(data);
		}
	}
}

static void renderStaticGeometryToPointShadowMap(point_shadow_render_pass* renderPass, game_scene& scene)
{
	for (auto [entityHandle, raster, transform] : 
		scene.group(entt::get<raster_component, transform_component>, entt::exclude<dynamic_transform_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (auto& sm : raster.mesh->submeshes)
		{
			shadow_render_data data = { m, mesh.vertexBuffer.positions, mesh.indexBuffer, sm.info };
			renderPass->renderStaticObject<point_shadow_pipeline::single_sided>(data);
		}
	}
}

static void renderDynamicGeometryToPointShadowMap(point_shadow_render_pass* renderPass, game_scene& scene)
{
	for (auto [entityHandle, raster, transform, dynamic] : 
		scene.group(entt::get<raster_component, transform_component, dynamic_transform_component>, entt::exclude<animation_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (auto& sm : raster.mesh->submeshes)
		{
			shadow_render_data data = { m, mesh.vertexBuffer.positions, mesh.indexBuffer, sm.info };
			renderPass->renderDynamicObject<point_shadow_pipeline::single_sided>(data);
		}
	}

	for (auto [entityHandle, raster, anim, transform, dynamic] : 
		scene.group(entt::get<raster_component, animation_component, transform_component, dynamic_transform_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (uint32 i = 0; i < (uint32)raster.mesh->submeshes.size(); ++i)
		{
			auto submesh = raster.mesh->submeshes[i].info;
			submesh.baseVertex -= raster.mesh->submeshes[0].info.baseVertex; // Vertex buffer from skinning already points to first vertex.

			shadow_render_data data = { m, anim.currentVertexBuffer.positions, mesh.indexBuffer, submesh };
			renderPass->renderDynamicObject<point_shadow_pipeline::single_sided>(data);
		}
	}
}





void renderSunShadowMap(directional_light& sun, sun_shadow_render_pass* renderPass, game_scene& scene, bool invalidateCache)
{
	bool staticCacheAvailable = !invalidateCache;

	renderPass->numCascades = sun.numShadowCascades;

	uint64 movementHash = getLightMovementHash(sun);

	for (uint32 i = 0; i < sun.numShadowCascades; ++i)
	{
		auto [vp, cache] = assignShadowMapViewport(i, movementHash, sun.shadowDimensions);

		sun.shadowMapViewports[i] = vec4(vp.x, vp.y, vp.size, vp.size) / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);

		sun_cascade_render_pass& cascadePass = renderPass->cascades[i];
		cascadePass.viewport = vp;
		cascadePass.viewProj = sun.viewProjs[i];

		staticCacheAvailable &= cache;
	}

	if (staticCacheAvailable)
	{
		renderPass->copyFromStaticCache = true;
	}
	else
	{
		renderStaticGeometryToSunShadowMap(renderPass, scene);
	}

	renderDynamicGeometryToSunShadowMap(renderPass, scene);
}

spot_shadow_info renderSpotShadowMap(const spot_light_cb& spotLight, uint32 lightID, spot_shadow_render_pass* renderPass, game_scene& scene, bool invalidateCache, uint32 resolution)
{
	uint64 uniqueID = ((uint64)(lightID + 1) << 32);

	renderPass->viewProjMatrix = getSpotLightViewProjectionMatrix(spotLight);

	uint64 movementHash = getLightMovementHash(spotLight);

	auto [vp, staticCacheAvailable] = assignShadowMapViewport(uniqueID, movementHash, resolution);
	renderPass->viewport = vp;

	if (staticCacheAvailable && !invalidateCache)
	{
		renderPass->copyFromStaticCache = true;
	}
	else
	{
		renderStaticGeometryToSpotShadowMap(renderPass, scene);
	}

	renderDynamicGeometryToSpotShadowMap(renderPass, scene);

	spot_shadow_info si;
	si.viewport = vec4(vp.x, vp.y, vp.size, vp.size) / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);
	si.viewProj = renderPass->viewProjMatrix;
	si.bias = 0.00002f;
	return si;
}

point_shadow_info renderPointShadowMap(const point_light_cb& pointLight, uint32 lightID, point_shadow_render_pass* renderPass, game_scene& scene, bool invalidateCache, uint32 resolution)
{
	uint64 uniqueID = ((uint64)(lightID + 1) << 32);

	renderPass->lightPosition = pointLight.position;
	renderPass->maxDistance = pointLight.radius;

	uint64 movementHash = getLightMovementHash(pointLight);

	auto [vp0, staticCacheAvailable0] = assignShadowMapViewport(uniqueID, movementHash, resolution);
	auto [vp1, staticCacheAvailable1] = assignShadowMapViewport(uniqueID + 1, movementHash, resolution);
	renderPass->viewport0 = vp0;
	renderPass->viewport1 = vp1;

	if (staticCacheAvailable0 && staticCacheAvailable1 && !invalidateCache) // TODO: When we separate the two hemispheres, this needs to get handled differently.
	{
		renderPass->copyFromStaticCache0 = true;
		renderPass->copyFromStaticCache1 = true;
	}
	else
	{
		renderStaticGeometryToPointShadowMap(renderPass, scene);
	}

	renderDynamicGeometryToPointShadowMap(renderPass, scene);


	point_shadow_info si;
	si.viewport0 = vec4(vp0.x, vp0.y, vp0.size, vp0.size) / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);
	si.viewport1 = vec4(vp1.x, vp1.y, vp1.size, vp1.size) / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);
	return si;
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

PIPELINE_RENDER_IMPL(shadow_pipeline)
{
	cl->setGraphics32BitConstants(SHADOW_RS_MVP, viewProj * rc.data.transform);

	cl->setVertexBuffer(0, rc.data.vertexBuffer);
	cl->setIndexBuffer(rc.data.indexBuffer);

	cl->drawIndexed(rc.data.submesh.numIndices, 1, rc.data.submesh.firstIndex, rc.data.submesh.baseVertex, 0);
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

PIPELINE_RENDER_IMPL(point_shadow_pipeline)
{
	cl->setGraphics32BitConstants(SHADOW_RS_MVP, rc.data.transform);

	cl->setVertexBuffer(0, rc.data.vertexBuffer);
	cl->setIndexBuffer(rc.data.indexBuffer);

	cl->drawIndexed(rc.data.submesh.numIndices, 1, rc.data.submesh.firstIndex, rc.data.submesh.baseVertex, 0);
}







