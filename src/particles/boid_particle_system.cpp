#include "pch.h"
#include "boid_particle_system.h"
#include "dx/dx_pipeline.h"
#include "rendering/render_resources.h"
#include "rendering/render_utils.h"
#include "animation/skinning.h"
#include "core/log.h"


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

boid_particle_system::boid_particle_system(uint32 maxNumParticles, float emitRate)
{
	initialize(maxNumParticles, emitRate);
}

void boid_particle_system::initialize(uint32 maxNumParticles, float emitRate)
{
	this->emitRate = emitRate;
	cartoonMesh = loadAnimatedMeshFromFile("assets/cartoon/cartoon.fbx");
	if (cartoonMesh)
	{
		particle_system::initializeAsMesh(sizeof(boid_particle_data), cartoonMesh->mesh, cartoonMesh->submeshes[0].info, maxNumParticles, sort_mode_none);

		settings.radius = 15.f;
	}
	else
	{
		LOG_ERROR("Cannot create particle system, since mesh was not found");
		std::cerr << "Cannot create particle system, since mesh was not found\n";
	}
}

void boid_particle_system::update(struct dx_command_list* cl, const common_particle_simulation_data& common, float dt)
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


		struct setter : public particle_parameter_setter
		{
			boid_simulation_cb cb;

			virtual void setRootParameters(dx_command_list* cl) override
			{
				cl->setCompute32BitConstants(BOID_PARTICLE_SYSTEM_COMPUTE_RS_CBV, cb);
			}
		};

		setter s;
		s.cb.emitPosition = vec3(-30.f, 20.f, -10.f); // TEMPORARY.
		s.cb.frameIndex = (uint32)dxContext.frameID;
		s.cb.radius = settings.radius;

		updateInternal(cl, emitRate * dt, dt, emitPipeline, simulatePipeline, &s);
	}
}

void boid_particle_system::render(transparent_render_pass* renderPass)
{
	if (cartoonMesh)
	{
		boid_material material;

		renderPass->renderParticles<boid_pipeline>(skinnedVertexBuffer, cartoonMesh->mesh.indexBuffer,
			getDrawInfo(renderPipeline),
			material);
	}
}

PIPELINE_SETUP_IMPL(boid_particle_system::boid_pipeline)
{
	cl->setPipelineState(*renderPipeline.pipeline);
	cl->setGraphicsRootSignature(*renderPipeline.rootSignature);
	cl->setGraphicsDynamicConstantBuffer(BOID_PARTICLE_SYSTEM_RENDERING_RS_CAMERA, common.cameraCBV);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cl->setDescriptorHeapSRV(BOID_PARTICLE_SYSTEM_RENDERING_RS_PBR, 0, common.irradiance);
	cl->setDescriptorHeapSRV(BOID_PARTICLE_SYSTEM_RENDERING_RS_PBR, 1, common.prefilteredRadiance);
	cl->setDescriptorHeapSRV(BOID_PARTICLE_SYSTEM_RENDERING_RS_PBR, 2, render_resources::brdfTex);
}

PARTICLE_PIPELINE_RENDER_IMPL(boid_particle_system::boid_pipeline, boid_particle_system::boid_material)
{
	particle_render_pipeline::render(cl, viewProj, rc);
}


