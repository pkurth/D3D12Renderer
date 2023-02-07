#include "pch.h"
#include "grass.h"

#include "core/math.h"
#include "core/log.h"

#include "rendering/render_command.h"
#include "rendering/material.h"
#include "rendering/render_utils.h"

#include "dx/dx_command_list.h"
#include "dx/dx_profiling.h"
#include "dx/dx_barrier_batcher.h"

#include "grass_rs.hlsli"
#include "proc_placement_rs.hlsli"


static const uint32 numSegmentsLOD0 = 4;


static dx_pipeline grassPipeline;
static dx_pipeline grassGenerationPipeline;
static dx_pipeline grassCreateDrawCallsPipeline;

static dx_command_signature grassCommandSignature;

void initializeGrassPipelines()
{
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.cullingOff()
			//.wireframe()
			.renderTargets(ldrFormat, depthStencilFormat);

		grassPipeline = createReloadablePipeline(desc, { "grass_vs", "grass_ps" });
	}

	grassGenerationPipeline = createReloadablePipeline("grass_generation_cs");
	grassCreateDrawCallsPipeline = createReloadablePipeline("grass_create_draw_calls_cs");


	D3D12_INDIRECT_ARGUMENT_DESC argumentDesc;
	argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
	grassCommandSignature = createCommandSignature({}, &argumentDesc, 1, sizeof(grass_draw));
}

struct grass_render_data
{
	grass_settings settings;
	ref<dx_buffer> drawBuffer;
	ref<dx_buffer> bladeBufferLOD0;
	ref<dx_buffer> bladeBufferLOD1;
};

struct grass_pipeline
{
	using render_data_t = grass_render_data;

	PIPELINE_SETUP_DECL;
	PIPELINE_RENDER_DECL;
};

PIPELINE_SETUP_IMPL(grass_pipeline)
{
	cl->setPipelineState(*grassPipeline.pipeline);
	cl->setGraphicsRootSignature(*grassPipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	cl->setGraphicsDynamicConstantBuffer(GRASS_RS_CAMERA, common.cameraCBV);
}

PIPELINE_RENDER_IMPL(grass_pipeline)
{
	PROFILE_ALL(cl, "Grass");

	static float time = 0.f;
	time += 1.f / 150.f;

	vec3 windDirection = normalize(vec3(1.f, 0.f, 1.f));

	uint32 numVerticesLOD0 = numSegmentsLOD0 * 2 + 1;
	uint32 numVerticesLOD1 = numVerticesLOD0 / 2 + 1;

	{
		grass_cb cb;
		cb.numVertices = numVerticesLOD0;
		cb.halfWidth = rc.data.settings.bladeWidth * 0.5f;
		cb.height = rc.data.settings.bladeHeight;
		cb.time = time;
		cb.windDirection = windDirection;

		cl->setRootGraphicsSRV(GRASS_RS_BLADES, rc.data.bladeBufferLOD0);
		cl->setGraphics32BitConstants(GRASS_RS_CB, cb);

		cl->drawIndirect(grassCommandSignature, 1, rc.data.drawBuffer, 0 * sizeof(grass_draw));
	}

	{
		grass_cb cb;
		cb.numVertices = numVerticesLOD1;
		cb.halfWidth = rc.data.settings.bladeWidth * 0.5f;
		cb.height = rc.data.settings.bladeHeight;
		cb.time = time;
		cb.windDirection = windDirection;

		cl->setRootGraphicsSRV(GRASS_RS_BLADES, rc.data.bladeBufferLOD1);
		cl->setGraphics32BitConstants(GRASS_RS_CB, cb);

		cl->drawIndirect(grassCommandSignature, 1, rc.data.drawBuffer, 1 * sizeof(grass_draw));
	}
}




#define READBACK 0

#if READBACK
static ref<dx_buffer> readbackIndirect;
#endif


grass_component::grass_component()
{
	uint32 numVerticesLOD0 = numSegmentsLOD0 * 2 + 1;
	uint32 numVerticesLOD1 = numVerticesLOD0 / 2 + 1;

	grass_draw draw[2] = {};
	draw[0].draw.VertexCountPerInstance = numVerticesLOD0;
	draw[1].draw.VertexCountPerInstance = numVerticesLOD1;

	drawBuffer = createBuffer(sizeof(grass_draw), 2, &draw, true);
	countBuffer = createBuffer(sizeof(uint32), 2, 0, true, true);
	bladeBufferLOD0 = createBuffer(sizeof(grass_blade), 1000000, 0, true);
	bladeBufferLOD1 = createBuffer(sizeof(grass_blade), 1000000, 0, true);


#if READBACK
	readbackIndirect = createReadbackBuffer(sizeof(grass_draw), 2);
#endif
}

void grass_component::generate(const render_camera& camera, const terrain_component& terrain, vec3 positionOffset)
{
	dx_command_list* cl = dxContext.getFreeComputeCommandList(false);

	{
		PROFILE_ALL(cl, "Grass generation");

		{
			PROFILE_ALL(cl, "Generate blades");

			camera_frustum_planes frustum = camera.getWorldSpaceFrustumPlanes();

			const float radiusInUVSpace = sqrt(1.f / (2.f * sqrt(3.f) * arraysize(POISSON_SAMPLES)));
			float diameterInUVSpace = radiusInUVSpace * 2.f;
			float diameterInWorldSpace = diameterInUVSpace * terrain.chunkSize;

			cl->setPipelineState(*grassGenerationPipeline.pipeline);
			cl->setComputeRootSignature(*grassGenerationPipeline.rootSignature);

			vec3 minCorner = terrain.getMinCorner(positionOffset);
			vec3 chunkSize(terrain.chunkSize, terrain.amplitudeScale, terrain.chunkSize);

			cl->clearUAV(countBuffer, 0u);



			float footprint = settings.footprint;
			float scaling = diameterInWorldSpace / footprint;
			uint32 numGroupsPerDim = (uint32)ceil(scaling);

			grass_generation_common_cb common;
			memcpy(common.frustumPlanes, frustum.planes, sizeof(vec4) * 6);
			common.amplitudeScale = terrain.amplitudeScale;
			common.chunkSize = terrain.chunkSize;
			common.uvScale = 1.f / scaling;
			common.cameraPosition = camera.position;

			auto commonCBV = dxContext.uploadDynamicConstantBuffer(common);
			cl->setComputeDynamicConstantBuffer(GRASS_GENERATION_RS_COMMON, commonCBV);


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
						cl->setDescriptorHeapSRV(GRASS_GENERATION_RS_RESOURCES, 0, chunk.heightmap);
						cl->setDescriptorHeapSRV(GRASS_GENERATION_RS_RESOURCES, 1, chunk.normalmap);
						cl->setDescriptorHeapUAV(GRASS_GENERATION_RS_RESOURCES, 2, bladeBufferLOD0);
						cl->setDescriptorHeapUAV(GRASS_GENERATION_RS_RESOURCES, 3, bladeBufferLOD1);
						cl->setDescriptorHeapUAV(GRASS_GENERATION_RS_RESOURCES, 4, countBuffer);

						cl->setCompute32BitConstants(GRASS_GENERATION_RS_CB, grass_generation_cb{ chunkMinCorner });

						cl->dispatch(numGroupsPerDim, numGroupsPerDim, 1);

						//barrier_batcher(cl)
						//	.uav(bladeBufferLOD0)
						//	.uav(bladeBufferLOD1)
						//	.uav(countBuffer);
					}
				}
			}
		}

		cl->transitionBarrier(countBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);

		{
			PROFILE_ALL(cl, "Create draw calls");

			cl->setPipelineState(*grassCreateDrawCallsPipeline.pipeline);
			cl->setComputeRootSignature(*grassCreateDrawCallsPipeline.rootSignature);

			cl->setCompute32BitConstants(GRASS_CREATE_DRAW_CALLS_RS_CB, grass_create_draw_calls_cb{ bladeBufferLOD0->elementCount });
			cl->setRootComputeUAV(GRASS_CREATE_DRAW_CALLS_RS_OUTPUT, drawBuffer);
			cl->setRootComputeSRV(GRASS_CREATE_DRAW_CALLS_RS_MESH_COUNTS, countBuffer);

			cl->dispatch(bucketize(drawBuffer->elementCount, GRASS_CREATE_DRAW_CALLS_BLOCK_SIZE));
		}
	}


#if READBACK
	cl->transitionBarrier(drawBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
	cl->copyBufferRegionToBuffer(drawBuffer, readbackIndirect, 0, drawBuffer->elementCount, 0);
#endif

	dxContext.executeCommandList(cl);


#if READBACK
	dxContext.flushApplication();

	{
		grass_draw* draw = (grass_draw*)mapBuffer(readbackIndirect, true);
		LOG_MESSAGE("%u, %u grass blades", draw[0].draw.InstanceCount, draw[1].draw.InstanceCount);
		unmapBuffer(readbackIndirect, false);
	}
#endif
}

void grass_component::render(ldr_render_pass* renderPass)
{
	renderPass->renderObject<grass_pipeline>({ settings, drawBuffer, bladeBufferLOD0, bladeBufferLOD1 });
}
