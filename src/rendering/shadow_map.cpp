#include "pch.h"
#include "shadow_map.h"
#include "render_resources.h"
#include "shadow_map_cache.h"

static void renderStaticGeometryToSunShadowMap(sun_shadow_render_pass* renderPass, game_scene& scene)
{
	for (auto [entityHandle, raster, transform] : 
		scene.group(entt::get<raster_component, transform_component>, entt::exclude<dynamic_transform_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (auto& sm : raster.mesh->submeshes)
		{
			renderPass->renderStaticObject(0, m, mesh.vertexBuffer, mesh.indexBuffer, sm.info);
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
			renderPass->renderDynamicObject(0, m, mesh.vertexBuffer, mesh.indexBuffer, sm.info);
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
			renderPass->renderDynamicObject(0, m, anim.currentVertexBuffer, mesh.indexBuffer, submesh);
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
			renderPass->renderStaticObject(m, mesh.vertexBuffer, mesh.indexBuffer, sm.info);
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
			renderPass->renderDynamicObject(m, mesh.vertexBuffer, mesh.indexBuffer, sm.info);
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
			renderPass->renderDynamicObject(m, anim.currentVertexBuffer, mesh.indexBuffer, submesh);
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
			renderPass->renderStaticObject(m, mesh.vertexBuffer, mesh.indexBuffer, sm.info);
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
			renderPass->renderDynamicObject(m, mesh.vertexBuffer, mesh.indexBuffer, sm.info);
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
			renderPass->renderDynamicObject(m, anim.currentVertexBuffer, mesh.indexBuffer, submesh);
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

