#include "pch.h"
#include "debris_particle_system.h"
#include "dx/dx_pipeline.h"
#include "rendering/render_resources.h"
#include "rendering/render_utils.h"
#include "dx/dx_profiling.h"


dx_pipeline debris_particle_system::emitPipeline;
dx_pipeline debris_particle_system::simulatePipeline;
dx_pipeline debris_particle_system::renderPipeline;


void debris_particle_system::initializePipeline()
{
#define name "debris_particle_system"

	emitPipeline = createReloadablePipeline(EMIT_PIPELINE_NAME(name));
	simulatePipeline = createReloadablePipeline(SIMULATE_PIPELINE_NAME(name));

	auto desc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout_position)
		.renderTargets(transparentLightPassFormats, arraysize(transparentLightPassFormats), depthStencilFormat)
		.alphaBlending(0)
		.depthSettings(true, false);

	renderPipeline = createReloadablePipeline(desc, { VERTEX_SHADER_NAME(name), PIXEL_SHADER_NAME(name) });

#undef name
}

debris_particle_system::debris_particle_system(uint32 maxNumParticles)
{
	initialize(maxNumParticles);
}

void debris_particle_system::initialize(uint32 maxNumParticles)
{
	particle_system::initializeAsBillboard(sizeof(debris_particle_data), maxNumParticles, sort_mode_back_to_front);
}

void debris_particle_system::burst(vec3 position)
{
	bursts.push_back({ position });
}

void debris_particle_system::update(struct dx_command_list* cl, vec3 cameraPosition, float dt)
{
	struct setter : public particle_parameter_setter
	{
		debris_simulation_cb cb;

		virtual void setRootParameters(dx_command_list* cl) override
		{
			cl->setCompute32BitConstants(DEBRIS_PARTICLE_SYSTEM_COMPUTE_RS_CBV, cb);
		}
	};

	setter s;
	s.cb.cameraPosition = cameraPosition;
	s.cb.frameIndex = (uint32)dxContext.frameID;

	uint32 numBursts = (uint32)min(arraysize(s.cb.emitPositions), bursts.size());
	uint32 numNewParticles = numBursts * 256;
	for (uint32 i = 0; i < numBursts; ++i)
	{
		s.cb.emitPositions[i] = vec4(bursts[i].position, 1.f);
	}

	updateInternal(cl, (float)numNewParticles, dt, emitPipeline, simulatePipeline, &s);

	bursts.clear();
}

void debris_particle_system::render(transparent_render_pass* renderPass)
{
	debris_material material;

	renderPass->renderParticles<debris_pipeline>(billboardMesh.vertexBuffer, billboardMesh.indexBuffer,
		getDrawInfo(renderPipeline),
		material);
}

PIPELINE_SETUP_IMPL(debris_particle_system::debris_pipeline)
{
	cl->setPipelineState(*renderPipeline.pipeline);
	cl->setGraphicsRootSignature(*renderPipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cl->setGraphicsDynamicConstantBuffer(DEBRIS_PARTICLE_SYSTEM_RENDERING_RS_CAMERA, common.cameraCBV);
}

PARTICLE_PIPELINE_RENDER_IMPL(debris_particle_system::debris_pipeline)
{
	particle_render_pipeline::render(cl, viewProj, rc);
}




