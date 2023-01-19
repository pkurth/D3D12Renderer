#include "pch.h"
#include "terrain.h"

#include "dx/dx_command_list.h"

#include "rendering/render_utils.h"
#include "rendering/render_pass.h"

#include "core/random.h"

#include "terrain_rs.hlsli"


static dx_pipeline terrainPipeline;
static ref<dx_index_buffer> terrainIndexBuffers[TERRAIN_MAX_LOD + 1];

terrain_component::terrain_component(uint32 chunksPerDim, float chunkSize, float amplitudeScale)
{
	this->chunksPerDim = chunksPerDim;
	this->chunkSize = chunkSize;
	this->amplitudeScale = amplitudeScale;
	this->chunks.resize(chunksPerDim * chunksPerDim);


	const uint32 normalMapDimension = 1024;


	uint32 numSegmentsPerDim = TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1;
	float positionScale = chunkSize / (float)numSegmentsPerDim;
	float normalScale = chunkSize / (float)(normalMapDimension - 1);

	thread_job_context context;

	for (int32 cz = 0; cz < (int32)chunksPerDim; ++cz)
	{
		for (int32 cx = 0; cx < (int32)chunksPerDim; ++cx)
		{
			context.addWork([this, cx, cz, chunkSize, positionScale, normalScale, normalMapDimension, amplitudeScale]()
			{
				vec2 minCorner = vec2(cx * chunkSize, cz * chunkSize);

				float* heights = new float[TERRAIN_LOD_0_VERTICES_PER_DIMENSION * TERRAIN_LOD_0_VERTICES_PER_DIMENSION];
				vec2* normals = new vec2[normalMapDimension * normalMapDimension];

				auto& c = chunk(cx, cz);

				float minHeight = FLT_MAX;
				float maxHeight = -FLT_MAX;

				for (uint32 z = 0; z < TERRAIN_LOD_0_VERTICES_PER_DIMENSION; ++z)
				{
					for (uint32 x = 0; x < TERRAIN_LOD_0_VERTICES_PER_DIMENSION; ++x)
					{
						vec2 position = vec2(x * positionScale, z * positionScale) + minCorner;

						vec3 value = fbm(position * 0.01f).x;
						float height = value.x;
						height = abs(height);

						height *= amplitudeScale;

						height = amplitudeScale - height;

						minHeight = min(minHeight, height);
						maxHeight = max(maxHeight, height);

						heights[z * TERRAIN_LOD_0_VERTICES_PER_DIMENSION + x] = height;
					}
				}

				c.minHeight = minHeight;
				c.maxHeight = maxHeight;
				c.heightmap = createTexture(heights, TERRAIN_LOD_0_VERTICES_PER_DIMENSION, TERRAIN_LOD_0_VERTICES_PER_DIMENSION, DXGI_FORMAT_R32_FLOAT);


				for (uint32 z = 0; z < normalMapDimension; ++z)
				{
					for (uint32 x = 0; x < normalMapDimension; ++x)
					{
						vec2 position = vec2(x * normalScale, z * normalScale) + minCorner;

						vec3 value = fbm(position * 0.01f);
						float& height = value.x;
						vec2& grad = value.yz;

						grad *= 0.01f;

						if (height < 0.f)
						{
							value = -value;
						}

						grad *= amplitudeScale;

						grad = -grad;

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

void terrain_component::render(const render_camera& camera, opaque_render_pass* renderPass, vec3 positionOffset)
{
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
						lods[(z) * lodStride + (x - 1)],
						lods[(z) * lodStride + (x + 1)],
						lods[(z - 1) * lodStride + (x)],
						lods[(z + 1) * lodStride + (x)],
						c.heightmap,
						c.normalmap };
					renderPass->renderStaticObject<terrain_pipeline>(mat4::identity, {}, {}, {}, data, -1, false, false);
				}
			}
		}
	}
}

void terrain_pipeline::initialize()
{
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
	cl->setGraphics32BitConstants(TERRAIN_RS_CB, terrain_cb{ rc.data.minCorner, (uint32)rc.data.lod, rc.data.chunkSize, scaleDownByLODs });
	cl->setDescriptorHeapSRV(TERRAIN_RS_HEIGHTMAP, 0, rc.data.heightmap);
	cl->setDescriptorHeapSRV(TERRAIN_RS_NORMALMAP, 0, rc.data.normalmap);

	cl->setIndexBuffer(terrainIndexBuffers[rc.data.lod]);
	cl->drawIndexed(numTris * 3, 1, 0, 0, 0);
}
