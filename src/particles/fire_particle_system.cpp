#include "pch.h"
#include "fire_particle_system.h"
#include "dx/dx_pipeline.h"
#include "rendering/render_resources.h"
#include "rendering/render_utils.h"
#include "dx/dx_profiling.h"


dx_pipeline fire_particle_system::emitPipeline;
dx_pipeline fire_particle_system::simulatePipeline;
dx_pipeline fire_particle_system::renderPipeline;


void fire_particle_system::initializePipeline()
{
#define name "fire_particle_system"

	emitPipeline = createReloadablePipeline(EMIT_PIPELINE_NAME(name));
	simulatePipeline = createReloadablePipeline(SIMULATE_PIPELINE_NAME(name));

	auto desc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout_position)
		.renderTargets(transparentLightPassFormats, arraysize(transparentLightPassFormats), depthStencilFormat)
		//.additiveBlending(0)
		.alphaBlending(0)
		.depthSettings(true, false);

	renderPipeline = createReloadablePipeline(desc, { VERTEX_SHADER_NAME(name), PIXEL_SHADER_NAME(name) });

#undef name
}

fire_particle_system::fire_particle_system(uint32 maxNumParticles, float emitRate, const std::string& textureFilename, uint32 cols, uint32 rows)
{
	initialize(maxNumParticles, emitRate, textureFilename, cols, rows);
}

void fire_particle_system::initialize(uint32 maxNumParticles, float emitRate, const std::string& textureFilename, uint32 cols, uint32 rows)
{
	this->emitRate = emitRate;
	auto tex = loadTextureFromFile(textureFilename);

	if (!tex)
	{
		tex = render_resources::whiteTexture;
	}

	particle_system::initializeAsBillboard(sizeof(fire_particle_data), maxNumParticles, sort_mode_back_to_front);

	atlas.texture = tex;
	atlas.cols = cols;
	atlas.rows = rows;

	atlasCB.initialize(rows, cols);

	settings.sizeOverLifetime.values[0] = 0.25f;
	settings.sizeOverLifetime.values[1] = 0.7f;

	settings.intensityOverLifetime.ts[0] = 0.f;
	settings.intensityOverLifetime.ts[1] = 0.1f;
	settings.intensityOverLifetime.ts[2] = 0.25f;
	settings.intensityOverLifetime.ts[3] = 1.f;
	settings.intensityOverLifetime.values[0] = 0.f;
	settings.intensityOverLifetime.values[1] = 0.8f;
	settings.intensityOverLifetime.values[2] = 0.9f;
	settings.intensityOverLifetime.values[3] = 1.f;
}

void fire_particle_system::update(struct dx_command_list* cl, const common_particle_simulation_data& common, float dt)
{
	struct setter : public particle_parameter_setter
	{
		fire_simulation_cb cb;

		virtual void setRootParameters(dx_command_list* cl) override
		{
			cl->setCompute32BitConstants(FIRE_PARTICLE_SYSTEM_COMPUTE_RS_CBV, cb);
		}
	};

	setter s;
	s.cb.cameraPosition = common.cameraPosition;
	s.cb.emitPosition = vec3(0.f, 20.f, -10.f); // TEMPORARY.
	s.cb.frameIndex = (uint32)dxContext.frameID;

	updateInternal(cl, emitRate * dt, dt, emitPipeline, simulatePipeline, &s);
}

void fire_particle_system::render(transparent_render_pass* renderPass)
{
	fire_rendering_cb cb;
	cb.atlas = atlasCB;
	cb.settings = settings;

	fire_material material;
	material.atlas = atlas;
	material.cbv = dxContext.uploadDynamicConstantBuffer(cb);


	renderPass->renderParticles<fire_pipeline>(billboardMesh.vertexBuffer, billboardMesh.indexBuffer,
		getDrawInfo(renderPipeline),
		material);
}

PIPELINE_SETUP_IMPL(fire_particle_system::fire_pipeline)
{
	cl->setPipelineState(*renderPipeline.pipeline);
	cl->setGraphicsRootSignature(*renderPipeline.rootSignature);
	cl->setGraphicsDynamicConstantBuffer(FIRE_PARTICLE_SYSTEM_RENDERING_RS_CAMERA, common.cameraCBV);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

PARTICLE_PIPELINE_RENDER_IMPL(fire_particle_system::fire_pipeline, fire_particle_system::fire_material)
{
	cl->setGraphicsDynamicConstantBuffer(FIRE_PARTICLE_SYSTEM_RENDERING_RS_CBV, rc.data.cbv);
	cl->setDescriptorHeapSRV(FIRE_PARTICLE_SYSTEM_RENDERING_RS_TEXTURE, 0, rc.data.atlas.texture);

	particle_render_pipeline::render(cl, viewProj, rc);
}




