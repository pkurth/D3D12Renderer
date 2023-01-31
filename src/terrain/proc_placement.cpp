#include "pch.h"
#include "proc_placement.h"

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
static dx_pipeline visualizePointsPipeline;

static dx_mesh sphereMesh;
static submesh_info sphereSubmesh;
static dx_command_signature visualizePointsCommandSignature;


void initializeProceduralPlacementPipelines()
{
	generatePointsPipeline = createReloadablePipeline("proc_placement_generate_points_cs");

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position)
			.renderTargets(ldrFormat, depthStencilFormat);

		visualizePointsPipeline = createReloadablePipeline(desc, { "proc_placement_points_vs", "proc_placement_points_ps" });
	}


	mesh_builder builder(mesh_creation_flags_with_positions);
	builder.pushSphere({});
	sphereSubmesh = builder.endSubmesh();
	sphereMesh = builder.createDXMesh();


	D3D12_INDIRECT_ARGUMENT_DESC argumentDesc;
	argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
	visualizePointsCommandSignature = createCommandSignature({}, &argumentDesc, 1, sizeof(placement_draw));
}



struct visualize_points_render_data
{
	ref<dx_buffer> points;
	ref<dx_buffer> commandBuffer;
};

struct visualize_points_pipeline
{
	using render_data_t = visualize_points_render_data;

	PIPELINE_SETUP_DECL;
	PIPELINE_RENDER_DECL;
};

PIPELINE_SETUP_IMPL(visualize_points_pipeline)
{
	cl->setPipelineState(*visualizePointsPipeline.pipeline);
	cl->setGraphicsRootSignature(*visualizePointsPipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cl->setGraphicsDynamicConstantBuffer(1, common.cameraCBV);
}

PIPELINE_RENDER_IMPL(visualize_points_pipeline)
{
	cl->setVertexBuffer(0, sphereMesh.vertexBuffer.positions);
	cl->setIndexBuffer(sphereMesh.indexBuffer);

	cl->setRootGraphicsSRV(0, rc.data.points);
	cl->drawIndirect(visualizePointsCommandSignature, 1, rc.data.commandBuffer);
}











proc_placement_component::proc_placement_component(uint32 chunksPerDim, const std::vector<proc_placement_layer_desc>& layers)
{
	std::vector<placement_draw> drawArgs;

	{
		placement_draw draw;
		draw.draw.BaseVertexLocation = sphereSubmesh.baseVertex;
		draw.draw.IndexCountPerInstance = sphereSubmesh.numIndices;
		draw.draw.StartIndexLocation = sphereSubmesh.firstIndex;
		draw.draw.InstanceCount = 0;
		draw.draw.StartInstanceLocation = 0;
		drawArgs.push_back(draw);
	}


	for (const auto& layerDesc : layers)
	{
		placement_layer layer;
		layer.footprint = layerDesc.footprint;
		layer.globalSubmeshOffset = (uint32)drawArgs.size();

		for (uint32 i = 0; i < 4; ++i)
		{
			const ref<multi_mesh>& mesh = layerDesc.meshes[i];
			layer.meshes[i] = mesh;

			if (mesh)
			{
				for (const auto& sub : mesh->submeshes)
				{
					placement_draw draw;
					draw.draw.BaseVertexLocation = sub.info.baseVertex;
					draw.draw.IndexCountPerInstance = sub.info.numIndices;
					draw.draw.StartIndexLocation = sub.info.firstIndex;
					draw.draw.InstanceCount = 0;
					draw.draw.StartInstanceLocation = 0;
					drawArgs.push_back(draw);
				}
			}
		}

		this->layers.push_back(layer);
	}

	placementPointBuffer = createBuffer(sizeof(placement_point), 1000000, 0, true);
	placementPointCountBuffer = createBuffer(sizeof(uint32), 1, 0, true, true);
	submeshCountBuffer = createBuffer(sizeof(uint32), (uint32)drawArgs.size(), 0, true, true);
	drawIndirectBuffer = createBuffer(sizeof(placement_draw), (uint32)drawArgs.size(), drawArgs.data(), true);
}

void proc_placement_component::generate(const render_camera& camera, const terrain_component& terrain, vec3 positionOffset)
{
	dx_command_list* cl = dxContext.getFreeComputeCommandList(false);

	{
		PROFILE_ALL(cl, "Procedural placement");

		camera_frustum_planes frustum = camera.getWorldSpaceFrustumPlanes();

		const float radiusInUVSpace = sqrt(1.f / (2.f * sqrt(3.f) * arraysize(POISSON_SAMPLES)));
		float diameterInUVSpace = radiusInUVSpace * 2.f;
		float diameterInWorldSpace = diameterInUVSpace * terrain.chunkSize;

		cl->setPipelineState(*generatePointsPipeline.pipeline);
		cl->setComputeRootSignature(*generatePointsPipeline.rootSignature);

		vec3 minCorner = terrain.getMinCorner(positionOffset);
		vec3 chunkSize(terrain.chunkSize, terrain.amplitudeScale, terrain.chunkSize);

		cl->clearUAV(placementPointCountBuffer, 0u);
		cl->clearUAV(submeshCountBuffer, 0u);

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
					cl->setDescriptorHeapUAV(PROC_PLACEMENT_GENERATE_POINTS_RS_RESOURCES, 3, placementPointCountBuffer);
					cl->setDescriptorHeapUAV(PROC_PLACEMENT_GENERATE_POINTS_RS_RESOURCES, 4, submeshCountBuffer);


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

						uint32 submeshOffset = layer.globalSubmeshOffset;
						for (uint32 i = 0; i < 4; ++i)
						{
							cb.firstSubmeshPerLayerObject[i] = submeshOffset;
							cb.numSubmeshesPerLayerObject[i] = layer.meshes[i] ? (uint32)layer.meshes[i]->submeshes.size() : 0;
							submeshOffset += cb.numSubmeshesPerLayerObject[i];
						}


						cl->setCompute32BitConstants(PROC_PLACEMENT_GENERATE_POINTS_RS_CB, cb);

						cl->dispatch(numGroupsPerDim, numGroupsPerDim, 1);

						barrier_batcher(cl)
							.uav(placementPointBuffer)
							.uav(placementPointCountBuffer);
					}
				}
			}
		}

		cl->transitionBarrier(placementPointCountBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
		cl->copyBufferRegionToBuffer_ByteOffset(placementPointCountBuffer, drawIndirectBuffer, 0, sizeof(uint32), offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, InstanceCount));
	}

	dxContext.executeCommandList(cl);
}

void proc_placement_component::render(ldr_render_pass* renderPass)
{
	visualize_points_render_data data = { placementPointBuffer, drawIndirectBuffer };
	renderPass->renderObject<visualize_points_pipeline>(data);
}
