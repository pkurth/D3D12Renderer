#include "pch.h"
#include "shadow_map.h"
#include "render_resources.h"
#include "shadow_map_cache.h"

static void renderStaticGeometryToSunShadowMap(sun_shadow_render_pass* renderPass, scene& appScene)
{
	for (auto [entityHandle, raster, transform] : appScene.group(entt::get<raster_component, trs>, entt::exclude<dynamic_geometry_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (auto& sm : raster.mesh->submeshes)
		{
			renderPass->renderStaticObject(0, m, mesh.vertexBuffer, mesh.indexBuffer, sm.info);
		}
	}
}

static void renderDynamicGeometryToSunShadowMap(sun_shadow_render_pass* renderPass, scene& appScene)
{
	for (auto [entityHandle, raster, transform, dynamic] : appScene.group(entt::get<raster_component, trs, dynamic_geometry_component>, entt::exclude<animation_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (auto& sm : raster.mesh->submeshes)
		{
			renderPass->renderDynamicObject(0, m, mesh.vertexBuffer, mesh.indexBuffer, sm.info);
		}
	}

	for (auto [entityHandle, raster, anim, transform, dynamic] : appScene.group(entt::get<raster_component, animation_component, trs, dynamic_geometry_component>).each())
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

static void renderStaticGeometryToSpotShadowMap(spot_shadow_render_pass* renderPass, scene& appScene)
{
	for (auto [entityHandle, raster, transform] : appScene.group(entt::get<raster_component, trs>, entt::exclude<dynamic_geometry_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (auto& sm : raster.mesh->submeshes)
		{
			renderPass->renderStaticObject(m, mesh.vertexBuffer, mesh.indexBuffer, sm.info);
		}
	}
}

static void renderDynamicGeometryToSpotShadowMap(spot_shadow_render_pass* renderPass, scene& appScene)
{
	for (auto [entityHandle, raster, transform, dynamic] : appScene.group(entt::get<raster_component, trs, dynamic_geometry_component>, entt::exclude<animation_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (auto& sm : raster.mesh->submeshes)
		{
			renderPass->renderDynamicObject(m, mesh.vertexBuffer, mesh.indexBuffer, sm.info);
		}
	}

	for (auto [entityHandle, raster, anim, transform, dynamic] : appScene.group(entt::get<raster_component, animation_component, trs, dynamic_geometry_component>).each())
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

static void renderStaticGeometryToPointShadowMap(point_shadow_render_pass* renderPass, scene& appScene)
{
	for (auto [entityHandle, raster, transform] : appScene.group(entt::get<raster_component, trs>, entt::exclude<dynamic_geometry_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (auto& sm : raster.mesh->submeshes)
		{
			renderPass->renderStaticObject(m, mesh.vertexBuffer, mesh.indexBuffer, sm.info);
		}
	}
}

static void renderDynamicGeometryToPointShadowMap(point_shadow_render_pass* renderPass, scene& appScene)
{
	for (auto [entityHandle, raster, transform, dynamic] : appScene.group(entt::get<raster_component, trs, dynamic_geometry_component>, entt::exclude<animation_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (auto& sm : raster.mesh->submeshes)
		{
			renderPass->renderDynamicObject(m, mesh.vertexBuffer, mesh.indexBuffer, sm.info);
		}
	}

	for (auto [entityHandle, raster, anim, transform, dynamic] : appScene.group(entt::get<raster_component, animation_component, trs, dynamic_geometry_component>).each())
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





void renderSunShadowMap(directional_light& sun, sun_shadow_render_pass* renderPass, scene& appScene, bool invalidateCache)
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
		renderStaticGeometryToSunShadowMap(renderPass, appScene);
	}

	renderDynamicGeometryToSunShadowMap(renderPass, appScene);
}

spot_shadow_info renderSpotShadowMap(spot_light_cb& spotLight, uint32 lightIndex, spot_shadow_render_pass* renderPass, uint32 renderPassIndex, scene& appScene, bool invalidateCache)
{
	uint32 uniqueID = lightIndex + 1;

	spotLight.shadowInfoIndex = renderPassIndex;
	renderPass->viewProjMatrix = getSpotLightViewProjectionMatrix(spotLight);

	uint64 movementHash = getLightMovementHash(spotLight);

	auto [vp, staticCacheAvailable] = assignShadowMapViewport(uniqueID << 10, movementHash, 512);
	renderPass->viewport = vp;

	if (staticCacheAvailable && !invalidateCache)
	{
		renderPass->copyFromStaticCache = true;
	}
	else
	{
		renderStaticGeometryToSpotShadowMap(renderPass, appScene);
	}

	renderDynamicGeometryToSpotShadowMap(renderPass, appScene);

	spot_shadow_info si;
	si.viewport = vec4(vp.x, vp.y, vp.size, vp.size) / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);
	si.viewProj = renderPass->viewProjMatrix;
	si.bias = 0.00002f;
	return si;
}

point_shadow_info renderPointShadowMap(point_light_cb& pointLight, uint32 lightIndex, point_shadow_render_pass* renderPass, uint32 renderPassIndex, scene& appScene, bool invalidateCache)
{
	uint32 uniqueID = lightIndex + 1;

	pointLight.shadowInfoIndex = renderPassIndex;
	renderPass->lightPosition = pointLight.position;
	renderPass->maxDistance = pointLight.radius;

	uint64 movementHash = getLightMovementHash(pointLight);

	auto [vp0, staticCacheAvailable0] = assignShadowMapViewport((2 * uniqueID + 0) << 20, movementHash, 512);
	auto [vp1, staticCacheAvailable1] = assignShadowMapViewport((2 * uniqueID + 1) << 20, movementHash, 512);
	renderPass->viewport0 = vp0;
	renderPass->viewport1 = vp1;

	if (staticCacheAvailable0 && staticCacheAvailable1 && !invalidateCache) // TODO: When we separate the two hemispheres, this needs to get handled differently.
	{
		renderPass->copyFromStaticCache0 = true;
		renderPass->copyFromStaticCache1 = true;
	}
	else
	{
		renderStaticGeometryToPointShadowMap(renderPass, appScene);
	}

	renderDynamicGeometryToPointShadowMap(renderPass, appScene);


	point_shadow_info si;
	si.viewport0 = vec4(vp0.x, vp0.y, vp0.size, vp0.size) / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);
	si.viewport1 = vec4(vp1.x, vp1.y, vp1.size, vp1.size) / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);
	return si;
}

