#include "pch.h"

#include "scene_rendering.h"

#include "rendering/pbr.h"
#include "rendering/depth_prepass.h"
#include "rendering/outline.h"
#include "rendering/shadow_map.h"

#include "geometry/mesh.h"

#include "dx/dx_context.h"

#include "physics/cloth.h"

#include "core/cpu_profiling.h"


struct offset_count
{
	uint32 offset;
	uint32 count;
};

enum light_frustum_type
{
	light_frustum_standard,
	light_frustum_sphere,
};

struct light_frustum
{
	union
	{
		camera_frustum_planes frustum;
		bounding_sphere sphere;
	};

	light_frustum_type type;
};

struct shadow_pass
{
	light_frustum frustum;
	shadow_render_pass_base* pass;
	bool isPointLight;
};

struct shadow_passes
{
	shadow_pass* shadowRenderPasses;
	uint32 numShadowRenderPasses;
};


static bool shouldRender(bounding_sphere s, bounding_box aabb, const trs& transform)
{
	s.center = conjugate(transform.rotation) * (s.center - transform.position) + transform.position;
	aabb.minCorner *= transform.scale;
	aabb.maxCorner *= transform.scale;
	return aabb.contains(s.center) || sphereVsAABB(s, aabb);
}

static bool shouldRender(const camera_frustum_planes& frustum, const mesh_component& mesh, const transform_component& transform)
{
	return mesh.mesh && ((mesh.mesh->aabb.maxCorner.x == mesh.mesh->aabb.minCorner.x) || !frustum.cullModelSpaceAABB(mesh.mesh->aabb, transform));
}

static bool shouldRender(const bounding_sphere& frustum, const mesh_component& mesh, const transform_component& transform)
{
	return mesh.mesh && ((mesh.mesh->aabb.maxCorner.x == mesh.mesh->aabb.minCorner.x) || shouldRender(frustum, mesh.mesh->aabb, transform));
}

static bool shouldRender(const light_frustum& frustum, const mesh_component& mesh, const transform_component& transform)
{
	return (frustum.type == light_frustum_standard) ? shouldRender(frustum.frustum, mesh, transform) : shouldRender(frustum.sphere, mesh, transform);
}

template <typename group_t>
std::unordered_map<multi_mesh*, offset_count> getOffsetsPerMesh(group_t group)
{
	uint32 groupSize = (uint32)group.size();

	std::unordered_map<multi_mesh*, offset_count> ocPerMesh;

	uint32 index = 0;
	for (entity_handle entityHandle : group)
	{
		mesh_component& mesh = group.get<mesh_component>(entityHandle);
		++ocPerMesh[mesh.mesh.get()].count;
	}

	uint32 offset = 0;
	for (auto& [mesh, oc] : ocPerMesh)
	{
		oc.offset = offset;
		offset += oc.count;
		oc.count = 0;
	}

	ocPerMesh.erase(nullptr);

	return ocPerMesh;
}

static void addToRenderPass(pbr_material_shader shader, const pbr_render_data& data, const depth_prepass_data& depthPrepassData,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass)
{
	switch (shader)
	{
		case pbr_material_shader_default:
		case pbr_material_shader_double_sided:
		{
			if (shader == pbr_material_shader_default)
			{
				opaqueRenderPass->renderObject<pbr_pipeline::opaque>(data);
				opaqueRenderPass->renderDepthOnly<depth_prepass_pipeline::single_sided>(depthPrepassData);
			}
			else
			{
				opaqueRenderPass->renderObject<pbr_pipeline::opaque_double_sided>(data);
				opaqueRenderPass->renderDepthOnly<depth_prepass_pipeline::double_sided>(depthPrepassData);
			}
		} break;
		case pbr_material_shader_alpha_cutout:
		{
			opaqueRenderPass->renderObject<pbr_pipeline::opaque_double_sided>(data);
			opaqueRenderPass->renderDepthOnly<depth_prepass_pipeline::alpha_cutout>(depthPrepassData);
		} break;
		case pbr_material_shader_transparent:
		{
			transparentRenderPass->renderObject<pbr_pipeline::transparent>(data);
		} break;
	}
}

static void addToStaticRenderPass(pbr_material_shader shader, const shadow_render_data& shadowData, shadow_render_pass_base* shadowRenderPass, bool isPointLight)
{
	switch (shader)
	{
		case pbr_material_shader_default:
		{
			if (isPointLight)
			{
				shadowRenderPass->renderStaticObject<point_shadow_pipeline::single_sided>(shadowData);
			}
			else
			{
				shadowRenderPass->renderStaticObject<shadow_pipeline::single_sided>(shadowData);
			}
		} break;
		case pbr_material_shader_double_sided:
		case pbr_material_shader_alpha_cutout:
		{
			if (isPointLight)
			{
				shadowRenderPass->renderStaticObject<point_shadow_pipeline::double_sided>(shadowData);
			}
			else
			{
				shadowRenderPass->renderStaticObject<shadow_pipeline::double_sided>(shadowData);
			}
		} break;
		case pbr_material_shader_transparent:
		{
			// Do nothing.
		} break;
	}
}

static void addToDynamicRenderPass(pbr_material_shader shader, const shadow_render_data& shadowData, shadow_render_pass_base* shadowRenderPass, bool isPointLight)
{
	switch (shader)
	{
		case pbr_material_shader_default:
		{
			if (isPointLight)
			{
				shadowRenderPass->renderDynamicObject<point_shadow_pipeline::single_sided>(shadowData);
			}
			else
			{
				shadowRenderPass->renderDynamicObject<shadow_pipeline::single_sided>(shadowData);
			}
		} break;
		case pbr_material_shader_double_sided:
		case pbr_material_shader_alpha_cutout:
		{
			if (isPointLight)
			{
				shadowRenderPass->renderDynamicObject<point_shadow_pipeline::double_sided>(shadowData);
			}
			else
			{
				shadowRenderPass->renderDynamicObject<shadow_pipeline::double_sided>(shadowData);
			}
		} break;
		case pbr_material_shader_transparent:
		{
			// Do nothing.
		} break;
	}
}


template <typename group_t>
static void renderStaticObjectsToMainCamera(group_t group, std::unordered_map<multi_mesh*, offset_count> ocPerMesh,
	const camera_frustum_planes& frustum, memory_arena& arena, entity_handle selectedObjectID,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass)
{
	uint32 groupSize = (uint32)group.size();

	dx_allocation transformAllocation = dxContext.allocateDynamicBuffer(groupSize * sizeof(mat4), 4);
	mat4* transforms = (mat4*)transformAllocation.cpuPtr;

	dx_allocation objectIDAllocation = dxContext.allocateDynamicBuffer(groupSize * sizeof(uint32), 4);
	uint32* objectIDs = (uint32*)objectIDAllocation.cpuPtr;

	for (auto [entityHandle, transform, mesh] : group.each())
	{
		if (!shouldRender(frustum, mesh, transform))
		{
			continue;
		}

		const dx_mesh& dxMesh = mesh.mesh->mesh;

		offset_count& oc = ocPerMesh.at(mesh.mesh.get());

		uint32 index = oc.offset + oc.count;
		transforms[index] = trsToMat4(transform);
		objectIDs[index] = (uint32)entityHandle;

		++oc.count;


		if (entityHandle == selectedObjectID)
		{
			for (auto& sm : mesh.mesh->submeshes)
			{
				renderOutline(ldrRenderPass, transforms[index], dxMesh.vertexBuffer, dxMesh.indexBuffer, sm.info);
			}
		}
	}


	D3D12_GPU_VIRTUAL_ADDRESS transformsAddress = transformAllocation.gpuPtr;
	D3D12_GPU_VIRTUAL_ADDRESS objectIDAddress = objectIDAllocation.gpuPtr;

	uint32 numDrawCalls = 0;

	for (auto& [mesh, oc] : ocPerMesh)
	{
		if (oc.count == 0)
		{
			continue;
		}

		D3D12_GPU_VIRTUAL_ADDRESS baseM = transformsAddress + (oc.offset * sizeof(mat4));
		D3D12_GPU_VIRTUAL_ADDRESS baseObjectID = objectIDAddress + (oc.offset * sizeof(uint32));

		const dx_mesh& dxMesh = mesh->mesh;

		pbr_render_data data;
		data.transformPtr = baseM;
		data.vertexBuffer = dxMesh.vertexBuffer;
		data.indexBuffer = dxMesh.indexBuffer;
		data.numInstances = oc.count;

		depth_prepass_data depthPrepassData;
		depthPrepassData.transformPtr = baseM;
		depthPrepassData.prevFrameTransformPtr = baseM;
		depthPrepassData.objectIDPtr = baseObjectID;
		depthPrepassData.vertexBuffer = dxMesh.vertexBuffer;
		depthPrepassData.prevFrameVertexBuffer = dxMesh.vertexBuffer.positions;
		depthPrepassData.indexBuffer = dxMesh.indexBuffer;
		depthPrepassData.numInstances = oc.count;

		for (auto& sm : mesh->submeshes)
		{
			data.submesh = sm.info;
			data.material = sm.material;

			depthPrepassData.submesh = data.submesh;
			depthPrepassData.alphaCutoutTextureSRV = (sm.material && sm.material->albedo) ? sm.material->albedo->defaultSRV : dx_cpu_descriptor_handle{};

			addToRenderPass(sm.material->shader, data, depthPrepassData, opaqueRenderPass, transparentRenderPass);

			++numDrawCalls;
		}
	}

	CPU_PROFILE_STAT("Static draw calls", numDrawCalls);
}

template <typename group_t>
static void renderStaticObjectsToShadowMap(group_t group, std::unordered_map<multi_mesh*, offset_count> ocPerMesh, 
	const light_frustum& frustum, memory_arena& arena, shadow_render_pass_base* shadowRenderPass)
{
	uint32 groupSize = (uint32)group.size();

	dx_allocation transformAllocation = dxContext.allocateDynamicBuffer(groupSize * sizeof(mat4), 4);
	mat4* transforms = (mat4*)transformAllocation.cpuPtr;

	for (auto [entityHandle, transform, mesh] : group.each())
	{
		if (!shouldRender(frustum, mesh, transform))
		{
			continue;
		}

		const dx_mesh& dxMesh = mesh.mesh->mesh;

		offset_count& oc = ocPerMesh.at(mesh.mesh.get());

		uint32 index = oc.offset + oc.count;
		transforms[index] = trsToMat4(transform);

		++oc.count;
	}


	D3D12_GPU_VIRTUAL_ADDRESS transformsAddress = transformAllocation.gpuPtr;

	for (auto& [mesh, oc] : ocPerMesh)
	{
		if (oc.count == 0)
		{
			continue;
		}

		D3D12_GPU_VIRTUAL_ADDRESS baseM = transformsAddress + (oc.offset * sizeof(mat4));

		const dx_mesh& dxMesh = mesh->mesh;

		shadow_render_data data;
		data.transformPtr = baseM;
		data.vertexBuffer = dxMesh.vertexBuffer.positions;
		data.indexBuffer = dxMesh.indexBuffer;
		data.numInstances = oc.count;

		for (auto& sm : mesh->submeshes)
		{
			data.submesh = sm.info;
			addToStaticRenderPass(sm.material->shader, data, shadowRenderPass, frustum.type == light_frustum_sphere);
		}
	}
}

static void renderStaticObjects(game_scene& scene, const camera_frustum_planes& frustum, memory_arena& arena, entity_handle selectedObjectID,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass, shadow_passes& shadow)
{
	CPU_PROFILE_BLOCK("Static objects");

	using specialized_components = component_group_t<
		animation_component,
		dynamic_transform_component,
		tree_component
	>;

	auto group = scene.group(
		component_group<transform_component, mesh_component>,
		specialized_components{});

	std::unordered_map<multi_mesh*, offset_count> ocPerMesh = getOffsetsPerMesh(group);
	

	renderStaticObjectsToMainCamera(group, ocPerMesh, frustum, arena, selectedObjectID, opaqueRenderPass, transparentRenderPass, ldrRenderPass);

	for (uint32 i = 0; i < shadow.numShadowRenderPasses; ++i)
	{
		auto& pass = shadow.shadowRenderPasses[i];
		renderStaticObjectsToShadowMap(group, ocPerMesh, pass.frustum, arena, pass.pass);
	}
}




template <typename group_t>
static void renderDynamicObjectsToMainCamera(group_t group, std::unordered_map<multi_mesh*, offset_count> ocPerMesh,
	const camera_frustum_planes& frustum, memory_arena& arena, entity_handle selectedObjectID,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass)
{
	uint32 groupSize = (uint32)group.size();

	dx_allocation transformAllocation = dxContext.allocateDynamicBuffer(groupSize * sizeof(mat4) * 2, 4);
	mat4* transforms = (mat4*)transformAllocation.cpuPtr;
	mat4* prevFrameTransforms = transforms + groupSize;

	dx_allocation objectIDAllocation = dxContext.allocateDynamicBuffer(groupSize * sizeof(uint32), 4);
	uint32* objectIDs = (uint32*)objectIDAllocation.cpuPtr;

	for (auto [entityHandle, transform, dynamicTransform, mesh] : group.each())
	{
		if (!shouldRender(frustum, mesh, transform))
		{
			continue;
		}

		const dx_mesh& dxMesh = mesh.mesh->mesh;

		offset_count& oc = ocPerMesh.at(mesh.mesh.get());

		uint32 index = oc.offset + oc.count;
		transforms[index] = trsToMat4(transform);
		prevFrameTransforms[index] = trsToMat4(dynamicTransform);
		objectIDs[index] = (uint32)entityHandle;

		++oc.count;


		if (entityHandle == selectedObjectID)
		{
			for (auto& sm : mesh.mesh->submeshes)
			{
				renderOutline(ldrRenderPass, transforms[index], dxMesh.vertexBuffer, dxMesh.indexBuffer, sm.info);
			}
		}
	}


	D3D12_GPU_VIRTUAL_ADDRESS transformsAddress = transformAllocation.gpuPtr;
	D3D12_GPU_VIRTUAL_ADDRESS prevFrameTransformsAddress = transformAllocation.gpuPtr + (groupSize * sizeof(mat4));
	D3D12_GPU_VIRTUAL_ADDRESS objectIDAddress = objectIDAllocation.gpuPtr;

	uint32 numDrawCalls = 0;

	for (auto& [mesh, oc] : ocPerMesh)
	{
		if (oc.count == 0)
		{
			continue;
		}

		D3D12_GPU_VIRTUAL_ADDRESS baseM = transformsAddress + (oc.offset * sizeof(mat4));
		D3D12_GPU_VIRTUAL_ADDRESS prevBaseM = prevFrameTransformsAddress + (oc.offset * sizeof(mat4));
		D3D12_GPU_VIRTUAL_ADDRESS baseObjectID = objectIDAddress + (oc.offset * sizeof(uint32));

		const dx_mesh& dxMesh = mesh->mesh;

		pbr_render_data data;
		data.transformPtr = baseM;
		data.vertexBuffer = dxMesh.vertexBuffer;
		data.indexBuffer = dxMesh.indexBuffer;
		data.numInstances = oc.count;

		depth_prepass_data depthPrepassData;
		depthPrepassData.transformPtr = baseM;
		depthPrepassData.prevFrameTransformPtr = prevBaseM;
		depthPrepassData.objectIDPtr = baseObjectID;
		depthPrepassData.vertexBuffer = dxMesh.vertexBuffer;
		depthPrepassData.prevFrameVertexBuffer = dxMesh.vertexBuffer.positions;
		depthPrepassData.indexBuffer = dxMesh.indexBuffer;
		depthPrepassData.numInstances = oc.count;

		for (auto& sm : mesh->submeshes)
		{
			data.submesh = sm.info;
			data.material = sm.material;

			depthPrepassData.submesh = data.submesh;
			depthPrepassData.alphaCutoutTextureSRV = (sm.material && sm.material->albedo) ? sm.material->albedo->defaultSRV : dx_cpu_descriptor_handle{};

			addToRenderPass(sm.material->shader, data, depthPrepassData, opaqueRenderPass, transparentRenderPass);

			++numDrawCalls;
		}
	}

	CPU_PROFILE_STAT("Dynamic draw calls", numDrawCalls);
}

template <typename group_t>
static void renderDynamicObjectsToShadowMap(group_t group, std::unordered_map<multi_mesh*, offset_count> ocPerMesh,
	const light_frustum& frustum, memory_arena& arena, shadow_render_pass_base* shadowRenderPass)
{
	uint32 groupSize = (uint32)group.size();

	dx_allocation transformAllocation = dxContext.allocateDynamicBuffer(groupSize * sizeof(mat4), 4);
	mat4* transforms = (mat4*)transformAllocation.cpuPtr;

	for (auto [entityHandle, transform, dynamicTransform, mesh] : group.each())
	{
		if (!shouldRender(frustum, mesh, transform))
		{
			continue;
		}

		const dx_mesh& dxMesh = mesh.mesh->mesh;

		offset_count& oc = ocPerMesh.at(mesh.mesh.get());

		uint32 index = oc.offset + oc.count;
		transforms[index] = trsToMat4(transform);

		++oc.count;
	}


	D3D12_GPU_VIRTUAL_ADDRESS transformsAddress = transformAllocation.gpuPtr;

	for (auto& [mesh, oc] : ocPerMesh)
	{
		if (oc.count == 0)
		{
			continue;
		}

		D3D12_GPU_VIRTUAL_ADDRESS baseM = transformsAddress + (oc.offset * sizeof(mat4));

		const dx_mesh& dxMesh = mesh->mesh;

		shadow_render_data data;
		data.transformPtr = baseM;
		data.vertexBuffer = dxMesh.vertexBuffer.positions;
		data.indexBuffer = dxMesh.indexBuffer;
		data.numInstances = oc.count;

		for (auto& sm : mesh->submeshes)
		{
			data.submesh = sm.info;
			addToDynamicRenderPass(sm.material->shader, data, shadowRenderPass, frustum.type == light_frustum_sphere);
		}
	}
}

static void renderDynamicObjects(game_scene& scene, const camera_frustum_planes& frustum, memory_arena& arena, entity_handle selectedObjectID,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass, shadow_passes& shadow)
{
	CPU_PROFILE_BLOCK("Dynamic objects");

	auto group = scene.group(
		component_group<transform_component, dynamic_transform_component, mesh_component>,
		component_group<animation_component>);


	std::unordered_map<multi_mesh*, offset_count> ocPerMesh = getOffsetsPerMesh(group);
	renderDynamicObjectsToMainCamera(group, ocPerMesh, frustum, arena, selectedObjectID, opaqueRenderPass, transparentRenderPass, ldrRenderPass);

	for (uint32 i = 0; i < shadow.numShadowRenderPasses; ++i)
	{
		auto& pass = shadow.shadowRenderPasses[i];
		renderDynamicObjectsToShadowMap(group, ocPerMesh, pass.frustum, arena, pass.pass);
	}
}

static void renderAnimatedObjects(game_scene& scene, const camera_frustum_planes& frustum, memory_arena& arena, entity_handle selectedObjectID,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass, shadow_passes& shadow)
{
	CPU_PROFILE_BLOCK("Animated objects");

	auto group = scene.group(
		component_group<transform_component, dynamic_transform_component, mesh_component, animation_component>);


	uint32 groupSize = (uint32)group.size();

	dx_allocation transformAllocation = dxContext.allocateDynamicBuffer(groupSize * sizeof(mat4) * 2, 4);
	mat4* transforms = (mat4*)transformAllocation.cpuPtr;
	mat4* prevFrameTransforms = transforms + groupSize;

	dx_allocation objectIDAllocation = dxContext.allocateDynamicBuffer(groupSize * sizeof(uint32), 4);
	uint32* objectIDs = (uint32*)objectIDAllocation.cpuPtr;

	D3D12_GPU_VIRTUAL_ADDRESS transformsAddress = transformAllocation.gpuPtr;
	D3D12_GPU_VIRTUAL_ADDRESS prevFrameTransformsAddress = transformAllocation.gpuPtr + (groupSize * sizeof(mat4));
	D3D12_GPU_VIRTUAL_ADDRESS objectIDAddress = objectIDAllocation.gpuPtr;


	uint32 index = 0;
	for (auto [entityHandle, transform, dynamicTransform, mesh, anim] : group.each())
	{
		if (!mesh.mesh)
		{
			continue;
		}

		transforms[index] = trsToMat4(transform);
		prevFrameTransforms[index] = trsToMat4(dynamicTransform);
		objectIDs[index] = (uint32)entityHandle;

		D3D12_GPU_VIRTUAL_ADDRESS baseM = transformsAddress + (index * sizeof(mat4));
		D3D12_GPU_VIRTUAL_ADDRESS prevBaseM = prevFrameTransformsAddress + (index * sizeof(mat4));
		D3D12_GPU_VIRTUAL_ADDRESS baseObjectID = objectIDAddress + (index * sizeof(uint32));


		const dx_mesh& dxMesh = mesh.mesh->mesh;

		pbr_render_data data;
		data.transformPtr = baseM;
		data.vertexBuffer = anim.currentVertexBuffer;
		data.indexBuffer = dxMesh.indexBuffer;
		data.numInstances = 1;

		depth_prepass_data depthPrepassData;
		depthPrepassData.transformPtr = baseM;
		depthPrepassData.prevFrameTransformPtr = prevBaseM;
		depthPrepassData.objectIDPtr = baseObjectID;
		depthPrepassData.vertexBuffer = anim.currentVertexBuffer;
		depthPrepassData.prevFrameVertexBuffer = anim.prevFrameVertexBuffer.positions ? anim.prevFrameVertexBuffer.positions : anim.currentVertexBuffer.positions;
		depthPrepassData.indexBuffer = dxMesh.indexBuffer;
		depthPrepassData.numInstances = 1;

		for (auto& sm : mesh.mesh->submeshes)
		{
			data.submesh = sm.info;
			data.material = sm.material;

			data.submesh.baseVertex -= mesh.mesh->submeshes[0].info.baseVertex; // Vertex buffer from skinning already points to first vertex.

			depthPrepassData.submesh = data.submesh;
			depthPrepassData.alphaCutoutTextureSRV = (sm.material && sm.material->albedo) ? sm.material->albedo->defaultSRV : dx_cpu_descriptor_handle{};

			addToRenderPass(sm.material->shader, data, depthPrepassData, opaqueRenderPass, transparentRenderPass);

			shadow_render_data shadowData;
			shadowData.transformPtr = baseM;
			shadowData.vertexBuffer = anim.currentVertexBuffer.positions;
			shadowData.indexBuffer = dxMesh.indexBuffer;
			shadowData.submesh = data.submesh;
			shadowData.numInstances = 1;

			for (uint32 i = 0; i < shadow.numShadowRenderPasses; ++i)
			{
				auto& pass = shadow.shadowRenderPasses[i];
				addToDynamicRenderPass(sm.material->shader, shadowData, pass.pass, pass.frustum.type == light_frustum_sphere);
			}

			if (entityHandle == selectedObjectID)
			{
				renderOutline(ldrRenderPass, transforms[index], anim.currentVertexBuffer, dxMesh.indexBuffer, data.submesh);
			}
		}
		
		++index;
	}
}

static void renderTerrain(const render_camera& camera, game_scene& scene, memory_arena& arena, entity_handle selectedObjectID,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass, sun_shadow_render_pass* sunShadowRenderPass,
	float dt)
{
	CPU_PROFILE_BLOCK("Terrain");

	memory_marker tempMemoryMarker = arena.getMarker();
	position_scale_component* waterPlaneTransforms = arena.allocate<position_scale_component>(scene.numberOfComponentsOfType<water_component>());
	uint32 numWaterPlanes = 0;

	for (auto [entityHandle, water, transform] : scene.group(component_group<water_component, position_scale_component>).each())
	{
		water.render(camera, transparentRenderPass, transform.position, vec2(transform.scale.x, transform.scale.z), dt, (uint32)entityHandle);

		waterPlaneTransforms[numWaterPlanes++] = transform;
	}

	for (auto [entityHandle, terrain, position] : scene.group(component_group<terrain_component, position_component>).each())
	{
		terrain.render(camera, opaqueRenderPass, sunShadowRenderPass, ldrRenderPass,
			position.position, (uint32)entityHandle, selectedObjectID == entityHandle, waterPlaneTransforms, numWaterPlanes);
	}
	arena.resetToMarker(tempMemoryMarker);
}

static void renderTrees(game_scene& scene, const camera_frustum_planes& frustum, memory_arena& arena, entity_handle selectedObjectID,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass, sun_shadow_render_pass* sunShadowRenderPass,
	float dt)
{
	CPU_PROFILE_BLOCK("Trees");

	auto group = scene.group(
		component_group<transform_component, mesh_component, tree_component>);

	uint32 groupSize = (uint32)group.size();

	std::unordered_map<multi_mesh*, offset_count> ocPerMesh = getOffsetsPerMesh(group);

	dx_allocation transformAllocation = dxContext.allocateDynamicBuffer(groupSize * sizeof(mat4), 4);
	mat4* transforms = (mat4*)transformAllocation.cpuPtr;

	dx_allocation objectIDAllocation = dxContext.allocateDynamicBuffer(groupSize * sizeof(uint32), 4);
	uint32* objectIDs = (uint32*)objectIDAllocation.cpuPtr;

	for (auto [entityHandle, transform, mesh, tree] : group.each())
	{
		if (!shouldRender(frustum, mesh, transform))
		{
			continue;
		}

		const dx_mesh& dxMesh = mesh.mesh->mesh;

		offset_count& oc = ocPerMesh.at(mesh.mesh.get());

		uint32 index = oc.offset + oc.count;
		transforms[index] = trsToMat4(transform);
		objectIDs[index] = (uint32)entityHandle;

		++oc.count;


		if (entityHandle == selectedObjectID)
		{
			for (auto& sm : mesh.mesh->submeshes)
			{
				renderOutline(ldrRenderPass, transforms[index], dxMesh.vertexBuffer, dxMesh.indexBuffer, sm.info);
			}
		}
	}


	D3D12_GPU_VIRTUAL_ADDRESS transformsAddress = transformAllocation.gpuPtr;
	D3D12_GPU_VIRTUAL_ADDRESS objectIDAddress = objectIDAllocation.gpuPtr;

	for (auto& [mesh, oc] : ocPerMesh)
	{
		if (oc.count == 0)
		{
			continue;
		}

		D3D12_GPU_VIRTUAL_ADDRESS baseM = transformsAddress + (oc.offset * sizeof(mat4));
		D3D12_GPU_VIRTUAL_ADDRESS baseObjectID = objectIDAddress + (oc.offset * sizeof(uint32));

		renderTree(opaqueRenderPass, baseM, oc.count, mesh, dt);
	}
}

static void renderCloth(game_scene& scene, entity_handle selectedObjectID, 
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass, sun_shadow_render_pass* sunShadowRenderPass)
{
	CPU_PROFILE_BLOCK("Cloth");

	auto group = scene.group(
		component_group<cloth_component, cloth_render_component>);

	uint32 groupSize = (uint32)group.size();

	dx_allocation transformAllocation = dxContext.allocateDynamicBuffer(1 * sizeof(mat4), 4);
	*(mat4*)transformAllocation.cpuPtr = mat4::identity;

	dx_allocation objectIDAllocation = dxContext.allocateDynamicBuffer(groupSize * sizeof(uint32), 4);
	uint32* objectIDs = (uint32*)objectIDAllocation.cpuPtr;

	D3D12_GPU_VIRTUAL_ADDRESS objectIDAddress = objectIDAllocation.gpuPtr;

	uint32 index = 0;
	for (auto [entityHandle, cloth, render] : scene.group<cloth_component, cloth_render_component>().each())
	{
		pbr_material_desc desc;
		desc.albedo = "assets/sponza/textures/Sponza_Curtain_Red_diffuse.tga";
		desc.normal = "assets/sponza/textures/Sponza_Curtain_Red_normal.tga";
		desc.roughness = "assets/sponza/textures/Sponza_Curtain_roughness.tga";
		desc.metallic = "assets/sponza/textures/Sponza_Curtain_metallic.tga";
		desc.shader = pbr_material_shader_double_sided;

		static auto clothMaterial = createPBRMaterial(desc);

		objectIDs[index] = (uint32)entityHandle;
		D3D12_GPU_VIRTUAL_ADDRESS baseObjectID = objectIDAddress + (index * sizeof(uint32));

		auto [vb, prevFrameVB, ib, sm] = render.getRenderData(cloth);

		pbr_render_data data;
		data.transformPtr = transformAllocation.gpuPtr;
		data.vertexBuffer = vb;
		data.indexBuffer = ib;
		data.submesh = sm;
		data.material = clothMaterial;
		data.numInstances = 1;

		depth_prepass_data depthPrepassData;
		depthPrepassData.transformPtr = transformAllocation.gpuPtr;
		depthPrepassData.prevFrameTransformPtr = transformAllocation.gpuPtr;
		depthPrepassData.objectIDPtr = baseObjectID;
		depthPrepassData.vertexBuffer = vb;
		depthPrepassData.prevFrameVertexBuffer = prevFrameVB.positions ? prevFrameVB.positions : vb.positions;
		depthPrepassData.indexBuffer = ib;
		depthPrepassData.submesh = sm;
		depthPrepassData.numInstances = 1;
		depthPrepassData.alphaCutoutTextureSRV = (clothMaterial && clothMaterial->albedo) ? clothMaterial->albedo->defaultSRV : dx_cpu_descriptor_handle{};

		addToRenderPass(clothMaterial->shader, data, depthPrepassData, opaqueRenderPass, transparentRenderPass);

		if (sunShadowRenderPass)
		{
			shadow_render_data shadowData;
			shadowData.transformPtr = transformAllocation.gpuPtr;
			shadowData.vertexBuffer = vb.positions;
			shadowData.indexBuffer = ib;
			shadowData.numInstances = 1;
			shadowData.submesh = data.submesh;

			addToDynamicRenderPass(clothMaterial->shader, shadowData, &sunShadowRenderPass->cascades[0], false);
		}

		if (entityHandle == selectedObjectID)
		{
			renderOutline(ldrRenderPass, mat4::identity, vb, ib, sm);
		}

		++index;
	}
}

static void setupSunShadowPass(directional_light& sun, sun_shadow_render_pass* sunShadowRenderPass, bool invalidateShadowMapCache)
{
	shadow_render_command command = determineSunShadowInfo(sun, invalidateShadowMapCache);
	sunShadowRenderPass->numCascades = sun.numShadowCascades;
	for (uint32 i = 0; i < sun.numShadowCascades; ++i)
	{
		sun_cascade_render_pass& cascadePass = sunShadowRenderPass->cascades[i];
		cascadePass.viewport = command.viewports[i];
		cascadePass.viewProj = sun.viewProjs[i];
	}
	sunShadowRenderPass->copyFromStaticCache = !command.renderStaticGeometry;
}

static void setupSpotShadowPasses(game_scene& scene, scene_lighting& lighting, bool invalidateShadowMapCache)
{
	uint32 numSpotLights = scene.numberOfComponentsOfType<spot_light_component>();
	if (numSpotLights)
	{
		auto* slPtr = (spot_light_cb*)mapBuffer(lighting.spotLightBuffer, false);
		auto* siPtr = (spot_shadow_info*)mapBuffer(lighting.spotLightShadowInfoBuffer, false);

		for (auto [entityHandle, transform, sl] : scene.group<position_rotation_component, spot_light_component>().each())
		{
			spot_light_cb cb(transform.position, transform.rotation * vec3(0.f, 0.f, -1.f), sl.color * sl.intensity, sl.innerAngle, sl.outerAngle, sl.distance);

			if (sl.castsShadow && lighting.numSpotShadowRenderPasses < lighting.maxNumSpotShadowRenderPasses)
			{
				cb.shadowInfoIndex = lighting.numSpotShadowRenderPasses++;

				auto [command, si] = determineSpotShadowInfo(cb, (uint32)entityHandle, sl.shadowMapResolution, invalidateShadowMapCache);
				spot_shadow_render_pass& pass = lighting.spotShadowRenderPasses[cb.shadowInfoIndex];

				pass.copyFromStaticCache = !command.renderStaticGeometry;
				pass.viewport = command.viewports[0];
				pass.viewProjMatrix = si.viewProj;

				*siPtr++ = si;
			}

			*slPtr++ = cb;
		}

		unmapBuffer(lighting.spotLightBuffer, true, { 0, numSpotLights });
		unmapBuffer(lighting.spotLightShadowInfoBuffer, true, { 0, lighting.numSpotShadowRenderPasses });
	}
}

static void setupPointShadowPasses(game_scene& scene, scene_lighting& lighting, bool invalidateShadowMapCache)
{
	uint32 numPointLights = scene.numberOfComponentsOfType<point_light_component>();
	if (numPointLights)
	{
		auto* plPtr = (point_light_cb*)mapBuffer(lighting.pointLightBuffer, false);
		auto* siPtr = (point_shadow_info*)mapBuffer(lighting.pointLightShadowInfoBuffer, false);

		for (auto [entityHandle, position, pl] : scene.group<position_component, point_light_component>().each())
		{
			point_light_cb cb(position.position, pl.color * pl.intensity, pl.radius);

			if (pl.castsShadow && lighting.numPointShadowRenderPasses < lighting.maxNumPointShadowRenderPasses)
			{
				cb.shadowInfoIndex = lighting.numPointShadowRenderPasses++;

				auto [command, si] = determinePointShadowInfo(cb, (uint32)entityHandle, pl.shadowMapResolution, invalidateShadowMapCache);
				point_shadow_render_pass& pass = lighting.pointShadowRenderPasses[cb.shadowInfoIndex];

				pass.copyFromStaticCache0 = !command.renderStaticGeometry;
				pass.copyFromStaticCache1 = !command.renderStaticGeometry;
				pass.viewport0 = command.viewports[0];
				pass.viewport1 = command.viewports[1];
				pass.lightPosition = cb.position;
				pass.maxDistance = cb.radius;

				*siPtr++ = si;
			}

			*plPtr++ = cb;
		}

		unmapBuffer(lighting.pointLightBuffer, true, { 0, numPointLights });
		unmapBuffer(lighting.pointLightShadowInfoBuffer, true, { 0, lighting.numPointShadowRenderPasses });
	}
}

void renderScene(const render_camera& camera, game_scene& scene, memory_arena& arena, entity_handle selectedObjectID,
	directional_light& sun, scene_lighting& lighting, bool invalidateShadowMapCache,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass, sun_shadow_render_pass* sunShadowRenderPass,
	float dt)
{
	CPU_PROFILE_BLOCK("Submit scene render commands");

	setupSunShadowPass(sun, sunShadowRenderPass, invalidateShadowMapCache);
	setupSpotShadowPasses(scene, lighting, invalidateShadowMapCache);
	setupPointShadowPasses(scene, lighting, invalidateShadowMapCache);

	shadow_passes staticShadowPasses = {};
	shadow_passes dynamicShadowPasses = {};

	memory_marker tempMemoryMarker = arena.getMarker();
	uint32 numShadowCastingLights = 1 + lighting.numSpotShadowRenderPasses + lighting.numPointShadowRenderPasses;
	staticShadowPasses.shadowRenderPasses = arena.allocate<shadow_pass>(numShadowCastingLights);
	dynamicShadowPasses.shadowRenderPasses = arena.allocate<shadow_pass>(numShadowCastingLights);

	{
		auto& outPass = dynamicShadowPasses.shadowRenderPasses[dynamicShadowPasses.numShadowRenderPasses++];
		outPass.frustum.frustum = getWorldSpaceFrustumPlanes(sunShadowRenderPass->cascades[sunShadowRenderPass->numCascades - 1].viewProj);
		outPass.frustum.type = light_frustum_standard;
		outPass.pass = &sunShadowRenderPass->cascades[0];

		if (!sunShadowRenderPass->copyFromStaticCache)
		{
			staticShadowPasses.shadowRenderPasses[staticShadowPasses.numShadowRenderPasses++] = outPass;
		}
	}

	for (uint32 i = 0; i < lighting.numSpotShadowRenderPasses; ++i)
	{
		auto pass = &lighting.spotShadowRenderPasses[i];
		auto& outPass = dynamicShadowPasses.shadowRenderPasses[dynamicShadowPasses.numShadowRenderPasses++];
		outPass.frustum.frustum = getWorldSpaceFrustumPlanes(pass->viewProjMatrix);
		outPass.frustum.type = light_frustum_standard;
		outPass.pass = pass;

		if (!pass->copyFromStaticCache)
		{
			staticShadowPasses.shadowRenderPasses[staticShadowPasses.numShadowRenderPasses++] = outPass;
		}
	}

	for (uint32 i = 0; i < lighting.numPointShadowRenderPasses; ++i)
	{
		auto pass = &lighting.pointShadowRenderPasses[i];
		auto& outPass = dynamicShadowPasses.shadowRenderPasses[dynamicShadowPasses.numShadowRenderPasses++];
		outPass.frustum.sphere = { pass->lightPosition, pass->maxDistance };
		outPass.frustum.type = light_frustum_sphere;
		outPass.pass = pass;

		if (!pass->copyFromStaticCache0)
		{
			staticShadowPasses.shadowRenderPasses[staticShadowPasses.numShadowRenderPasses++] = outPass;
		}
	}

	bool sunRenderStaticGeometry = !sunShadowRenderPass->copyFromStaticCache;


	camera_frustum_planes frustum = camera.getWorldSpaceFrustumPlanes();

	renderStaticObjects(scene, frustum, arena, selectedObjectID, opaqueRenderPass, transparentRenderPass, ldrRenderPass, staticShadowPasses);
	renderDynamicObjects(scene, frustum, arena, selectedObjectID, opaqueRenderPass, transparentRenderPass, ldrRenderPass, dynamicShadowPasses);
	renderAnimatedObjects(scene, frustum, arena, selectedObjectID, opaqueRenderPass, transparentRenderPass, ldrRenderPass, dynamicShadowPasses);
	renderTerrain(camera, scene, arena, selectedObjectID, opaqueRenderPass, transparentRenderPass, ldrRenderPass, sunRenderStaticGeometry ? sunShadowRenderPass : 0, dt);
	renderTrees(scene, frustum, arena, selectedObjectID, opaqueRenderPass, transparentRenderPass, ldrRenderPass, sunShadowRenderPass, dt);
	renderCloth(scene, selectedObjectID, opaqueRenderPass, transparentRenderPass, ldrRenderPass, sunShadowRenderPass);

	arena.resetToMarker(tempMemoryMarker);
}

