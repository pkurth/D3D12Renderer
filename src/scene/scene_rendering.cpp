#include "pch.h"

#include "scene_rendering.h"

#include "rendering/pbr.h"
#include "rendering/depth_prepass.h"
#include "rendering/outline.h"
#include "rendering/shadow_map.h"

#include "geometry/mesh.h"

#include "dx/dx_context.h"

#include "physics/cloth.h"


struct offset_count
{
	uint32 offset;
	uint32 count;
};


template <typename group_t>
std::unordered_map<multi_mesh*, offset_count> getOffsetsPerMesh(group_t group)
{
	uint32 groupSize = (uint32)group.size();

	std::unordered_map<multi_mesh*, offset_count> ocPerMesh;
	//ocPerMesh.reserve(groupSize); // Probably overkill.

	for (entity_handle entityHandle : group)
	{
		auto& mesh = group.get<mesh_component>(entityHandle);

		if (!mesh.mesh)
		{
			continue;
		}

		++ocPerMesh[mesh.mesh.get()].count;
	}

	uint32 offset = 0;
	for (auto& [mesh, oc] : ocPerMesh)
	{
		oc.offset = offset;
		offset += oc.count;
		oc.count = 0;
	}

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

static void renderStaticObjects(game_scene& scene, memory_arena& arena, entity_handle selectedObjectID,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass, sun_shadow_render_pass* sunShadowRenderPass)
{
	using specialized_components = component_group_t<
		animation_component,
		dynamic_transform_component,
		tree_component
	>;

	auto group = scene.group(
		component_group<transform_component, mesh_component>,
		specialized_components{});


	std::unordered_map<multi_mesh*, offset_count> ocPerMesh = getOffsetsPerMesh(group);

	uint32 groupSize = (uint32)group.size();

	dx_allocation transformAllocation = dxContext.allocateDynamicBuffer(groupSize * sizeof(mat4), 4);
	mat4* transforms = (mat4*)transformAllocation.cpuPtr;

	dx_allocation objectIDAllocation = dxContext.allocateDynamicBuffer(groupSize * sizeof(uint32), 4);
	uint32* objectIDs = (uint32*)objectIDAllocation.cpuPtr;

	for (auto [entityHandle, transform, mesh] : group.each())
	{
		if (!mesh.mesh)
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

			addToRenderPass(sm.material->shader, data, depthPrepassData, opaqueRenderPass, transparentRenderPass);

			if (sunShadowRenderPass)
			{
				shadow_render_data shadowData;
				shadowData.transformPtr = baseM;
				shadowData.vertexBuffer = dxMesh.vertexBuffer.positions;
				shadowData.indexBuffer = dxMesh.indexBuffer;
				shadowData.submesh = data.submesh;
				shadowData.numInstances = oc.count;

				sunShadowRenderPass->renderStaticObject<shadow_pipeline::single_sided>(0, shadowData);
			}
		}
	}
}

static void renderDynamicObjects(game_scene& scene, memory_arena& arena, entity_handle selectedObjectID,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass, sun_shadow_render_pass* sunShadowRenderPass)
{
	auto group = scene.group(
		component_group<transform_component, dynamic_transform_component, mesh_component>,
		component_group<animation_component>);


	std::unordered_map<multi_mesh*, offset_count> ocPerMesh = getOffsetsPerMesh(group);

	uint32 groupSize = (uint32)group.size();

	dx_allocation transformAllocation = dxContext.allocateDynamicBuffer(groupSize * sizeof(mat4) * 2, 4);
	mat4* transforms = (mat4*)transformAllocation.cpuPtr;
	mat4* prevFrameTransforms = transforms + groupSize;

	dx_allocation objectIDAllocation = dxContext.allocateDynamicBuffer(groupSize * sizeof(uint32), 4);
	uint32* objectIDs = (uint32*)objectIDAllocation.cpuPtr;

	for (auto [entityHandle, transform, dynamicTransform, mesh] : group.each())
	{
		if (!mesh.mesh)
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

	for (auto& [mesh, oc] : ocPerMesh)
	{
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

			addToRenderPass(sm.material->shader, data, depthPrepassData, opaqueRenderPass, transparentRenderPass);

			if (sunShadowRenderPass)
			{
				shadow_render_data shadowData;
				shadowData.transformPtr = baseM;
				shadowData.vertexBuffer = dxMesh.vertexBuffer.positions;
				shadowData.indexBuffer = dxMesh.indexBuffer;
				shadowData.submesh = data.submesh;
				shadowData.numInstances = oc.count;

				sunShadowRenderPass->renderDynamicObject<shadow_pipeline::single_sided>(0, shadowData);
			}
		}
	}
}

static void renderAnimatedObjects(game_scene& scene, memory_arena& arena, entity_handle selectedObjectID,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass, sun_shadow_render_pass* sunShadowRenderPass)
{
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

			addToRenderPass(sm.material->shader, data, depthPrepassData, opaqueRenderPass, transparentRenderPass);

			if (sunShadowRenderPass)
			{
				shadow_render_data shadowData;
				shadowData.transformPtr = baseM;
				shadowData.vertexBuffer = anim.currentVertexBuffer.positions;
				shadowData.indexBuffer = dxMesh.indexBuffer;
				shadowData.submesh = data.submesh;
				shadowData.numInstances = 1;

				sunShadowRenderPass->renderDynamicObject<shadow_pipeline::single_sided>(0, shadowData);
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

static void renderTrees(game_scene& scene, memory_arena& arena, entity_handle selectedObjectID,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass, sun_shadow_render_pass* sunShadowRenderPass,
	float dt)
{
	auto group = scene.group(
		component_group<transform_component, mesh_component, tree_component>);


	std::unordered_map<multi_mesh*, offset_count> ocPerMesh = getOffsetsPerMesh(group);

	uint32 groupSize = (uint32)group.size();

	dx_allocation transformAllocation = dxContext.allocateDynamicBuffer(groupSize * sizeof(mat4), 4);
	mat4* transforms = (mat4*)transformAllocation.cpuPtr;

	dx_allocation objectIDAllocation = dxContext.allocateDynamicBuffer(groupSize * sizeof(uint32), 4);
	uint32* objectIDs = (uint32*)objectIDAllocation.cpuPtr;

	for (auto [entityHandle, transform, mesh, tree] : group.each())
	{
		if (!mesh.mesh)
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
		D3D12_GPU_VIRTUAL_ADDRESS baseM = transformsAddress + (oc.offset * sizeof(mat4));
		D3D12_GPU_VIRTUAL_ADDRESS baseObjectID = objectIDAddress + (oc.offset * sizeof(uint32));

		renderTree(opaqueRenderPass, baseM, oc.count, mesh, dt);
	}
}

static void renderCloth(game_scene& scene, entity_handle selectedObjectID, 
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass, sun_shadow_render_pass* sunShadowRenderPass)
{
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
		static auto clothMaterial = createPBRMaterial(
			"assets/sponza/textures/Sponza_Curtain_Red_diffuse.tga",
			"assets/sponza/textures/Sponza_Curtain_Red_normal.tga",
			"assets/sponza/textures/Sponza_Curtain_roughness.tga",
			"assets/sponza/textures/Sponza_Curtain_metallic.tga",
			vec4(0.f), vec4(1.f), 1.f, 1.f, pbr_material_shader_double_sided);

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

		addToRenderPass(clothMaterial->shader, data, depthPrepassData, opaqueRenderPass, transparentRenderPass);

		if (sunShadowRenderPass)
		{
			shadow_render_data shadowData;
			shadowData.transformPtr = transformAllocation.gpuPtr;
			shadowData.vertexBuffer = vb.positions;
			shadowData.indexBuffer = ib;
			shadowData.numInstances = 1;
			shadowData.submesh = data.submesh;

			sunShadowRenderPass->renderDynamicObject<shadow_pipeline::double_sided>(0, shadowData);
		}

		if (entityHandle == selectedObjectID)
		{
			renderOutline(ldrRenderPass, mat4::identity, vb, ib, sm);
		}

		++index;
	}
}

static shadow_render_command setupSunShadowPass(directional_light& sun, bool invalidateShadowMapCache, sun_shadow_render_pass* sunShadowRenderPass)
{
	shadow_render_command result = determineSunShadowInfo(sun, invalidateShadowMapCache);
	sunShadowRenderPass->numCascades = sun.numShadowCascades;
	for (uint32 i = 0; i < sun.numShadowCascades; ++i)
	{
		sun_cascade_render_pass& cascadePass = sunShadowRenderPass->cascades[i];
		cascadePass.viewport = result.viewports[i];
		cascadePass.viewProj = sun.viewProjs[i];
	}
	sunShadowRenderPass->copyFromStaticCache = !result.renderStaticGeometry;
	return result;
}

void renderScene(const render_camera& camera, game_scene& scene, memory_arena& arena, entity_handle selectedObjectID, directional_light& sun, bool invalidateShadowMapCache,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass, sun_shadow_render_pass* sunShadowRenderPass,
	float dt)
{
	CPU_PROFILE_BLOCK("Submit scene render commands");

	shadow_render_command sunShadow = setupSunShadowPass(sun, invalidateShadowMapCache, sunShadowRenderPass);

	ASSERT(sunShadow.renderDynamicGeometry);

	renderStaticObjects(scene, arena, selectedObjectID, opaqueRenderPass, transparentRenderPass, ldrRenderPass, sunShadow.renderStaticGeometry ? sunShadowRenderPass : 0);
	renderDynamicObjects(scene, arena, selectedObjectID, opaqueRenderPass, transparentRenderPass, ldrRenderPass, sunShadowRenderPass);
	renderAnimatedObjects(scene, arena, selectedObjectID, opaqueRenderPass, transparentRenderPass, ldrRenderPass, sunShadowRenderPass);
	renderTerrain(camera, scene, arena, selectedObjectID, opaqueRenderPass, transparentRenderPass, ldrRenderPass, sunShadow.renderStaticGeometry ? sunShadowRenderPass : 0, dt);
	renderTrees(scene, arena, selectedObjectID, opaqueRenderPass, transparentRenderPass, ldrRenderPass, sunShadowRenderPass, dt);
	renderCloth(scene, selectedObjectID, opaqueRenderPass, transparentRenderPass, ldrRenderPass, sunShadowRenderPass);
}

