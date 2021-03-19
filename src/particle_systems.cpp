#include "pch.h"
#include "particle_systems.h"
#include "dx_pipeline.h"
#include "dx_renderer.h"


#define BUILD_PARTICLE_SHADER_NAME(name, suffix) name##suffix

#define EMIT_PIPELINE_NAME(name) BUILD_PARTICLE_SHADER_NAME(name, "_emit_cs")
#define SIMULATE_PIPELINE_NAME(name) BUILD_PARTICLE_SHADER_NAME(name, "_sim_cs")
#define VERTEX_SHADER_NAME(name) BUILD_PARTICLE_SHADER_NAME(name, "_vs")
#define PIXEL_SHADER_NAME(name) BUILD_PARTICLE_SHADER_NAME(name, "_ps")




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
		.renderTargets(dx_renderer::transparentLightPassFormats, arraysize(dx_renderer::transparentLightPassFormats), dx_renderer::hdrDepthStencilFormat)
		.additiveBlending(0)
		.depthSettings(true, false);

	renderPipeline = createReloadablePipeline(desc, { VERTEX_SHADER_NAME(name), PIXEL_SHADER_NAME(name) });

#undef name
}

void fire_particle_system::initialize(uint32 maxNumParticles, float emitRate, const std::string& textureFilename, uint32 cols, uint32 rows)
{
	particle_system::initializeAsBillboard(sizeof(fire_particle_data), maxNumParticles, emitRate);

	material = make_ref<fire_material>();
	material->atlas.texture = loadTextureFromFile(textureFilename);
	material->atlas.cols = cols;
	material->atlas.rows = rows;

	settings.emitPosition = vec3(0.f, 20.f, -10.f); // TEMPORARY.
	settings.atlas.initialize(rows, cols);
}

void fire_particle_system::update(float dt)
{
	settings.frameIndex = (uint32)dxContext.frameID;

	material->settingsCBV = dxContext.uploadDynamicConstantBuffer(settings);

	particle_system::update(dt, emitPipeline, simulatePipeline);
}

void fire_particle_system::setSimulationParameters(dx_command_list* cl)
{
	cl->setComputeDynamicConstantBuffer(FIRE_PARTICLE_SYSTEM_COMPUTE_RS_CBV, material->settingsCBV);
}

void fire_particle_system::render(transparent_render_pass* renderPass)
{
	renderPass->renderParticles(billboardMesh.vertexBuffer, billboardMesh.indexBuffer,
		getDrawInfo(renderPipeline),
		material);
}

void fire_particle_system::fire_material::setupTransparentPipeline(dx_command_list* cl, const common_material_info& materialInfo)
{
	cl->setPipelineState(*fire_particle_system::renderPipeline.pipeline);
	cl->setGraphicsRootSignature(*fire_particle_system::renderPipeline.rootSignature);
	cl->setGraphicsDynamicConstantBuffer(FIRE_PARTICLE_SYSTEM_RENDERING_RS_CAMERA, materialInfo.cameraCBV);
}

void fire_particle_system::fire_material::prepareForRendering(dx_command_list* cl)
{
	cl->setGraphicsDynamicConstantBuffer(FIRE_PARTICLE_SYSTEM_RENDERING_RS_CBV, settingsCBV);
	cl->setDescriptorHeapSRV(FIRE_PARTICLE_SYSTEM_RENDERING_RS_TEXTURE, 0, atlas.texture);
}

void loadAllParticleSystemPipelines()
{
	fire_particle_system::initializePipeline();
}
