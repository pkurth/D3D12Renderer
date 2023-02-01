#include "pch.h"
#include "proc_placement.h"

#include "core/log.h"

#include "dx/dx_command_list.h"
#include "dx/dx_pipeline.h"
#include "dx/dx_barrier_batcher.h"
#include "dx/dx_profiling.h"

#include "geometry/mesh_builder.h"

#include "rendering/render_utils.h"
#include "rendering/material.h"
#include "rendering/render_pass.h"

#include "proc_placement_rs.hlsli"

static dx_pipeline generatePointsPipeline;
static dx_pipeline prefixSumPipeline;
static dx_pipeline createDrawCallsPipeline;
static dx_pipeline createTransformsPipeline;

static dx_pipeline visualizePointsPipeline;

static dx_mesh visualizePointsMesh;
static submesh_info visualizePointsSubmesh;
static dx_command_signature visualizePointsCommandSignature;


void initializeProceduralPlacementPipelines()
{
	generatePointsPipeline = createReloadablePipeline("proc_placement_generate_points_cs");
	prefixSumPipeline = createReloadablePipeline("proc_placement_prefix_sum_cs");
	createDrawCallsPipeline = createReloadablePipeline("proc_placement_create_draw_calls_cs");
	createTransformsPipeline = createReloadablePipeline("proc_placement_create_transforms_cs");

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position)
			.renderTargets(ldrFormat, depthStencilFormat);

		visualizePointsPipeline = createReloadablePipeline(desc, { "proc_placement_points_vs", "proc_placement_points_ps" }, rs_in_pixel_shader, true);
	}


	mesh_builder builder(mesh_creation_flags_with_positions);
	//builder.pushSphere({});
	builder.pushArrow({});
	visualizePointsSubmesh = builder.endSubmesh();
	visualizePointsMesh = builder.createDXMesh();


	D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[2] = {};
	argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
	argumentDescs[0].Constant.Num32BitValuesToSet = 1;
	argumentDescs[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
	visualizePointsCommandSignature = createCommandSignature(*visualizePointsPipeline.rootSignature, argumentDescs, arraysize(argumentDescs), sizeof(placement_draw));
}



struct render_proc_placement_layer_data
{
	ref<dx_buffer> transforms;
	ref<dx_buffer> commandBuffer;
	uint32 commandBufferOffset;

	dx_vertex_buffer_group_view vertexBuffer;
	dx_index_buffer_view indexBuffer;
};

struct render_proc_placement_layer_pipeline
{
	using render_data_t = render_proc_placement_layer_data;

	PIPELINE_SETUP_DECL;
	PIPELINE_RENDER_DECL;
};

PIPELINE_SETUP_IMPL(render_proc_placement_layer_pipeline)
{
	cl->setPipelineState(*visualizePointsPipeline.pipeline);
	cl->setGraphicsRootSignature(*visualizePointsPipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cl->setGraphicsDynamicConstantBuffer(1, common.cameraCBV);
}

PIPELINE_RENDER_IMPL(render_proc_placement_layer_pipeline)
{
	cl->setVertexBuffer(0, rc.data.vertexBuffer.positions);
	cl->setIndexBuffer(rc.data.indexBuffer);

	cl->setRootGraphicsSRV(2, rc.data.transforms);
	cl->drawIndirect(visualizePointsCommandSignature, 1, rc.data.commandBuffer, rc.data.commandBufferOffset * sizeof(placement_draw));
}



#define READBACK 0

#if READBACK
static ref<dx_buffer> readbackIndirect;
static ref<dx_buffer> readbackMeshOffsets;
#endif

proc_placement_component::proc_placement_component(uint32 chunksPerDim, const std::vector<proc_placement_layer_desc>& layers)
{
	std::vector<placement_draw> drawArgs;

	{
		placement_draw draw;
		draw.draw.BaseVertexLocation = visualizePointsSubmesh.baseVertex;
		draw.draw.IndexCountPerInstance = visualizePointsSubmesh.numIndices;
		draw.draw.StartIndexLocation = visualizePointsSubmesh.firstIndex;
		draw.draw.InstanceCount = 0;
		draw.draw.StartInstanceLocation = 0;
		drawArgs.push_back(draw);

		submeshToMesh.push_back(0);
	}


	uint32 globalMeshOffset = 1;

	for (const auto& layerDesc : layers)
	{
		placement_layer layer;
		layer.footprint = layerDesc.footprint;
		layer.globalMeshOffset = globalMeshOffset;
		layer.numMeshes = 0;

		for (uint32 i = 0; i < 4; ++i)
		{
			const ref<multi_mesh>& mesh = layerDesc.meshes[i];
			layer.meshes[i] = mesh;

			if (mesh)
			{
				++layer.numMeshes;

				for (const auto& sub : mesh->submeshes)
				{
					placement_draw draw;
					draw.draw.BaseVertexLocation = sub.info.baseVertex;
					draw.draw.IndexCountPerInstance = sub.info.numIndices;
					draw.draw.StartIndexLocation = sub.info.firstIndex;
					draw.draw.InstanceCount = 0;
					draw.draw.StartInstanceLocation = 0;
					drawArgs.push_back(draw);

					submeshToMesh.push_back(globalMeshOffset + i);
				}
			}
		}

		globalMeshOffset += layer.numMeshes;

		this->layers.push_back(layer);
	}

	assert(globalMeshOffset <= 512); // Prefix sum currently only supports up to 512 points.

	placementPointBuffer = createBuffer(sizeof(placement_point), 100000, 0, true);
	transformBuffer = createBuffer(sizeof(placement_transform), 100000, 0, true);
	meshCountBuffer = createBuffer(sizeof(uint32), globalMeshOffset, 0, true, true);
	meshOffsetBuffer = createBuffer(sizeof(uint32), globalMeshOffset, 0, true, true);
	submeshToMeshBuffer = createBuffer(sizeof(uint32), (uint32)submeshToMesh.size(), submeshToMesh.data());
	drawIndirectBuffer = createBuffer(sizeof(placement_draw), (uint32)drawArgs.size(), drawArgs.data(), true);


#if READBACK
	readbackIndirect = createReadbackBuffer(sizeof(placement_draw), (uint32)drawArgs.size());
	readbackMeshOffsets = createReadbackBuffer(sizeof(uint32), globalMeshOffset);
#endif
}

void proc_placement_component::generate(const render_camera& camera, const terrain_component& terrain, vec3 positionOffset)
{
	dx_command_list* cl = dxContext.getFreeComputeCommandList(false);

	{
		PROFILE_ALL(cl, "Procedural placement");

		{
			PROFILE_ALL(cl, "Generate points");

			camera_frustum_planes frustum = camera.getWorldSpaceFrustumPlanes();

			const float radiusInUVSpace = sqrt(1.f / (2.f * sqrt(3.f) * arraysize(POISSON_SAMPLES)));
			float diameterInUVSpace = radiusInUVSpace * 2.f;
			float diameterInWorldSpace = diameterInUVSpace * terrain.chunkSize;

			cl->setPipelineState(*generatePointsPipeline.pipeline);
			cl->setComputeRootSignature(*generatePointsPipeline.rootSignature);

			vec3 minCorner = terrain.getMinCorner(positionOffset);
			vec3 chunkSize(terrain.chunkSize, terrain.amplitudeScale, terrain.chunkSize);

			cl->clearUAV(meshCountBuffer, 0u);

			for (uint32 z = 0; z < terrain.chunksPerDim; ++z)
			{
				for (uint32 x = 0; x < terrain.chunksPerDim; ++x)
				{
					auto& chunk = terrain.chunk(x, z);
					vec3 chunkMinCorner = minCorner + vec3(x * terrain.chunkSize, 0.f, z * terrain.chunkSize);
					vec3 chunkMaxCorner = chunkMinCorner + chunkSize;

					bounding_box aabb = { chunkMinCorner, chunkMaxCorner };
					if (!frustum.cullWorldSpaceAABB(aabb))
					{
						cl->setDescriptorHeapSRV(PROC_PLACEMENT_GENERATE_POINTS_RS_RESOURCES, 0, chunk.heightmap);
						cl->setDescriptorHeapSRV(PROC_PLACEMENT_GENERATE_POINTS_RS_RESOURCES, 1, chunk.normalmap);
						cl->setDescriptorHeapUAV(PROC_PLACEMENT_GENERATE_POINTS_RS_RESOURCES, 2, placementPointBuffer);
						cl->setDescriptorHeapUAV(PROC_PLACEMENT_GENERATE_POINTS_RS_RESOURCES, 3, meshCountBuffer);


						proc_placement_generate_points_cb cb;
						cb.amplitudeScale = terrain.amplitudeScale;
						cb.chunkCorner = chunkMinCorner;
						cb.chunkSize = terrain.chunkSize;

						for (const auto& layer : layers)
						{
							float footprint = layer.footprint;

							float scaling = diameterInWorldSpace / footprint;
							uint32 numGroupsPerDim = (uint32)ceil(scaling);

							cb.uvScale = 1.f / scaling;
							cb.uvStride = 1.f / scaling;
							cb.globalMeshOffset = layer.globalMeshOffset;

							cl->setCompute32BitConstants(PROC_PLACEMENT_GENERATE_POINTS_RS_CB, cb);

							cl->dispatch(numGroupsPerDim, numGroupsPerDim, 1);

							barrier_batcher(cl)
								.uav(placementPointBuffer)
								.uav(meshCountBuffer);
						}
					}
				}
			}
		}

		barrier_batcher(cl)
			.transition(meshCountBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ)
			.transition(placementPointBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);

		{
			PROFILE_ALL(cl, "Prefix sum");
		
			cl->setPipelineState(*prefixSumPipeline.pipeline);
			cl->setComputeRootSignature(*prefixSumPipeline.rootSignature);
		
			cl->setCompute32BitConstants(PROC_PLACEMENT_PREFIX_SUM_RS_CB, prefix_sum_cb{ meshCountBuffer->elementCount - 1 });
		
			// Offset by 1 element, since that is the point count.
			cl->setRootComputeUAV(PROC_PLACEMENT_PREFIX_SUM_RS_OUTPUT, meshOffsetBuffer->gpuVirtualAddress + meshOffsetBuffer->elementSize);
			cl->setRootComputeSRV(PROC_PLACEMENT_PREFIX_SUM_RS_INPUT, meshCountBuffer->gpuVirtualAddress + meshCountBuffer->elementSize);
		
			cl->dispatch(1);
		}

		barrier_batcher(cl)
			.transition(meshOffsetBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
		
		{
			PROFILE_ALL(cl, "Create draw calls");
		
			cl->setPipelineState(*createDrawCallsPipeline.pipeline);
			cl->setComputeRootSignature(*createDrawCallsPipeline.rootSignature);
		
			cl->setRootComputeUAV(PROC_PLACEMENT_CREATE_DRAW_CALLS_RS_OUTPUT, drawIndirectBuffer);
			cl->setRootComputeSRV(PROC_PLACEMENT_CREATE_DRAW_CALLS_RS_MESH_COUNTS, meshCountBuffer);
			cl->setRootComputeSRV(PROC_PLACEMENT_CREATE_DRAW_CALLS_RS_MESH_OFFSETS, meshOffsetBuffer);
			cl->setRootComputeSRV(PROC_PLACEMENT_CREATE_DRAW_CALLS_RS_SUBMESH_TO_MESH, submeshToMeshBuffer);
		
			cl->dispatch(bucketize(drawIndirectBuffer->elementCount, PROC_PLACEMENT_CREATE_DRAW_CALLS_BLOCK_SIZE));
		}

		barrier_batcher(cl)
			.transition(meshCountBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.uav(drawIndirectBuffer);

		{
			PROFILE_ALL(cl, "Create transforms");
		
			cl->setPipelineState(*createTransformsPipeline.pipeline);
			cl->setComputeRootSignature(*createTransformsPipeline.rootSignature);
		
			cl->setRootComputeUAV(PROC_PLACEMENT_CREATE_TRANSFORMS_RS_TRANSFORMS, transformBuffer);
			cl->setRootComputeUAV(PROC_PLACEMENT_CREATE_TRANSFORMS_RS_MESH_COUNTS, meshCountBuffer);
			cl->setRootComputeSRV(PROC_PLACEMENT_CREATE_TRANSFORMS_RS_POINTS, placementPointBuffer);
			cl->setRootComputeSRV(PROC_PLACEMENT_CREATE_TRANSFORMS_RS_MESH_OFFSETS, meshOffsetBuffer);
		
			cl->dispatch(bucketize(placementPointBuffer->elementCount, PROC_PLACEMENT_CREATE_TRANSFORMS_BLOCK_SIZE)); // TODO
			cl->uavBarrier(transformBuffer);
			cl->uavBarrier(meshCountBuffer);
		}
	}

#if READBACK
	cl->transitionBarrier(drawIndirectBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
	cl->copyBufferRegionToBuffer(drawIndirectBuffer, readbackIndirect, 0, drawIndirectBuffer->elementCount, 0);
	cl->copyBufferRegionToBuffer(meshOffsetBuffer, readbackMeshOffsets, 0, meshOffsetBuffer->elementCount, 0);
#endif

	dxContext.executeCommandList(cl);


#if READBACK
	dxContext.flushApplication();

	{	
		placement_draw* draw = (placement_draw*)mapBuffer(readbackIndirect, true);
		uint32* offsets = (uint32*)mapBuffer(readbackMeshOffsets, true);

		LOG_MESSAGE("%u (%u/%u), %u (%u/%u), %u (%u/%u)", 
			draw[0].draw.InstanceCount, draw[0].offset, offsets[0], 
			draw[1].draw.InstanceCount, draw[1].offset, offsets[1],
			draw[2].draw.InstanceCount, draw[2].offset, offsets[2]);

		unmapBuffer(readbackMeshOffsets, false);
		unmapBuffer(readbackIndirect, false);
	}
	{
	}
#endif
}

void proc_placement_component::render(ldr_render_pass* renderPass)
{
#if 0
	render_proc_placement_layer_data data = { transformBuffer, drawIndirectBuffer, 0, visualizePointsMesh.vertexBuffer, visualizePointsMesh.indexBuffer };
	renderPass->renderObject<render_proc_placement_layer_pipeline>(data);
#else
	
	uint32 drawCallOffset = 1;

	for (const auto& layer : layers)
	{
		for (uint32 i = 0; i < layer.numMeshes; ++i)
		{
			for (uint32 j = 0; j < (uint32)layer.meshes[i]->submeshes.size(); ++j)
			{
				render_proc_placement_layer_data data = { transformBuffer, drawIndirectBuffer, drawCallOffset,
					layer.meshes[i]->mesh.vertexBuffer, layer.meshes[i]->mesh.indexBuffer };

				renderPass->renderObject<render_proc_placement_layer_pipeline>(data);

				++drawCallOffset;
			}
		}
	}

#endif
}
