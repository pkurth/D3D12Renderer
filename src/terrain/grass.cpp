#include "pch.h"
#include "grass.h"

#include "core/math.h"

#include "rendering/render_command.h"
#include "rendering/material.h"
#include "rendering/render_utils.h"

#include "dx/dx_command_list.h"

#include "grass_rs.hlsli"


static dx_pipeline grassPipeline;


void initializeGrassPipelines()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.cullingOff()
		//.wireframe()
		.renderTargets(ldrFormat, depthStencilFormat);

	grassPipeline = createReloadablePipeline(desc, { "grass_vs", "grass_ps" });
}

struct grass_render_data
{
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
}

PIPELINE_RENDER_IMPL(grass_pipeline)
{
	static float time = 0.f;
	time += 1.f / 150.f;

	vec3 windDirection = normalize(vec3(1.f, 0.f, 1.f));

	{
		grass_cb cb;
		cb.mvp = viewProj;
		cb.numVertices = 13;
		cb.halfWidth = 0.1f;
		cb.height = 1.f;
		cb.lod = sin(time / 5.f) * 0.5f + 0.5f;
		cb.time = time;
		cb.windDirection = windDirection;

		cl->setGraphics32BitConstants(GRASS_RS_CB, cb);
		cl->draw(13, 1, 0, 0);
	}

	{
		grass_cb cb;
		cb.mvp = viewProj * createModelMatrix(vec3(0.3f, 0.f, 0.f), quat::identity);
		cb.numVertices = 7;
		cb.halfWidth = 0.1f;
		cb.height = 1.f;
		cb.lod = 0.f;
		cb.time = time;
		cb.windDirection = windDirection;

		cl->setGraphics32BitConstants(GRASS_RS_CB, cb);
		cl->draw(7, 1, 0, 0);
	}
}



void renderBladeOfGrass(ldr_render_pass* renderPass)
{
	renderPass->renderObject<grass_pipeline>({});
}
