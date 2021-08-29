#include "pch.h"
#include "render_utils.h"
#include "core/random.h"
#include "dx/dx_context.h"
#include "animation/skinning.h"
#include "texture_preprocessing.h"
#include "render_resources.h"
#include "render_algorithms.h"
#include "pbr.h"
#include "scene/particle_systems.h"
#include "bitonic_sort.h"
#include "debug_visualization.h"

#include "particles_rs.hlsli"


static vec2 haltonSequence[128];
static uint64 skinningFence;


dx_pipeline depthOnlyPipeline;
dx_pipeline animatedDepthOnlyPipeline;
dx_pipeline shadowPipeline;
dx_pipeline pointLightShadowPipeline;

dx_pipeline textureSkyPipeline;
dx_pipeline proceduralSkyPipeline;
dx_pipeline preethamSkyPipeline;
dx_pipeline sphericalHarmonicsSkyPipeline;

dx_pipeline outlineMarkerPipeline;
dx_pipeline outlineDrawerPipeline;

dx_command_signature particleCommandSignature;




void initializeRenderUtils()
{

	// Sky.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(skyPassFormats, arraysize(skyPassFormats), depthStencilFormat)
			.depthSettings(true, false)
			.cullFrontFaces();

		textureSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_texture_ps" });
		proceduralSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_procedural_ps" });
		preethamSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_preetham_ps" });
		sphericalHarmonicsSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_sh_ps" });
	}

	// Depth prepass.
	{
		DXGI_FORMAT depthOnlyFormat[] = { screenVelocitiesFormat, objectIDsFormat };

		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(depthOnlyFormat, arraysize(depthOnlyFormat), depthStencilFormat)
			.inputLayout(inputLayout_position);

		depthOnlyPipeline = createReloadablePipeline(desc, { "depth_only_vs", "depth_only_ps" }, rs_in_vertex_shader);
		animatedDepthOnlyPipeline = createReloadablePipeline(desc, { "depth_only_animated_vs", "depth_only_ps" }, rs_in_vertex_shader);
	}

	// Shadow.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(0, 0, render_resources::shadowDepthFormat)
			.inputLayout(inputLayout_position)
			//.cullFrontFaces()
			;

		shadowPipeline = createReloadablePipeline(desc, { "shadow_vs" }, rs_in_vertex_shader);
		pointLightShadowPipeline = createReloadablePipeline(desc, { "shadow_point_light_vs", "shadow_point_light_ps" }, rs_in_vertex_shader);
	}

	// Outline.
	{
		auto markerDesc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position)
			.renderTargets(0, 0, depthStencilFormat)
			.stencilSettings(D3D12_COMPARISON_FUNC_ALWAYS,
				D3D12_STENCIL_OP_REPLACE,
				D3D12_STENCIL_OP_REPLACE,
				D3D12_STENCIL_OP_KEEP,
				D3D12_DEFAULT_STENCIL_READ_MASK,
				stencil_flag_selected_object) // Mark selected object.
			.depthSettings(false, false);

		outlineMarkerPipeline = createReloadablePipeline(markerDesc, { "outline_vs" }, rs_in_vertex_shader);


		auto drawerDesc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(ldrFormat, depthStencilFormat)
			.stencilSettings(D3D12_COMPARISON_FUNC_EQUAL,
				D3D12_STENCIL_OP_KEEP,
				D3D12_STENCIL_OP_KEEP,
				D3D12_STENCIL_OP_KEEP,
				stencil_flag_selected_object, // Read only selected object bit.
				0)
			.depthSettings(false, false);

		outlineDrawerPipeline = createReloadablePipeline(drawerDesc, { "fullscreen_triangle_vs", "outline_ps" });
	}


	D3D12_INDIRECT_ARGUMENT_DESC argumentDesc;
	argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
	particleCommandSignature = createCommandSignature({}, &argumentDesc, 1, sizeof(particle_draw));





	initializeTexturePreprocessing();
	initializeSkinning();
	loadCommonShaders();

	debug_simple_pipeline::initialize();
	debug_unlit_pipeline::initialize();
	opaque_pbr_pipeline::initialize();
	transparent_pbr_pipeline::initialize();
	particle_system::initializePipeline();
	initializeBitonicSort();
	loadAllParticleSystemPipelines();


	createAllPendingReloadablePipelines();
	render_resources::initializeGlobalResources();

	for (uint32 i = 0; i < arraysize(haltonSequence); ++i)
	{
		haltonSequence[i] = halton23(i) * 2.f - vec2(1.f);
	}
}

void endFrameCommon()
{
	checkForChangedPipelines();
	skinningFence = performSkinning();
}

void buildCameraConstantBuffer(const render_camera& camera, float cameraJitterStrength, camera_cb& outCB)
{
	if (cameraJitterStrength > 0.f)
	{
		vec2 jitter = haltonSequence[dxContext.frameID % arraysize(haltonSequence)] / vec2((float)camera.width, (float)camera.height) * cameraJitterStrength;
		buildCameraConstantBuffer(camera.getJitteredVersion(jitter), jitter, outCB);
	}
	else
	{
		buildCameraConstantBuffer(camera, vec2(0.f, 0.f), outCB);
	}
}

void buildCameraConstantBuffer(const render_camera& camera, vec2 jitter, camera_cb& outCB)
{
	outCB.prevFrameViewProj = outCB.viewProj;
	outCB.viewProj = camera.viewProj;
	outCB.view = camera.view;
	outCB.proj = camera.proj;
	outCB.invViewProj = camera.invViewProj;
	outCB.invView = camera.invView;
	outCB.invProj = camera.invProj;
	outCB.position = vec4(camera.position, 1.f);
	outCB.forward = vec4(camera.rotation * vec3(0.f, 0.f, -1.f), 0.f);
	outCB.right = vec4(camera.rotation * vec3(1.f, 0.f, 0.f), 0.f);
	outCB.up = vec4(camera.rotation * vec3(0.f, 1.f, 0.f), 0.f);
	outCB.projectionParams = vec4(camera.nearPlane, camera.farPlane, camera.farPlane / camera.nearPlane, 1.f - camera.farPlane / camera.nearPlane);
	outCB.screenDims = vec2((float)camera.width, (float)camera.height);
	outCB.invScreenDims = vec2(1.f / camera.width, 1.f / camera.height);
	outCB.prevFrameJitter = outCB.jitter;
	outCB.jitter = jitter;
}

void waitForSkinningToFinish()
{
	if (skinningFence)
	{
		dxContext.renderQueue.waitForOtherQueue(dxContext.computeQueue); // Wait for GPU skinning.
	}
}
