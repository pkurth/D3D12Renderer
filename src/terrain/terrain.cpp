#include "pch.h"
#include "terrain.h"

#include "heightmap_collider.h"

#include "dx/dx_context.h"
#include "dx/dx_command_list.h"
#include "dx/dx_barrier_batcher.h"
#include "dx/dx_profiling.h"

#include "rendering/render_utils.h"
#include "rendering/render_pass.h"
#include "rendering/render_resources.h"
#include "rendering/texture_preprocessing.h"
#include "rendering/render_algorithms.h"

#include "core/random.h"
#include "core/job_system.h"
#include "scene/components.h"

#include "terrain_rs.hlsli"
#include "depth_only_rs.hlsli"


static dx_pipeline terrainGenerationPipeline;
static dx_pipeline terrainPipeline;
static dx_pipeline terrainDepthOnlyPipeline;
static dx_pipeline terrainShadowPipeline;
static dx_pipeline terrainOutlinePipeline;
static ref<dx_index_buffer> terrainIndexBuffers[TERRAIN_MAX_LOD + 1];




struct terrain_render_data_common
{
	vec3 minCorner;
	int32 lod;
	float chunkSize;
	float amplitudeScale;

	int32 lod_negX;
	int32 lod_posX;
	int32 lod_negZ;
	int32 lod_posZ;

	ref<dx_texture> heightmap;

	uint32 objectID;
};

struct terrain_render_data
{
	terrain_render_data_common common;

	ref<dx_texture> normalmap;

	ref<pbr_material> groundMaterial;
	ref<pbr_material> rockMaterial;
	ref<pbr_material> mudMaterial;

	dx_dynamic_constant_buffer waterPlanesCBV;
};

struct terrain_pipeline
{
	PIPELINE_SETUP_DECL;
	PIPELINE_RENDER_DECL(terrain_render_data);
};

struct terrain_depth_prepass_pipeline
{
	PIPELINE_SETUP_DECL;
	DEPTH_ONLY_RENDER_DECL(terrain_render_data_common);
};

struct terrain_shadow_pipeline
{
	PIPELINE_SETUP_DECL;
	PIPELINE_RENDER_DECL(terrain_render_data_common);
};

struct terrain_outline_pipeline
{
	PIPELINE_SETUP_DECL;
	PIPELINE_RENDER_DECL(terrain_render_data_common);
};





void initializeTerrainPipelines()
{
	terrainGenerationPipeline = createReloadablePipeline("terrain_generation_cs");

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			//.wireframe()
			.renderTargets(opaqueLightPassFormats, OPQAUE_LIGHT_PASS_NO_VELOCITIES_NO_OBJECT_ID, depthStencilFormat)
			.depthSettings(true, false, D3D12_COMPARISON_FUNC_EQUAL);

		terrainPipeline = createReloadablePipeline(desc, { "terrain_vs", "terrain_ps" });
	}
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(depthOnlyFormat, arraysize(depthOnlyFormat), depthStencilFormat);

		terrainDepthOnlyPipeline = createReloadablePipeline(desc, { "terrain_depth_only_vs", "depth_only_ps" }, rs_in_vertex_shader);
	}
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(0, 0, shadowDepthFormat);

		terrainShadowPipeline = createReloadablePipeline(desc, { "terrain_shadow_vs" }, rs_in_vertex_shader);
		//pointLightShadowPipeline = createReloadablePipeline(desc, { "shadow_point_light_vs", "shadow_point_light_ps" }, rs_in_vertex_shader);
	}
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(0, 0, depthStencilFormat)
			.stencilSettings(D3D12_COMPARISON_FUNC_ALWAYS,
				D3D12_STENCIL_OP_REPLACE,
				D3D12_STENCIL_OP_REPLACE,
				D3D12_STENCIL_OP_KEEP,
				D3D12_DEFAULT_STENCIL_READ_MASK,
				stencil_flag_selected_object) // Mark selected object.
			.depthSettings(false, false);

		terrainOutlinePipeline = createReloadablePipeline(desc, { "terrain_outline_vs" }, rs_in_vertex_shader);
	}



	uint32 numSegmentsPerDim = TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1;
	uint32 numTrisLod0 = numSegmentsPerDim * numSegmentsPerDim * 2;

	indexed_triangle16* tris = new indexed_triangle16[numTrisLod0];

	for (uint32 lod = 0; lod < TERRAIN_MAX_LOD + 1; ++lod)
	{
		uint32 numSegmentsPerDim = (TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1) >> lod;
		uint32 stride = numSegmentsPerDim + 1;
		uint32 numTris = numSegmentsPerDim * numSegmentsPerDim * 2;

		uint32 i = 0;
		for (uint32 z = 0; z < numSegmentsPerDim; ++z)
		{
			for (uint32 x = 0; x < numSegmentsPerDim; ++x)
			{
				tris[i++] = { (uint16)(stride * z + x),		(uint16)(stride * (z + 1) + x), (uint16)(stride * z + x + 1) };
				tris[i++] = { (uint16)(stride * z + x + 1), (uint16)(stride * (z + 1) + x), (uint16)(stride * (z + 1) + x + 1) };
			}
		}

		terrainIndexBuffers[lod] = createIndexBuffer(sizeof(uint16), numTris * 3, tris);
	}

	delete[] tris;
}

PIPELINE_SETUP_IMPL(terrain_pipeline)
{
	cl->setPipelineState(*terrainPipeline.pipeline);
	cl->setGraphicsRootSignature(*terrainPipeline.rootSignature);

	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cl->setGraphicsDynamicConstantBuffer(TERRAIN_RS_CAMERA, common.cameraCBV);
	cl->setGraphicsDynamicConstantBuffer(TERRAIN_RS_LIGHTING, common.lightingCBV);

	dx_cpu_descriptor_handle nullTexture = render_resources::nullTextureSRV;

	cl->setDescriptorHeapSRV(TERRAIN_RS_FRAME_CONSTANTS, 0, common.irradiance);
	cl->setDescriptorHeapSRV(TERRAIN_RS_FRAME_CONSTANTS, 1, common.prefilteredRadiance);
	cl->setDescriptorHeapSRV(TERRAIN_RS_FRAME_CONSTANTS, 2, render_resources::brdfTex);
	cl->setDescriptorHeapSRV(TERRAIN_RS_FRAME_CONSTANTS, 3, common.shadowMap);
	cl->setDescriptorHeapSRV(TERRAIN_RS_FRAME_CONSTANTS, 4, common.aoTexture ? common.aoTexture : render_resources::whiteTexture);
	cl->setDescriptorHeapSRV(TERRAIN_RS_FRAME_CONSTANTS, 5, common.sssTexture ? common.sssTexture : render_resources::whiteTexture);
	cl->setDescriptorHeapSRV(TERRAIN_RS_FRAME_CONSTANTS, 6, common.ssrTexture ? common.ssrTexture->defaultSRV : nullTexture);
}

static terrain_cb getTerrainCB(const terrain_render_data_common& common)
{
	uint32 lod_negX = max(0, common.lod_negX - common.lod);
	uint32 lod_posX = max(0, common.lod_posX - common.lod);
	uint32 lod_negZ = max(0, common.lod_negZ - common.lod);
	uint32 lod_posZ = max(0, common.lod_posZ - common.lod);

	uint32 scaleDownByLODs = SCALE_DOWN_BY_LODS(lod_negX, lod_posX, lod_negZ, lod_posZ);

	return terrain_cb{ common.minCorner, (uint32)common.lod, common.chunkSize, common.amplitudeScale, scaleDownByLODs };
}

PIPELINE_RENDER_IMPL(terrain_pipeline, terrain_render_data)
{
	PROFILE_ALL(cl, "Terrain");

	uint32 numSegmentsPerDim = (TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1) >> data.common.lod;
	uint32 numTris = numSegmentsPerDim * numSegmentsPerDim * 2;

	auto cb = getTerrainCB(data.common);

	cl->setGraphics32BitConstants(TERRAIN_RS_TRANSFORM, viewProj);
	cl->setGraphics32BitConstants(TERRAIN_RS_CB, cb);
	cl->setGraphicsDynamicConstantBuffer(TERRAIN_RS_WATER, data.waterPlanesCBV);
	cl->setDescriptorHeapSRV(TERRAIN_RS_HEIGHTMAP, 0, data.common.heightmap);
	cl->setDescriptorHeapSRV(TERRAIN_RS_NORMALMAP, 0, data.normalmap);

	cl->setDescriptorHeapSRV(TERRAIN_RS_TEXTURES, 0, data.groundMaterial->albedo ? data.groundMaterial->albedo : render_resources::blackTexture);
	cl->setDescriptorHeapSRV(TERRAIN_RS_TEXTURES, 1, data.groundMaterial->normal ? data.groundMaterial->normal : render_resources::defaultNormalMap);
	cl->setDescriptorHeapSRV(TERRAIN_RS_TEXTURES, 2, data.groundMaterial->roughness ? data.groundMaterial->roughness : render_resources::whiteTexture);

	cl->setDescriptorHeapSRV(TERRAIN_RS_TEXTURES, 3, data.rockMaterial->albedo ? data.rockMaterial->albedo : render_resources::blackTexture);
	cl->setDescriptorHeapSRV(TERRAIN_RS_TEXTURES, 4, data.rockMaterial->normal ? data.rockMaterial->normal : render_resources::defaultNormalMap);
	cl->setDescriptorHeapSRV(TERRAIN_RS_TEXTURES, 5, data.rockMaterial->roughness ? data.rockMaterial->roughness : render_resources::whiteTexture);

	cl->setDescriptorHeapSRV(TERRAIN_RS_TEXTURES, 6, data.mudMaterial->albedo ? data.mudMaterial->albedo : render_resources::blackTexture);
	cl->setDescriptorHeapSRV(TERRAIN_RS_TEXTURES, 7, data.mudMaterial->normal ? data.mudMaterial->normal : render_resources::defaultNormalMap);
	cl->setDescriptorHeapSRV(TERRAIN_RS_TEXTURES, 8, data.mudMaterial->roughness ? data.mudMaterial->roughness : render_resources::whiteTexture);

	cl->setIndexBuffer(terrainIndexBuffers[data.common.lod]);
	cl->drawIndexed(numTris * 3, 1, 0, 0, 0);
}


PIPELINE_SETUP_IMPL(terrain_depth_prepass_pipeline)
{
	cl->setPipelineState(*terrainDepthOnlyPipeline.pipeline);
	cl->setGraphicsRootSignature(*terrainDepthOnlyPipeline.rootSignature);

	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	cl->setGraphicsDynamicConstantBuffer(TERRAIN_DEPTH_ONLY_RS_CAMERA, common.cameraCBV);
}

DEPTH_ONLY_RENDER_IMPL(terrain_depth_prepass_pipeline, terrain_render_data_common)
{
	PROFILE_ALL(cl, "Terrain depth prepass");

	uint32 numSegmentsPerDim = (TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1) >> data.lod;
	uint32 numTris = numSegmentsPerDim * numSegmentsPerDim * 2;

	cl->setGraphics32BitConstants(TERRAIN_DEPTH_ONLY_RS_OBJECT_ID, data.objectID);
	cl->setGraphics32BitConstants(TERRAIN_DEPTH_ONLY_RS_TRANSFORM, depth_only_transform_cb{ viewProj, prevFrameViewProj });

	auto cb = getTerrainCB(data);
	cl->setGraphics32BitConstants(TERRAIN_DEPTH_ONLY_RS_CB, cb);
	cl->setDescriptorHeapSRV(TERRAIN_DEPTH_ONLY_RS_HEIGHTMAP, 0, data.heightmap);

	cl->setIndexBuffer(terrainIndexBuffers[data.lod]);
	cl->drawIndexed(numTris * 3, 1, 0, 0, 0);
}


PIPELINE_SETUP_IMPL(terrain_shadow_pipeline)
{
	cl->setPipelineState(*terrainShadowPipeline.pipeline);
	cl->setGraphicsRootSignature(*terrainShadowPipeline.rootSignature);

	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

PIPELINE_RENDER_IMPL(terrain_shadow_pipeline, terrain_render_data_common)
{
	PROFILE_ALL(cl, "Terrain shadow");

	uint32 numSegmentsPerDim = (TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1) >> data.lod;
	uint32 numTris = numSegmentsPerDim * numSegmentsPerDim * 2;

	cl->setGraphics32BitConstants(TERRAIN_SHADOW_RS_TRANSFORM, viewProj);
	auto cb = getTerrainCB(data);
	cl->setGraphics32BitConstants(TERRAIN_SHADOW_RS_CB, cb);
	cl->setDescriptorHeapSRV(TERRAIN_SHADOW_RS_HEIGHTMAP, 0, data.heightmap);

	cl->setIndexBuffer(terrainIndexBuffers[data.lod]);
	cl->drawIndexed(numTris * 3, 1, 0, 0, 0);
}


PIPELINE_SETUP_IMPL(terrain_outline_pipeline)
{
	cl->setPipelineState(*terrainOutlinePipeline.pipeline);
	cl->setGraphicsRootSignature(*terrainOutlinePipeline.rootSignature);

	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

PIPELINE_RENDER_IMPL(terrain_outline_pipeline, terrain_render_data_common)
{
	PROFILE_ALL(cl, "Terrain outline");

	uint32 numSegmentsPerDim = (TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1) >> data.lod;
	uint32 numTris = numSegmentsPerDim * numSegmentsPerDim * 2;

	cl->setGraphics32BitConstants(TERRAIN_OUTLINE_RS_TRANSFORM, terrain_transform_cb{ viewProj });

	auto cb = getTerrainCB(data);
	cl->setGraphics32BitConstants(TERRAIN_OUTLINE_RS_CB, cb);
	cl->setDescriptorHeapSRV(TERRAIN_OUTLINE_RS_HEIGHTMAP, 0, data.heightmap);

	cl->setIndexBuffer(terrainIndexBuffers[data.lod]);
	cl->drawIndexed(numTris * 3, 1, 0, 0, 0);
}















struct height_generator
{
	fbm_noise_2D noiseFunc = valueNoise;

	virtual float height(vec2 position) const = 0;
	virtual vec2 grad(vec2 position) const = 0;
};

struct height_generator_warped : height_generator
{
	terrain_generation_settings settings;

	virtual float height(vec2 position) const override
	{
		vec2 fbmPosition = position * settings.scale;

		vec3 domainWarpValue = fbm(noiseFunc, fbmPosition + settings.domainWarpNoiseOffset, settings.domainWarpOctaves);

		vec2 warpedFbmPosition = fbmPosition + vec2(domainWarpValue.x * settings.domainWarpStrength) + settings.noiseOffset + vec2(1000.f);
		vec3 value = fbm(noiseFunc, warpedFbmPosition); // We are using a lower number of noise octaves here, since the heightmap is lowres anyway.
		float height = value.x;

		height = height * 0.5f + 0.5f;

		return height;
	}

	virtual vec2 grad(vec2 position) const override
	{
		vec2 fbmPosition = position * settings.scale;
		float J_fbmPosition_position = settings.scale;

		vec3 domainWarpValue = fbm(noiseFunc, fbmPosition + settings.domainWarpNoiseOffset, settings.domainWarpOctaves);
		float domainWarpHeight = domainWarpValue.x;
		vec2 J_domainWarpHeight_fbmPosition = domainWarpValue.yz;

		vec2 warpedFbmPosition = fbmPosition + vec2(domainWarpHeight * settings.domainWarpStrength) + settings.noiseOffset + vec2(1000.f);
		float J_warpedFbmPosition_fbmPosition = 1.f;
		float J_warpedFbmPosition_domainWarpHeight = settings.domainWarpStrength;

		vec3 value = fbm(noiseFunc, warpedFbmPosition, settings.noiseOctaves);
		float height = value.x;
		vec2 J_height_warpedFbmPosition = value.yz;

		float scaledHeight = height * 0.5f + 0.5f;
		float J_scaledHeight_height = 0.5f;

		vec2 grad = J_scaledHeight_height * J_height_warpedFbmPosition *
			(J_warpedFbmPosition_fbmPosition + J_warpedFbmPosition_domainWarpHeight * J_domainWarpHeight_fbmPosition) * J_fbmPosition_position;

		return grad;
	}
};

struct height_generator_layered : height_generator
{
	float largeScaleWeight = 0.2f;
	float smallScaleWeight = 0.8f;

	virtual float height(vec2 position) const override
	{
		vec2 fbmPosition = position * 0.01f;

		vec2 largeScaleFbmPosition = position * 0.0001f;

		vec3 value = fbm(noiseFunc, fbmPosition);
		float height = value.x;

		vec3 largeScaleValue = fbm(noiseFunc, largeScaleFbmPosition, 3);
		float largeScaleHeight = largeScaleValue.x;

		height = largeScaleHeight * largeScaleWeight + height * smallScaleWeight;

		height = abs(height);

		height = 1.f - height;
		height = height * height;

		return height;
	}

	virtual vec2 grad(vec2 position) const override
	{
		vec2 fbmPosition = position * 0.01f;
		float J_fbmPosition_position = 0.01f;

		vec2 largeScaleFbmPosition = position * 0.0001f;
		float J_largeScaleFbmPosition_position = 0.0001f;

		vec3 value = fbm(noiseFunc, fbmPosition);
		float height = value.x;
		vec2 J_height_fbmPosition = value.yz;

		vec3 largeScaleValue = fbm(noiseFunc, largeScaleFbmPosition, 3);
		float largeScaleHeight = largeScaleValue.x;
		vec2 J_largeScaleHeight_largeScaleFbmPosition = largeScaleValue.yz;

		float combinedHeight = largeScaleHeight * largeScaleWeight + height * smallScaleWeight;
		vec2 J_combinedHeight_height = smallScaleWeight;
		vec2 J_combinedHeight_largeScaleHeight = largeScaleWeight;

		float absHeight = (combinedHeight < 0.f) ? -combinedHeight : combinedHeight;
		float J_absHeight_combinedHeight = (combinedHeight < 0.f) ? -1.f : 1.f;

		float oneMinusHeight = 1.f - absHeight;
		float J_oneMinusHeight_absHeight = -1.f;

		float powHeight = oneMinusHeight * oneMinusHeight;
		float J_powHeight_oneMinusHeight = 2.f * oneMinusHeight;

		vec2 J_combinedHeight_position =
			J_combinedHeight_height * J_height_fbmPosition * J_fbmPosition_position
			+ J_combinedHeight_largeScaleHeight * J_largeScaleHeight_largeScaleFbmPosition * J_largeScaleFbmPosition_position;

		vec2 grad =
			J_powHeight_oneMinusHeight
			* J_oneMinusHeight_absHeight
			* J_absHeight_combinedHeight
			* J_combinedHeight_position;

		return grad;
	}
};



const uint32 normalMapDimension = 2048;


terrain_component::terrain_component(uint32 chunksPerDim, float chunkSize, float amplitudeScale, 
	ref<pbr_material> groundMaterial, ref<pbr_material> rockMaterial, ref<pbr_material> mudMaterial,
	terrain_generation_settings genSettings)
	: chunksPerDim(chunksPerDim), 
	chunkSize(chunkSize), 
	genSettings(genSettings)
{
	oldGenSettings.scale = -FLT_MAX; // Set to garbage so that it is updated in the first frame.

	this->amplitudeScale = amplitudeScale;
	this->chunks.resize(chunksPerDim * chunksPerDim);

	this->groundMaterial = groundMaterial;
	this->rockMaterial = rockMaterial;
	this->mudMaterial = mudMaterial;
}

void terrain_component::generateChunksCPU()
{
	height_generator_warped generator;
	generator.settings = genSettings;

	struct terrain_gen_job_data
	{
		terrain_component& terrain;
		height_generator_warped generator;
	};

	terrain_gen_job_data data =
	{
		*this,
		generator,
	};

	job_handle parentJob = highPriorityJobQueue.createJob<terrain_gen_job_data>([](terrain_gen_job_data& data, job_handle parent)
	{
		for (int32 cz = 0; cz < (int32)data.terrain.chunksPerDim; ++cz)
		{
			for (int32 cx = 0; cx < (int32)data.terrain.chunksPerDim; ++cx)
			{
				struct chunk_gen_job_data
				{
					terrain_component& terrain;
					height_generator_warped generator;
					int32 cx, cz;
				};

				chunk_gen_job_data chunkData = 
				{
					data.terrain,
					data.generator,
					cx, cz,
				};

				job_handle job = highPriorityJobQueue.createJob<chunk_gen_job_data>([](chunk_gen_job_data& data, job_handle)
				{
					float chunkSize = data.terrain.chunkSize;
					uint32 numSegmentsPerDim = TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1;
					float positionScale = chunkSize / (float)numSegmentsPerDim;
					float normalScale = chunkSize / (float)(normalMapDimension - 1);

					int32 cx = data.cx;
					int32 cz = data.cz;
					height_generator_warped& generator = data.generator;
					float amplitudeScale = data.terrain.amplitudeScale;


					vec2 minCorner = vec2(cx * chunkSize, cz * chunkSize);

					auto& c = data.terrain.chunk(cx, cz);

					c.heights.resize(TERRAIN_LOD_0_VERTICES_PER_DIMENSION* TERRAIN_LOD_0_VERTICES_PER_DIMENSION);
					uint16* heights = c.heights.data();
					vec2* normals = new vec2[normalMapDimension * normalMapDimension];

					float minHeight = FLT_MAX;
					float maxHeight = -FLT_MAX;

					for (uint32 z = 0; z < TERRAIN_LOD_0_VERTICES_PER_DIMENSION; ++z)
					{
						for (uint32 x = 0; x < TERRAIN_LOD_0_VERTICES_PER_DIMENSION; ++x)
						{
							vec2 position = vec2(x * positionScale, z * positionScale) + minCorner;

							float height = generator.height(position);

							minHeight = min(minHeight, height * amplitudeScale);
							maxHeight = max(maxHeight, height * amplitudeScale);

							ASSERT(height >= 0.f);
							ASSERT(height <= 1.f);

							heights[z * TERRAIN_LOD_0_VERTICES_PER_DIMENSION + x] = (uint16)(height * UINT16_MAX);
						}
					}

					c.heightmap = createTexture(heights, TERRAIN_LOD_0_VERTICES_PER_DIMENSION, TERRAIN_LOD_0_VERTICES_PER_DIMENSION, DXGI_FORMAT_R16_UNORM, false, false, true, D3D12_RESOURCE_STATE_GENERIC_READ);


					for (uint32 z = 0; z < normalMapDimension; ++z)
					{
						for (uint32 x = 0; x < normalMapDimension; ++x)
						{
							vec2 position = vec2(x * normalScale, z * normalScale) + minCorner;

							vec2 grad = generator.grad(position);

							normals[z * normalMapDimension + x] = -grad;
						}
					}

					c.normalmap = createTexture(normals, normalMapDimension, normalMapDimension, DXGI_FORMAT_R32G32_FLOAT);

					delete[] normals;
				}, chunkData, parent);
			}
		}
	}, data);

	parentJob.submitNow();
	parentJob.waitForCompletion();
}

void terrain_component::generateChunksGPU()
{
	uint32 numSegmentsPerDim = TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1;
	float positionScale = chunkSize / (float)numSegmentsPerDim;
	float normalScale = chunkSize / (float)(normalMapDimension - 1);

	terrain_generation_settings_cb settings;
	settings.heightWidth = TERRAIN_LOD_0_VERTICES_PER_DIMENSION;
	settings.heightHeight = TERRAIN_LOD_0_VERTICES_PER_DIMENSION;
	settings.normalWidth = normalMapDimension;
	settings.normalHeight = normalMapDimension;
	settings.positionScale = positionScale;
	settings.normalScale = normalScale;

	settings.scale = genSettings.scale;
	settings.domainWarpStrength = genSettings.domainWarpStrength;
	settings.domainWarpNoiseOffset = genSettings.domainWarpNoiseOffset;
	settings.domainWarpOctaves = genSettings.domainWarpOctaves;
	settings.noiseOffset = genSettings.noiseOffset;
	settings.noiseOctaves = genSettings.noiseOctaves;

	auto settingsCBV = dxContext.uploadDynamicConstantBuffer(settings);


	bool mipmaps = true;


	dx_command_list* cl = dxContext.getFreeRenderCommandList();

	{
		PROFILE_ALL(cl, "Generate terrain chunks");

		for (int32 cz = 0; cz < (int32)chunksPerDim; ++cz)
		{
			for (int32 cx = 0; cx < (int32)chunksPerDim; ++cx)
			{
				cl->setPipelineState(*terrainGenerationPipeline.pipeline);
				cl->setComputeRootSignature(*terrainGenerationPipeline.rootSignature);
				cl->setComputeDynamicConstantBuffer(TERRAIN_GENERATION_RS_SETTINGS, settingsCBV);


				vec2 minCorner = vec2(cx * chunkSize, cz * chunkSize);

				auto& c = chunk(cx, cz);

				if (!c.heightmap)
				{
					c.heightmap = createTexture(0, TERRAIN_LOD_0_VERTICES_PER_DIMENSION, TERRAIN_LOD_0_VERTICES_PER_DIMENSION, DXGI_FORMAT_R16_UNORM, false, false, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					c.normalmap = createTexture(0, normalMapDimension, normalMapDimension, DXGI_FORMAT_R32G32_FLOAT, mipmaps, false, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				}
				else
				{
					barrier_batcher(cl)
						.transition(c.heightmap, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
						.transition(c.normalmap, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				}

				terrain_generation_cb cb;
				cb.minCorner = minCorner;

				cl->setCompute32BitConstants(TERRAIN_GENERATION_RS_CB, cb);
				cl->setDescriptorHeapUAV(TERRAIN_GENERATION_RS_TEXTURES, 0, c.heightmap);
				cl->setDescriptorHeapUAV(TERRAIN_GENERATION_RS_TEXTURES, 1, c.normalmap);

				cl->dispatch(bucketize(normalMapDimension, 16), bucketize(normalMapDimension, 16));






				barrier_batcher(cl)
					.transition(c.heightmap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ)
					.transition(c.normalmap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);

				if (mipmaps)
				{
					generateMipMapsOnGPU(cl, c.normalmap);
				}

			}
		}

	}

	dxContext.executeCommandList(cl);


	{
		CPU_PROFILE_BLOCK("Copy heights to CPU");

		for (int32 cz = 0; cz < (int32)chunksPerDim; ++cz)
		{
			for (int32 cx = 0; cx < (int32)chunksPerDim; ++cx)
			{
				auto& c = chunk(cx, cz);

				c.heights.resize(TERRAIN_LOD_0_VERTICES_PER_DIMENSION * TERRAIN_LOD_0_VERTICES_PER_DIMENSION);
				copyTextureToCPUBuffer(c.heightmap, c.heights.data(), D3D12_RESOURCE_STATE_GENERIC_READ);
			}
		}
	}
}

void terrain_component::update(vec3 positionOffset, heightmap_collider_component* collider)
{
	if (memcmp(&genSettings, &oldGenSettings, sizeof(terrain_generation_settings)) != 0)
	{
		generateChunksGPU();

		oldGenSettings = genSettings;

		if (collider)
		{
			for (int32 cz = 0; cz < (int32)chunksPerDim; ++cz)
			{
				for (int32 cx = 0; cx < (int32)chunksPerDim; ++cx)
				{
					auto& terrainChunk = chunk(cx, cz);
					auto& terrainCollider = collider->collider(cx, cz);

					terrainCollider.setHeights(terrainChunk.heights.data());
				}
			}
		}
	}

	if (collider)
	{
		collider->update(getMinCorner(positionOffset), amplitudeScale);
	}
}

void terrain_component::render(const render_camera& camera, struct opaque_render_pass* renderPass, struct sun_shadow_render_pass* shadowPass, struct ldr_render_pass* ldrPass,
	vec3 positionOffset, uint32 entityID, bool selected,
	struct position_scale_component* waterPlaneTransforms, uint32 numWaters)
{
	camera_frustum_planes frustum = camera.getWorldSpaceFrustumPlanes();
	camera_frustum_planes sunFrustum = {};
	if (shadowPass)
	{
		sunFrustum = getWorldSpaceFrustumPlanes(shadowPass->cascades[shadowPass->numCascades - 1].viewProj);
	}

	positionOffset = getMinCorner(positionOffset);

	int32 lodStride = (chunksPerDim + 2);
	uint32 paddedNumChunks = (chunksPerDim + 2) * (chunksPerDim + 2);
	int32* lodBuffer = (int32*)alloca(sizeof(int32) * paddedNumChunks);
	int32* lods = lodBuffer + lodStride + 1;

	vec3 chunkCenterOffset = vec3(chunkSize, 0.f, chunkSize) * 0.5f;

	for (int32 z = -1; z < (int32)chunksPerDim + 1; ++z)
	{
		for (int32 x = -1; x < (int32)chunksPerDim + 1; ++x)
		{
			vec3 localMinCorner(x * chunkSize, 0.f, z * chunkSize);
			vec3 minCorner = localMinCorner + positionOffset;
			vec3 chunkCenter = minCorner + chunkCenterOffset;

			float distance = length(chunkCenter - camera.position);
			int32 lod = (int32)(saturate(distance / 500.f) * TERRAIN_MAX_LOD);
			lods[z * lodStride + x] = lod;
		}
	}

	terrain_water_plane_cb waterPlanes;
	waterPlanes.numWaterPlanes = min(numWaters, 4u);
	for (uint32 i = 0; i < waterPlanes.numWaterPlanes; ++i)
	{
		vec3 pos = waterPlaneTransforms[i].position;
		vec3 scale = waterPlaneTransforms[i].scale;
		waterPlanes.waterMinMaxXZ[i] = vec4(pos.x, pos.z, pos.x, pos.z) + vec4(-scale.x, -scale.z, scale.x, scale.z);
		waterPlanes.waterHeights.data[i] = pos.y;
	}

	auto waterCBV = dxContext.uploadDynamicConstantBuffer(waterPlanes);

	for (int32 z = 0; z < (int32)chunksPerDim; ++z)
	{
		for (int32 x = 0; x < (int32)chunksPerDim; ++x)
		{
			const terrain_chunk& c = chunk(x, z);

			int32 lod = lods[z * lodStride + x];

			vec3 localMinCorner(x * chunkSize, 0.f, z * chunkSize);
			vec3 minCorner = localMinCorner + positionOffset;
			vec3 maxCorner = minCorner + vec3(chunkSize, amplitudeScale, chunkSize);

			terrain_render_data_common common =
			{
				minCorner,
				lod,
				chunkSize,
				amplitudeScale,
				lods[(z)*lodStride + (x - 1)],
				lods[(z)*lodStride + (x + 1)],
				lods[(z - 1) * lodStride + (x)],
				lods[(z + 1) * lodStride + (x)],
				c.heightmap,
				entityID,
			};

			bounding_box aabb = { minCorner, maxCorner };
			if (!frustum.cullWorldSpaceAABB(aabb))
			{
				terrain_render_data data = {
					common,
					c.normalmap,
					groundMaterial, rockMaterial, mudMaterial,
					waterCBV
				};
				renderPass->renderObject<terrain_pipeline, terrain_depth_prepass_pipeline>(data, common);
			}

			if (shadowPass)
			{
				if (!sunFrustum.cullWorldSpaceAABB(aabb))
				{
					shadowPass->renderStaticObject<terrain_shadow_pipeline>(0, common);
				}
			}

			if (ldrPass && selected)
			{
				ldrPass->renderOutline<terrain_outline_pipeline>(common);
			}
		}
	}
}















