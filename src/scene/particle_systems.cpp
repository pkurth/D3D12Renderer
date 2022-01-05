#include "pch.h"
#include "particle_systems.h"
#include "dx/dx_pipeline.h"
#include "rendering/render_resources.h"
#include "rendering/render_utils.h"
#include "animation/skinning.h"
#include "core/log.h"


#define BUILD_PARTICLE_SHADER_NAME(name, suffix) name##suffix

#define EMIT_PIPELINE_NAME(name) BUILD_PARTICLE_SHADER_NAME(name, "_emit_cs")
#define SIMULATE_PIPELINE_NAME(name) BUILD_PARTICLE_SHADER_NAME(name, "_sim_cs")
#define VERTEX_SHADER_NAME(name) BUILD_PARTICLE_SHADER_NAME(name, "_vs")
#define PIXEL_SHADER_NAME(name) BUILD_PARTICLE_SHADER_NAME(name, "_ps")





// ----------------------------------------
// FIRE
// ----------------------------------------


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

void fire_particle_system::initialize(uint32 maxNumParticles, float emitRate, const std::string& textureFilename, uint32 cols, uint32 rows)
{
	auto tex = loadTextureFromFile(textureFilename);

	if (!tex)
	{
		tex = render_resources::whiteTexture;
	}
	
	particle_system::initializeAsBillboard(sizeof(fire_particle_data), maxNumParticles, emitRate, sort_mode_back_to_front);

	material.atlas.texture = tex;
	material.atlas.cols = cols;
	material.atlas.rows = rows;

	settings.atlas.initialize(rows, cols);

	settings.emitPosition = vec3(0.f, 20.f, -10.f); // TEMPORARY.

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

void fire_particle_system::update(float dt)
{
	settings.frameIndex = (uint32)dxContext.frameID;

	material.settingsCBV = dxContext.uploadDynamicConstantBuffer(settings);

	particle_system::update(dt, emitPipeline, simulatePipeline);
}

void fire_particle_system::setSimulationParameters(dx_command_list* cl)
{
	cl->setComputeDynamicConstantBuffer(FIRE_PARTICLE_SYSTEM_COMPUTE_RS_CBV, material.settingsCBV);
}

void fire_particle_system::render(transparent_render_pass* renderPass)
{
	renderPass->renderParticles<fire_pipeline>(billboardMesh.vertexBuffer, billboardMesh.indexBuffer,
		getDrawInfo(renderPipeline),
		material);
}

PIPELINE_SETUP_IMPL(fire_particle_system::fire_pipeline)
{
	cl->setPipelineState(*renderPipeline.pipeline);
	cl->setGraphicsRootSignature(*renderPipeline.rootSignature);
	cl->setGraphicsDynamicConstantBuffer(FIRE_PARTICLE_SYSTEM_RENDERING_RS_CAMERA, materialInfo.cameraCBV);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

PARTICLE_PIPELINE_RENDER_IMPL(fire_particle_system::fire_pipeline)
{
	cl->setGraphicsDynamicConstantBuffer(FIRE_PARTICLE_SYSTEM_RENDERING_RS_CBV, rc.material.settingsCBV);
	cl->setDescriptorHeapSRV(FIRE_PARTICLE_SYSTEM_RENDERING_RS_TEXTURE, 0, rc.material.atlas.texture);

	particle_render_pipeline::render(cl, viewProj, rc);
}




// ----------------------------------------
// SMOKE
// ----------------------------------------


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

void smoke_particle_system::initialize(uint32 maxNumParticles, float emitRate, const std::string& textureFilename, uint32 cols, uint32 rows)
{
	auto tex = loadTextureFromFile(textureFilename);

	if (!tex)
	{
		tex = render_resources::whiteTexture;
	}

	particle_system::initializeAsBillboard(sizeof(smoke_particle_data), maxNumParticles, emitRate, sort_mode_back_to_front);

	material.atlas.texture = tex;
	material.atlas.cols = cols;
	material.atlas.rows = rows;

	settings.emitPosition = vec3(20.f, 20.f, -10.f); // TEMPORARY.
	settings.atlas.initialize(rows, cols);
}

void smoke_particle_system::update(float dt)
{
	settings.frameIndex = (uint32)dxContext.frameID;

	material.settingsCBV = dxContext.uploadDynamicConstantBuffer(settings);

	particle_system::update(dt, emitPipeline, simulatePipeline);
}

void smoke_particle_system::setSimulationParameters(dx_command_list* cl)
{
	cl->setComputeDynamicConstantBuffer(SMOKE_PARTICLE_SYSTEM_COMPUTE_RS_CBV, material.settingsCBV);
}

void smoke_particle_system::render(transparent_render_pass* renderPass)
{
	renderPass->renderParticles<smoke_pipeline>(billboardMesh.vertexBuffer, billboardMesh.indexBuffer,
		getDrawInfo(renderPipeline),
		material);
}

PIPELINE_SETUP_IMPL(smoke_particle_system::smoke_pipeline)
{
	cl->setPipelineState(*renderPipeline.pipeline);
	cl->setGraphicsRootSignature(*renderPipeline.rootSignature);
	cl->setGraphicsDynamicConstantBuffer(SMOKE_PARTICLE_SYSTEM_RENDERING_RS_CAMERA, materialInfo.cameraCBV);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

PARTICLE_PIPELINE_RENDER_IMPL(smoke_particle_system::smoke_pipeline)
{
	cl->setGraphicsDynamicConstantBuffer(SMOKE_PARTICLE_SYSTEM_RENDERING_RS_CBV, rc.material.settingsCBV);
	cl->setDescriptorHeapSRV(SMOKE_PARTICLE_SYSTEM_RENDERING_RS_TEXTURE, 0, rc.material.atlas.texture);

	particle_render_pipeline::render(cl, viewProj, rc);
}






// ----------------------------------------
// BOID
// ----------------------------------------


dx_pipeline boid_particle_system::emitPipeline;
dx_pipeline boid_particle_system::simulatePipeline;
dx_pipeline boid_particle_system::renderPipeline;


void boid_particle_system::initializePipeline()
{
#define name "boid_particle_system"

	emitPipeline = createReloadablePipeline(EMIT_PIPELINE_NAME(name));
	simulatePipeline = createReloadablePipeline(SIMULATE_PIPELINE_NAME(name));

	auto desc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout_position_uv_normal_tangent)
		.renderTargets(transparentLightPassFormats, arraysize(transparentLightPassFormats), depthStencilFormat)
		.depthSettings(true, true);

	renderPipeline = createReloadablePipeline(desc, { VERTEX_SHADER_NAME(name), PIXEL_SHADER_NAME(name) });

#undef name
}

void boid_particle_system::initialize(uint32 maxNumParticles, float emitRate)
{
	cartoonMesh = loadAnimatedMeshFromFile("assets/cartoon/cartoon.fbx");
	if (cartoonMesh)
	{
		particle_system::initializeAsMesh(sizeof(boid_particle_data), cartoonMesh->mesh, cartoonMesh->submeshes[0].info, maxNumParticles, emitRate, sort_mode_none);

		settings.emitPosition = vec3(-30.f, 20.f, -10.f); // TEMPORARY.
		settings.radius = 15.f;
	}
	else
	{
		LOG_ERROR("Cannot create particle system, since mesh was not found");
		std::cerr << "Cannot create particle system, since mesh was not found\n";
	}
}

void boid_particle_system::update(float dt)
{
	if (cartoonMesh)
	{
		const dx_mesh& mesh = cartoonMesh->mesh;
		animation_skeleton& skeleton = cartoonMesh->skeleton;

		time += dt;
		time = fmod(time, skeleton.clips[0].lengthInSeconds);

		auto [skinnedVertexBuffer, skinningMatrices] = skinObject(mesh.vertexBuffer, cartoonMesh->submeshes[0].info, (uint32)skeleton.joints.size());

		trs localTransforms[128];
		skeleton.sampleAnimation(0, time, localTransforms);
		skeleton.getSkinningMatricesFromLocalTransforms(localTransforms, skinningMatrices);

		this->skinnedVertexBuffer = skinnedVertexBuffer;
		this->submesh.baseVertex = 0;

		settings.frameIndex = (uint32)dxContext.frameID;

		material.settingsCBV = dxContext.uploadDynamicConstantBuffer(settings);

		particle_system::update(dt, emitPipeline, simulatePipeline);
	}
}

void boid_particle_system::setSimulationParameters(dx_command_list* cl)
{
	cl->setComputeDynamicConstantBuffer(BOID_PARTICLE_SYSTEM_COMPUTE_RS_CBV, material.settingsCBV);
}

void boid_particle_system::render(transparent_render_pass* renderPass)
{
	if (cartoonMesh)
	{
		renderPass->renderParticles<boid_pipeline>(skinnedVertexBuffer, cartoonMesh->mesh.indexBuffer,
			getDrawInfo(renderPipeline),
			material);
	}
}

PIPELINE_SETUP_IMPL(boid_particle_system::boid_pipeline)
{
	cl->setPipelineState(*renderPipeline.pipeline);
	cl->setGraphicsRootSignature(*renderPipeline.rootSignature);
	cl->setGraphicsDynamicConstantBuffer(BOID_PARTICLE_SYSTEM_RENDERING_RS_CAMERA, materialInfo.cameraCBV);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cl->setDescriptorHeapSRV(BOID_PARTICLE_SYSTEM_RENDERING_RS_PBR, 0, materialInfo.irradiance);
	cl->setDescriptorHeapSRV(BOID_PARTICLE_SYSTEM_RENDERING_RS_PBR, 1, materialInfo.environment);
	cl->setDescriptorHeapSRV(BOID_PARTICLE_SYSTEM_RENDERING_RS_PBR, 2, render_resources::brdfTex);
}

PARTICLE_PIPELINE_RENDER_IMPL(boid_particle_system::boid_pipeline)
{
	cl->setGraphicsDynamicConstantBuffer(BOID_PARTICLE_SYSTEM_RENDERING_RS_CBV, rc.material.settingsCBV);

	particle_render_pipeline::render(cl, viewProj, rc);
}


















// ----------------------------------------
// COMMON
// ----------------------------------------


void loadAllParticleSystemPipelines()
{
	fire_particle_system::initializePipeline();
	smoke_particle_system::initializePipeline();
	boid_particle_system::initializePipeline();
}
