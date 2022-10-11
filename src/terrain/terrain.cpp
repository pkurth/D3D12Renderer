#include "pch.h"
#include "terrain.h"

#include "dx/dx_command_list.h"
#include "rendering/render_utils.h"

#include "terrain_rs.hlsli"


static dx_pipeline terrainPipeline;
static ref<dx_index_buffer> terrainIndexBuffers[TERRAIN_MAX_LOD + 1];

terrain_component::terrain_component(float chunkSize, float amplitudeScale)
{
	this->chunkSize = chunkSize;
	this->amplitudeScale = amplitudeScale;
}

void terrain_pipeline::initialize()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.renderTargets(opaqueLightPassFormats, arraysize(opaqueLightPassFormats), depthStencilFormat)
		.wireframe();

	terrainPipeline = createReloadablePipeline(desc, { "terrain_vs", "terrain_ps" });



	uint32 numSegmentsPerDim = TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1;
	uint32 numTrisLod0 = numSegmentsPerDim * numSegmentsPerDim * 2;

	indexed_triangle16* tris = new indexed_triangle16[numTrisLod0];

	for (uint32 lod = 0; lod < TERRAIN_MAX_LOD; ++lod)
	{
		uint32 numSegmentsPerDim = (TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1) >> lod;
		uint32 stride = numSegmentsPerDim + 1;
		uint32 numTris = numSegmentsPerDim * numSegmentsPerDim * 2;

		uint32 i = 0;
		for (uint32 z = 0; z < numSegmentsPerDim; ++z)
		{
			for (uint32 x = 0; x < numSegmentsPerDim; ++x)
			{
				tris[i++] = { (uint16)(stride * z + x), (uint16)(stride * (z + 1) + x), (uint16)(stride * z + x + 1) };
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
	cl->setGraphics32BitConstants(TERRAIN_RS_CB, terrain_cb{ (uint32)rc.data.lod, rc.data.minCorner, rc.data.amplitudeScale, rc.data.chunkSize, scaleDownByLODs });

	cl->setIndexBuffer(terrainIndexBuffers[rc.data.lod]);
	cl->drawIndexed(numTris * 3, 1, 0, 0, 0);
}
