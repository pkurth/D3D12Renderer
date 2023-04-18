#include "pch.h"
#include "smoke_particle_system.h"
#include "dx/dx_pipeline.h"
#include "rendering/render_resources.h"
#include "rendering/render_utils.h"


dx_pipeline smoke_particle_system::emitPipeline;
dx_pipeline smoke_particle_system::simulatePipeline;
dx_pipeline smoke_particle_system::renderPipeline;


void smoke_particle_system::initializePipeline()
{
#define name "smoke_particle_system"

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

smoke_particle_system::smoke_particle_system(uint32 maxNumParticles, float emitRate, const std::string& textureFilename, uint32 cols, uint32 rows)
{
	initialize(maxNumParticles, emitRate, textureFilename, cols, rows);
}

void smoke_particle_system::initialize(uint32 maxNumParticles, float emitRate, const std::string& textureFilename, uint32 cols, uint32 rows)
{
	this->emitRate = emitRate;
	auto tex = loadTextureFromFileAsync(textureFilename);

	if (!tex)
	{
		tex = render_resources::whiteTexture;
	}

	particle_system::initializeAsBillboard(sizeof(smoke_particle_data), maxNumParticles, sort_mode_back_to_front);

	atlas.texture = tex;
	atlas.cols = cols;
	atlas.rows = rows;

	atlasCB.initialize(rows, cols);
}

void smoke_particle_system::update(struct dx_command_list* cl, const common_particle_simulation_data& common, float dt)
{
	struct setter : public particle_parameter_setter
	{
		dx_dynamic_constant_buffer cb;

		virtual void setRootParameters(dx_command_list* cl) override
		{
			cl->setComputeDynamicConstantBuffer(SMOKE_PARTICLE_SYSTEM_COMPUTE_RS_CBV, cb);
		}
	};

	smoke_simulation_cb cb;
	cb.cameraPosition = common.cameraPosition;
	cb.emitPosition = vec3(20.f, 20.f, -10.f); // TEMPORARY.
	cb.frameIndex = (uint32)dxContext.frameID;
	cb.lifeScaleFromDistance = settings.lifeScaleFromDistance;

	setter s;
	s.cb = dxContext.uploadDynamicConstantBuffer(cb);

	updateInternal(cl, emitRate * dt, dt, emitPipeline, simulatePipeline, &s);
}

void smoke_particle_system::render(transparent_render_pass* renderPass)
{
	smoke_rendering_cb cb;
	cb.atlas = atlasCB;
	cb.intensityOverLifetime = settings.intensityOverLifetime;
	cb.atlasProgressionOverLifetime = settings.atlasProgressionOverLifetime;

	smoke_material material;
	material.atlas = atlas;
	material.cbv = dxContext.uploadDynamicConstantBuffer(cb);

	renderPass->renderParticles<smoke_pipeline>(billboardMesh.vertexBuffer, billboardMesh.indexBuffer,
		getDrawInfo(renderPipeline),
		material);
}

PIPELINE_SETUP_IMPL(smoke_particle_system::smoke_pipeline)
{
	cl->setPipelineState(*renderPipeline.pipeline);
	cl->setGraphicsRootSignature(*renderPipeline.rootSignature);
	cl->setGraphicsDynamicConstantBuffer(SMOKE_PARTICLE_SYSTEM_RENDERING_RS_CAMERA, common.cameraCBV);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

PARTICLE_PIPELINE_RENDER_IMPL(smoke_particle_system::smoke_pipeline, smoke_particle_system::smoke_material)
{
	cl->setGraphicsDynamicConstantBuffer(SMOKE_PARTICLE_SYSTEM_RENDERING_RS_CBV, rc.data.cbv);
	cl->setDescriptorHeapSRV(SMOKE_PARTICLE_SYSTEM_RENDERING_RS_TEXTURE, 0, rc.data.atlas.texture);

	particle_render_pipeline::render(cl, viewProj, rc);
}

