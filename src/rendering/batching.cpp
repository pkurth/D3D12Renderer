#include "pch.h"

#include "batching.h"
#include "pbr.h"
#include "depth_prepass.h"
#include "outline.h"

#include "geometry/mesh.h"

#include "dx/dx_context.h"


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
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass)
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
		}
	}
}

static void renderDynamicObjects(game_scene& scene, memory_arena& arena, entity_handle selectedObjectID,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass)
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
		}
	}
}

static void renderAnimatedObjects(game_scene& scene, memory_arena& arena, entity_handle selectedObjectID,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass)
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
		}


		if (entityHandle == selectedObjectID)
		{
			for (auto& sm : mesh.mesh->submeshes)
			{
				submesh_info submesh = sm.info;
				submesh.baseVertex -= mesh.mesh->submeshes[0].info.baseVertex; // Vertex buffer from skinning already points to first vertex.

				renderOutline(ldrRenderPass, transforms[index], anim.currentVertexBuffer, dxMesh.indexBuffer, submesh);
			}
		}
		
		++index;
	}
}

void renderScene(game_scene& scene, memory_arena& arena, entity_handle selectedObjectID,
	opaque_render_pass* opaqueRenderPass, transparent_render_pass* transparentRenderPass, ldr_render_pass* ldrRenderPass)
{
	renderStaticObjects(scene, arena, selectedObjectID, opaqueRenderPass, transparentRenderPass, ldrRenderPass);
	renderDynamicObjects(scene, arena, selectedObjectID, opaqueRenderPass, transparentRenderPass, ldrRenderPass);
	renderAnimatedObjects(scene, arena, selectedObjectID, opaqueRenderPass, transparentRenderPass, ldrRenderPass);
}

