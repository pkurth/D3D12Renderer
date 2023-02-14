#include "pch.h"
#include "grass.h"

#include "core/math.h"
#include "core/log.h"

#include "rendering/render_command.h"
#include "rendering/material.h"
#include "rendering/render_utils.h"
#include "rendering/render_resources.h"

#include "dx/dx_command_list.h"
#include "dx/dx_profiling.h"
#include "dx/dx_barrier_batcher.h"

#include "grass_rs.hlsli"
#include "depth_only_rs.hlsli"


static const uint32 numSegmentsLOD0 = 4;


static dx_pipeline grassPipeline;
static dx_pipeline grassDepthOnlyPipeline;

static dx_pipeline grassNoDepthPrepassPipeline;

static dx_pipeline grassGenerationPipeline;
static dx_pipeline grassCreateDrawCallsPipeline;

static dx_command_signature grassCommandSignature;

void initializeGrassPipelines()
{
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.depthSettings(true, false, D3D12_COMPARISON_FUNC_EQUAL)
			.cullingOff()
			.renderTargets(opaqueLightPassFormats, OPQAUE_LIGHT_PASS_NO_VELOCITIES_NO_OBJECT_ID, depthStencilFormat);

		grassPipeline = createReloadablePipeline(desc, { "grass_vs", "grass_ps" });
	}
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.cullingOff()
			.renderTargets(depthOnlyFormat, arraysize(depthOnlyFormat), depthStencilFormat);

		grassDepthOnlyPipeline = createReloadablePipeline(desc, { "grass_depth_only_vs", "depth_only_ps" }, rs_in_vertex_shader);
	}
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.cullingOff()
			.renderTargets(opaqueLightPassFormats, OPQAUE_LIGHT_PASS_FULL, depthStencilFormat);

		grassNoDepthPrepassPipeline = createReloadablePipeline(desc, { "grass_no_depth_prepass_vs", "grass_no_depth_prepass_ps" });
	}

	grassGenerationPipeline = createReloadablePipeline("grass_generation_cs");
	grassCreateDrawCallsPipeline = createReloadablePipeline("grass_create_draw_calls_cs");


	D3D12_INDIRECT_ARGUMENT_DESC argumentDesc;
	argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
	grassCommandSignature = createCommandSignature({}, &argumentDesc, 1, sizeof(grass_draw));
}

static grass_cb createGrassCB(const grass_settings& settings, vec2 windDirection, uint32 numVertices)
{
	grass_cb cb;
	cb.numVertices = numVertices;
	cb.halfWidth = settings.bladeWidth * 0.5f;
	cb.windDirection = windDirection;
	return cb;
}

static grass_cb createGrassCB_LOD0(const grass_settings& settings, vec2 windDirection)
{
	const uint32 numVerticesLOD0 = numSegmentsLOD0 * 2 + 1;
	return createGrassCB(settings, windDirection, numVerticesLOD0);
}

static grass_cb createGrassCB_LOD1(const grass_settings& settings, vec2 windDirection)
{
	const uint32 numVerticesLOD0 = numSegmentsLOD0 * 2 + 1;
	const uint32 numVerticesLOD1 = numVerticesLOD0 / 2 + 1;
	return createGrassCB(settings, windDirection, numVerticesLOD1);
}

struct grass_render_data
{
	grass_settings settings;
	ref<dx_buffer> drawBuffer;
	ref<dx_buffer> bladeBufferLOD0;
	ref<dx_buffer> bladeBufferLOD1;

	vec2 windDirection;

	uint32 objectID;
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
	cl->setGraphicsDynamicConstantBuffer(GRASS_RS_LIGHTING, common.lightingCBV);


	dx_cpu_descriptor_handle nullTexture = render_resources::nullTextureSRV;

	cl->setDescriptorHeapSRV(GRASS_RS_FRAME_CONSTANTS, 0, common.irradiance);
	cl->setDescriptorHeapSRV(GRASS_RS_FRAME_CONSTANTS, 1, common.prefilteredRadiance);
	cl->setDescriptorHeapSRV(GRASS_RS_FRAME_CONSTANTS, 2, render_resources::brdfTex);
	cl->setDescriptorHeapSRV(GRASS_RS_FRAME_CONSTANTS, 3, common.shadowMap);
	cl->setDescriptorHeapSRV(GRASS_RS_FRAME_CONSTANTS, 4, common.aoTexture ? common.aoTexture : render_resources::whiteTexture);
	cl->setDescriptorHeapSRV(GRASS_RS_FRAME_CONSTANTS, 5, common.sssTexture ? common.sssTexture : render_resources::whiteTexture);
	cl->setDescriptorHeapSRV(GRASS_RS_FRAME_CONSTANTS, 6, common.ssrTexture ? common.ssrTexture->defaultSRV : nullTexture);
}

PIPELINE_RENDER_IMPL(grass_pipeline)
{
	PROFILE_ALL(cl, "Grass");

	{
		grass_cb cb = createGrassCB_LOD0(rc.data.settings, rc.data.windDirection);
		
		cl->setRootGraphicsSRV(GRASS_RS_BLADES, rc.data.bladeBufferLOD0);
		cl->setGraphics32BitConstants(GRASS_RS_CB, cb);

		cl->drawIndirect(grassCommandSignature, 1, rc.data.drawBuffer, 0 * sizeof(grass_draw));
	}

	{
		grass_cb cb = createGrassCB_LOD1(rc.data.settings, rc.data.windDirection);

		cl->setRootGraphicsSRV(GRASS_RS_BLADES, rc.data.bladeBufferLOD1);
		cl->setGraphics32BitConstants(GRASS_RS_CB, cb);

		cl->drawIndirect(grassCommandSignature, 1, rc.data.drawBuffer, 1 * sizeof(grass_draw));
	}
}



struct grass_depth_prepass_pipeline
{
	using render_data_t = grass_render_data;

	PIPELINE_SETUP_DECL;
	DEPTH_ONLY_RENDER_DECL;
};

PIPELINE_SETUP_IMPL(grass_depth_prepass_pipeline)
{
	cl->setPipelineState(*grassDepthOnlyPipeline.pipeline);
	cl->setGraphicsRootSignature(*grassDepthOnlyPipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	depth_only_camera_jitter_cb jitterCB = { common.cameraJitter, common.prevFrameCameraJitter };
	cl->setGraphics32BitConstants(GRASS_DEPTH_ONLY_RS_CAMERA_JITTER, jitterCB);
	cl->setGraphicsDynamicConstantBuffer(GRASS_DEPTH_ONLY_RS_CAMERA, common.cameraCBV);
}

DEPTH_ONLY_RENDER_IMPL(grass_depth_prepass_pipeline)
{
	PROFILE_ALL(cl, "Grass depth prepass");

	cl->setGraphics32BitConstants(GRASS_DEPTH_ONLY_RS_OBJECT_ID, rc.objectID);

	{
		grass_cb cb = createGrassCB_LOD0(rc.data.settings, rc.data.windDirection);

		cl->setRootGraphicsSRV(GRASS_DEPTH_ONLY_RS_BLADES, rc.data.bladeBufferLOD0);
		cl->setGraphics32BitConstants(GRASS_DEPTH_ONLY_RS_CB, cb);

		cl->drawIndirect(grassCommandSignature, 1, rc.data.drawBuffer, 0 * sizeof(grass_draw));
	}

	{
		grass_cb cb = createGrassCB_LOD1(rc.data.settings, rc.data.windDirection);

		cl->setRootGraphicsSRV(GRASS_DEPTH_ONLY_RS_BLADES, rc.data.bladeBufferLOD1);
		cl->setGraphics32BitConstants(GRASS_DEPTH_ONLY_RS_CB, cb);

		cl->drawIndirect(grassCommandSignature, 1, rc.data.drawBuffer, 1 * sizeof(grass_draw));
	}
}






struct grass_no_depth_prepass_pipeline
{
	using render_data_t = grass_render_data;

	PIPELINE_SETUP_DECL;
	PIPELINE_RENDER_DECL;
};

PIPELINE_SETUP_IMPL(grass_no_depth_prepass_pipeline)
{
	cl->setPipelineState(*grassNoDepthPrepassPipeline.pipeline);
	cl->setGraphicsRootSignature(*grassNoDepthPrepassPipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	cl->setGraphicsDynamicConstantBuffer(GRASS_RS_CAMERA, common.cameraCBV);
	cl->setGraphicsDynamicConstantBuffer(GRASS_RS_LIGHTING, common.lightingCBV);


	dx_cpu_descriptor_handle nullTexture = render_resources::nullTextureSRV;

	cl->setDescriptorHeapSRV(GRASS_RS_FRAME_CONSTANTS, 0, common.irradiance);
	cl->setDescriptorHeapSRV(GRASS_RS_FRAME_CONSTANTS, 1, common.prefilteredRadiance);
	cl->setDescriptorHeapSRV(GRASS_RS_FRAME_CONSTANTS, 2, render_resources::brdfTex);
	cl->setDescriptorHeapSRV(GRASS_RS_FRAME_CONSTANTS, 3, common.shadowMap);
	cl->setDescriptorHeapSRV(GRASS_RS_FRAME_CONSTANTS, 4, common.aoTexture ? common.aoTexture : render_resources::whiteTexture);
	cl->setDescriptorHeapSRV(GRASS_RS_FRAME_CONSTANTS, 5, common.sssTexture ? common.sssTexture : render_resources::whiteTexture);
	cl->setDescriptorHeapSRV(GRASS_RS_FRAME_CONSTANTS, 6, common.ssrTexture ? common.ssrTexture->defaultSRV : nullTexture);
}

PIPELINE_RENDER_IMPL(grass_no_depth_prepass_pipeline)
{
	PROFILE_ALL(cl, "Grass");

	cl->setGraphics32BitConstants(GRASS_RS_OBJECT_ID, rc.data.objectID);

	{
		grass_cb cb = createGrassCB_LOD0(rc.data.settings, rc.data.windDirection);

		cl->setRootGraphicsSRV(GRASS_RS_BLADES, rc.data.bladeBufferLOD0);
		cl->setGraphics32BitConstants(GRASS_RS_CB, cb);

		cl->drawIndirect(grassCommandSignature, 1, rc.data.drawBuffer, 0 * sizeof(grass_draw));
	}

	{
		grass_cb cb = createGrassCB_LOD1(rc.data.settings, rc.data.windDirection);

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

void grass_component::generate(const render_camera& camera, const terrain_component& terrain, vec3 positionOffset, float dt)
{
	prevTime = time;
	time += dt;

	dx_command_list* cl = dxContext.getFreeComputeCommandList(false);

	{
		PROFILE_ALL(cl, "Grass generation");

		{
			PROFILE_ALL(cl, "Generate blades");

			camera_frustum_planes frustum = camera.getWorldSpaceFrustumPlanes();

			
			cl->setPipelineState(*grassGenerationPipeline.pipeline);
			cl->setComputeRootSignature(*grassGenerationPipeline.rootSignature);

			vec3 minCorner = terrain.getMinCorner(positionOffset);
			vec3 chunkSize(terrain.chunkSize, terrain.amplitudeScale, terrain.chunkSize);

			cl->clearUAV(countBuffer, 0u);


			uint32 numGrassBladesPerDim = settings.numGrassBladesPerChunkDim & (~1); // Make sure this is an even number.
			numGrassBladesPerDim = min(numGrassBladesPerDim, 1024u);
			const float lodChangeEndDistance = settings.lodChangeStartDistance + settings.lodChangeTransitionDistance;


			grass_generation_common_cb common;
			memcpy(common.frustumPlanes, frustum.planes, sizeof(vec4) * 6);
			common.amplitudeScale = terrain.amplitudeScale;
			common.chunkSize = terrain.chunkSize;
			common.cameraPosition = camera.position;
			common.lodChangeStartDistance = settings.lodChangeStartDistance;
			common.lodChangeEndDistance = lodChangeEndDistance;
			common.uvScale = 1.f / numGrassBladesPerDim;
			common.baseHeight = settings.bladeHeight;
			common.time = time;
			common.prevFrameTime = prevTime;

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
						uint32 chunkNumGrassBladesPerDim = numGrassBladesPerDim;
						uint32 lodIndex = 0;

						float sqDistance = pointInBox(camera.position, aabb.minCorner, aabb.maxCorner) 
							? 0.f 
							: squaredLength(camera.position - closestPoint_PointAABB(camera.position, aabb));
						if (sqDistance > lodChangeEndDistance * lodChangeEndDistance)
						{
							chunkNumGrassBladesPerDim /= 2;
							lodIndex = 1;
						}

						cl->setDescriptorHeapSRV(GRASS_GENERATION_RS_RESOURCES, 0, chunk.heightmap);
						cl->setDescriptorHeapSRV(GRASS_GENERATION_RS_RESOURCES, 1, chunk.normalmap);
						cl->setDescriptorHeapUAV(GRASS_GENERATION_RS_RESOURCES, 2, bladeBufferLOD0);
						cl->setDescriptorHeapUAV(GRASS_GENERATION_RS_RESOURCES, 3, bladeBufferLOD1);
						cl->setDescriptorHeapUAV(GRASS_GENERATION_RS_RESOURCES, 4, countBuffer);

						cl->setCompute32BitConstants(GRASS_GENERATION_RS_CB, grass_generation_cb{ chunkMinCorner, lodIndex });

						cl->dispatch(bucketize(chunkNumGrassBladesPerDim, GRASS_GENERATION_BLOCK_SIZE), bucketize(chunkNumGrassBladesPerDim, GRASS_GENERATION_BLOCK_SIZE), 1);

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

void grass_component::render(opaque_render_pass* renderPass, uint32 entityID)
{
	grass_render_data data = { settings, drawBuffer, bladeBufferLOD0, bladeBufferLOD1, windDirection, entityID };
	if (grass_settings::depthPrepass)
	{
		renderPass->renderObject<grass_pipeline, grass_depth_prepass_pipeline>(data, data, entityID);
	}
	else
	{
		renderPass->renderObject<grass_no_depth_prepass_pipeline>(data);
	}
}
