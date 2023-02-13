#include "pch.h"
#include "water.h"

#include "dx/dx_buffer.h"
#include "dx/dx_pipeline.h"
#include "dx/dx_profiling.h"

#include "rendering/render_utils.h"
#include "rendering/render_pass.h"
#include "rendering/render_resources.h"

#include "geometry/mesh_builder.h"

#include "transform.hlsli"


static dx_pipeline waterPipeline;

static dx_mesh waterMesh;
static submesh_info waterSubmesh;

void initializeWaterPipelines()
{
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position)
			.renderTargets(transparentLightPassFormats, arraysize(transparentLightPassFormats), depthStencilFormat);

		waterPipeline = createReloadablePipeline(desc, { "water_vs", "water_ps" });
	}

	mesh_builder builder(mesh_creation_flags_with_positions);
	builder.pushQuad({ vec3(0.f), 10.f, quat(vec3(1.f, 0.f, 0.f), deg2rad(-90.f)) });
	waterSubmesh = builder.endSubmesh();
	waterMesh = builder.createDXMesh();
}


struct water_render_data
{
	mat4 m;
};

struct water_pipeline
{
	using render_data_t = water_render_data;
	
	PIPELINE_SETUP_DECL;
	PIPELINE_RENDER_DECL;
};

PIPELINE_SETUP_IMPL(water_pipeline)
{
	cl->setPipelineState(*waterPipeline.pipeline);
	cl->setGraphicsRootSignature(*waterPipeline.rootSignature);

	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	cl->setDescriptorHeapSRV(1, 0, common.opaqueColor);
	cl->setDescriptorHeapSRV(1, 1, common.opaqueDepth);
}

PIPELINE_RENDER_IMPL(water_pipeline)
{
	PROFILE_ALL(cl, "Water");

	cl->setGraphics32BitConstants(0, viewProj * rc.data.m);
	cl->setVertexBuffer(0, waterMesh.vertexBuffer.positions);
	cl->setIndexBuffer(waterMesh.indexBuffer);
	cl->drawIndexed(waterSubmesh.numIndices, 1, 0, 0, 0);
}

void water_component::render(const render_camera& camera, transparent_render_pass* renderPass, vec3 positionOffset, uint32 entityID)
{
	renderPass->renderObject<water_pipeline>({ createModelMatrix(positionOffset, quat::identity) });
}
