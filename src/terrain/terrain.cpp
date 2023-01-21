#include "pch.h"
#include "terrain.h"

#include "dx/dx_context.h"
#include "dx/dx_command_list.h"
#include "dx/dx_barrier_batcher.h"
#include "dx/dx_profiling.h"

#include "rendering/render_utils.h"
#include "rendering/render_pass.h"
#include "rendering/render_resources.h"
#include "rendering/texture_preprocessing.h"

#include "core/random.h"

#include "terrain_rs.hlsli"


static dx_pipeline terrainGenerationPipeline;
static dx_pipeline terrainPipeline;
static ref<dx_index_buffer> terrainIndexBuffers[TERRAIN_MAX_LOD + 1];


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


terrain_component::terrain_component(uint32 chunksPerDim, float chunkSize, float amplitudeScale, ref<pbr_material> groundMaterial, ref<pbr_material> rockMaterial,
	terrain_generation_settings genSettings)
	: chunksPerDim(chunksPerDim), chunkSize(chunkSize), genSettings(genSettings), oldGenSettings(genSettings)
{
	this->amplitudeScale = amplitudeScale;
	this->chunks.resize(chunksPerDim * chunksPerDim);

	this->groundMaterial = groundMaterial;
	this->rockMaterial = rockMaterial;

	generateChunksGPU();
}

void terrain_component::generateChunksCPU()
{
	thread_job_context context;

	height_generator_warped generator;
	generator.settings = genSettings;

	uint32 numSegmentsPerDim = TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1;
	float positionScale = chunkSize / (float)numSegmentsPerDim;
	float normalScale = chunkSize / (float)(normalMapDimension - 1);

	for (int32 cz = 0; cz < (int32)chunksPerDim; ++cz)
	{
		for (int32 cx = 0; cx < (int32)chunksPerDim; ++cx)
		{
			context.addWork([this, cx, cz, generator, positionScale, normalScale]()
			{
				vec2 minCorner = vec2(cx * chunkSize, cz * chunkSize);

				uint16* heights = new uint16[TERRAIN_LOD_0_VERTICES_PER_DIMENSION * TERRAIN_LOD_0_VERTICES_PER_DIMENSION];
				vec2* normals = new vec2[normalMapDimension * normalMapDimension];

				auto& c = chunk(cx, cz);

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

						assert(height >= 0.f);
						assert(height <= 1.f);

						heights[z * TERRAIN_LOD_0_VERTICES_PER_DIMENSION + x] = (uint16)(height * UINT16_MAX);
					}
				}

				c.minHeight = minHeight;
				c.maxHeight = maxHeight;
				c.heightmap = createTexture(heights, TERRAIN_LOD_0_VERTICES_PER_DIMENSION, TERRAIN_LOD_0_VERTICES_PER_DIMENSION, DXGI_FORMAT_R16_UNORM);


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
				delete[] heights;
			});
		}
	}

	context.waitForWorkCompletion();
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
						.transition(c.heightmap, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
						.transition(c.normalmap, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				}

				terrain_generation_cb cb;
				cb.minCorner = minCorner;

				cl->setCompute32BitConstants(TERRAIN_GENERATION_RS_CB, cb);
				cl->setDescriptorHeapUAV(TERRAIN_GENERATION_RS_TEXTURES, 0, c.heightmap);
				cl->setDescriptorHeapUAV(TERRAIN_GENERATION_RS_TEXTURES, 1, c.normalmap);

				cl->dispatch(bucketize(normalMapDimension, 16), bucketize(normalMapDimension, 16));






				barrier_batcher(cl)
					.transition(c.heightmap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON)
					.transition(c.normalmap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);

				if (mipmaps)
				{
					generateMipMapsOnGPU(cl, c.normalmap);
				}

				c.minHeight = 0.f;
				c.maxHeight = amplitudeScale;
			}
		}

	}

	dxContext.executeCommandList(cl);
}

void terrain_component::render(const render_camera& camera, opaque_render_pass* renderPass, vec3 positionOffset)
{
	if (memcmp(&genSettings, &oldGenSettings, sizeof(terrain_generation_settings)) != 0)
	{
		generateChunksGPU();

		oldGenSettings = genSettings;
	}


	camera_frustum_planes frustum = camera.getWorldSpaceFrustumPlanes();

	float xzOffset = -(chunkSize * chunksPerDim) * 0.5f; // Offsets entire terrain by half.
	positionOffset += vec3(xzOffset, 0.f, xzOffset);

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

	for (int32 z = 0; z < (int32)chunksPerDim; ++z)
	{
		for (int32 x = 0; x < (int32)chunksPerDim; ++x)
		{
			const terrain_chunk& c = chunk(x, z);

			if (c.active)
			{
				int32 lod = lods[z * lodStride + x];

				vec3 localMinCorner(x * chunkSize, 0.f, z * chunkSize);
				vec3 minCorner = localMinCorner + positionOffset;
				vec3 maxCorner = minCorner + vec3(chunkSize, 0.f, chunkSize);


				bounding_box aabb = { minCorner, maxCorner };
				aabb.minCorner.y += c.minHeight;
				aabb.maxCorner.y += c.maxHeight;

				if (!frustum.cullWorldSpaceAABB(aabb))
				{
					terrain_render_data data = { 
						minCorner, 
						lod, 
						chunkSize, 
						amplitudeScale,
						lods[(z) * lodStride + (x - 1)],
						lods[(z) * lodStride + (x + 1)],
						lods[(z - 1) * lodStride + (x)],
						lods[(z + 1) * lodStride + (x)],
						c.heightmap,
						c.normalmap,
						groundMaterial, rockMaterial 
					};
					renderPass->renderStaticObject<terrain_pipeline>(mat4::identity, {}, {}, {}, data, -1, false, false);
				}
			}
		}
	}
}

void terrain_pipeline::initialize()
{
	terrainGenerationPipeline = createReloadablePipeline("terrain_generation_cs");

	auto desc = CREATE_GRAPHICS_PIPELINE
		//.wireframe()
		.renderTargets(opaqueLightPassFormats, arraysize(opaqueLightPassFormats), depthStencilFormat);

	terrainPipeline = createReloadablePipeline(desc, { "terrain_vs", "terrain_ps" });



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

	cl->setDescriptorHeapSRV(TERRAIN_RS_FRAME_CONSTANTS, 0, common.irradiance);
	cl->setDescriptorHeapSRV(TERRAIN_RS_FRAME_CONSTANTS, 1, common.prefilteredRadiance);
	cl->setDescriptorHeapSRV(TERRAIN_RS_FRAME_CONSTANTS, 2, render_resources::brdfTex);
}

PIPELINE_RENDER_IMPL(terrain_pipeline)
{
	uint32 numSegmentsPerDim = (TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1) >> rc.data.lod;
	uint32 numTris = numSegmentsPerDim * numSegmentsPerDim * 2;

	uint32 lod_negX = max(0, rc.data.lod_negX - rc.data.lod);
	uint32 lod_posX = max(0, rc.data.lod_posX - rc.data.lod);
	uint32 lod_negZ = max(0, rc.data.lod_negZ - rc.data.lod);
	uint32 lod_posZ = max(0, rc.data.lod_posZ - rc.data.lod);

	uint32 scaleDownByLODs = SCALE_DOWN_BY_LODS(lod_negX, lod_posX, lod_negZ, lod_posZ);

	cl->setGraphics32BitConstants(TERRAIN_RS_TRANSFORM, viewProj);
	cl->setGraphics32BitConstants(TERRAIN_RS_CB, terrain_cb{ rc.data.minCorner, (uint32)rc.data.lod, rc.data.chunkSize, rc.data.amplitudeScale, scaleDownByLODs });
	cl->setDescriptorHeapSRV(TERRAIN_RS_HEIGHTMAP, 0, rc.data.heightmap);
	cl->setDescriptorHeapSRV(TERRAIN_RS_NORMALMAP, 0, rc.data.normalmap);

	dx_cpu_descriptor_handle nullTexture = render_resources::nullTextureSRV;

	cl->setDescriptorHeapSRV(TERRAIN_RS_TEXTURES, 0, rc.data.groundMaterial->albedo ? rc.data.groundMaterial->albedo->defaultSRV : nullTexture);
	cl->setDescriptorHeapSRV(TERRAIN_RS_TEXTURES, 1, rc.data.groundMaterial->roughness ? rc.data.groundMaterial->roughness->defaultSRV : nullTexture);
	cl->setDescriptorHeapSRV(TERRAIN_RS_TEXTURES, 2, rc.data.rockMaterial->albedo ? rc.data.rockMaterial->albedo->defaultSRV : nullTexture);
	cl->setDescriptorHeapSRV(TERRAIN_RS_TEXTURES, 3, rc.data.rockMaterial->roughness ? rc.data.rockMaterial->roughness->defaultSRV : nullTexture);

	cl->setIndexBuffer(terrainIndexBuffers[rc.data.lod]);
	cl->drawIndexed(numTris * 3, 1, 0, 0, 0);
}
