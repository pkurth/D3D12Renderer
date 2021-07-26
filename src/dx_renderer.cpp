#include "pch.h"
#include "dx_renderer.h"
#include "dx_command_list.h"
#include "dx_render_target.h"
#include "dx_pipeline.h"
#include "dx_bitonic_sort.h"
#include "geometry.h"
#include "dx_texture.h"
#include "dx_barrier_batcher.h"
#include "texture_preprocessing.h"
#include "skinning.h"
#include "dx_context.h"
#include "dx_profiling.h"
#include "random.h"
#include "particle_systems.h"
#include "render_resources.h"

#include "depth_only_rs.hlsli"
#include "outline_rs.hlsli"
#include "sky_rs.hlsli"
#include "camera.hlsli"
#include "transform.hlsli"
#include "particles_rs.hlsli"
#include "random.h"

#include "raytracing.h"


static dx_pipeline depthOnlyPipeline;
static dx_pipeline animatedDepthOnlyPipeline;
static dx_pipeline shadowPipeline;
static dx_pipeline pointLightShadowPipeline;

static dx_pipeline textureSkyPipeline;
static dx_pipeline proceduralSkyPipeline;

static dx_pipeline outlineMarkerPipeline;
static dx_pipeline outlineDrawerPipeline;




static dx_command_signature particleCommandSignature;


DXGI_FORMAT dx_renderer::outputFormat;


static bool performedSkinning;

static vec2 haltonSequence[128];


enum stencil_flags
{
	stencil_flag_selected_object = (1 << 0),
};


#define SSR_RAYCAST_WIDTH (renderWidth / 2)
#define SSR_RAYCAST_HEIGHT (renderHeight / 2)

#define SSR_RESOLVE_WIDTH (renderWidth / 2)
#define SSR_RESOLVE_HEIGHT (renderHeight / 2)


void dx_renderer::initializeCommon(DXGI_FORMAT outputFormat)
{
	dx_renderer::outputFormat = outputFormat;


	initializeTexturePreprocessing();
	initializeSkinning();



	// Sky.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(skyPassFormats, arraysize(skyPassFormats), hdrDepthStencilFormat)
			.depthSettings(true, false)
			.cullFrontFaces();

		proceduralSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_procedural_ps" });
		textureSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_texture_ps" });
	}

	// Depth prepass.
	{
		DXGI_FORMAT depthOnlyFormat[] = { screenVelocitiesFormat, objectIDsFormat };

		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(depthOnlyFormat, arraysize(depthOnlyFormat), hdrDepthStencilFormat)
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
			.renderTargets(0, 0, hdrDepthStencilFormat)
			.stencilSettings(D3D12_COMPARISON_FUNC_ALWAYS, 
				D3D12_STENCIL_OP_REPLACE, 
				D3D12_STENCIL_OP_REPLACE, 
				D3D12_STENCIL_OP_KEEP, 
				D3D12_DEFAULT_STENCIL_READ_MASK, 
				stencil_flag_selected_object) // Mark selected object.
			.depthSettings(false, false);

		outlineMarkerPipeline = createReloadablePipeline(markerDesc, { "outline_vs" }, rs_in_vertex_shader);


		auto drawerDesc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(ldrPostProcessFormat, hdrDepthStencilFormat)
			.stencilSettings(D3D12_COMPARISON_FUNC_EQUAL,
				D3D12_STENCIL_OP_KEEP,
				D3D12_STENCIL_OP_KEEP,
				D3D12_STENCIL_OP_KEEP,
				stencil_flag_selected_object, // Read only selected object bit.
				0)
			.depthSettings(false, false);

		outlineDrawerPipeline = createReloadablePipeline(drawerDesc, { "fullscreen_triangle_vs", "outline_ps" });
	}


	loadCommonShaders();

	pbr_material::initializePipeline();
	particle_system::initializePipeline();
	initializeBitonicSort();
	loadAllParticleSystemPipelines();



	D3D12_INDIRECT_ARGUMENT_DESC argumentDesc;
	argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
	particleCommandSignature = createCommandSignature({}, &argumentDesc, 1, sizeof(particle_draw));



	createAllPendingReloadablePipelines();

	render_resources::initializeGlobalResources();

	for (uint32 i = 0; i < arraysize(haltonSequence); ++i)
	{
		haltonSequence[i] = halton23(i) * 2.f - vec2(1.f);
	}
}

void dx_renderer::initialize(uint32 windowWidth, uint32 windowHeight, bool renderObjectIDs)
{
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;

	recalculateViewport(false);

	hdrColorTexture = createTexture(0, renderWidth, renderHeight, hdrFormat, false, true, true, D3D12_RESOURCE_STATE_RENDER_TARGET);

	D3D12_RESOURCE_DESC prevFrameHDRColorDesc = CD3DX12_RESOURCE_DESC::Tex2D(hdrFormat, renderWidth / 2, renderHeight / 2, 1,
		8, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	prevFrameHDRColorTexture = createTexture(prevFrameHDRColorDesc, 0, 0, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	prevFrameHDRColorTempTexture = createTexture(prevFrameHDRColorDesc, 0, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	allocateMipUAVs(prevFrameHDRColorTexture);
	allocateMipUAVs(prevFrameHDRColorTempTexture);

	worldNormalsTexture = createTexture(0, renderWidth, renderHeight, worldNormalsFormat, false, true, false, D3D12_RESOURCE_STATE_RENDER_TARGET);
	screenVelocitiesTexture = createTexture(0, renderWidth, renderHeight, screenVelocitiesFormat, false, true, false, D3D12_RESOURCE_STATE_RENDER_TARGET);
	reflectanceTexture = createTexture(0, renderWidth, renderHeight, reflectanceFormat, false, true, false, D3D12_RESOURCE_STATE_RENDER_TARGET);

	depthStencilBuffer = createDepthTexture(renderWidth, renderHeight, hdrDepthStencilFormat);
	opaqueDepthBuffer = createDepthTexture(renderWidth, renderHeight, hdrDepthStencilFormat, 1, D3D12_RESOURCE_STATE_COPY_DEST);
	D3D12_RESOURCE_DESC linearDepthDesc = CD3DX12_RESOURCE_DESC::Tex2D(linearDepthFormat, renderWidth, renderHeight, 1,
		6, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	linearDepthBuffer = createTexture(linearDepthDesc, 0, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	allocateMipUAVs(linearDepthBuffer);

	ssrRaycastTexture = createTexture(0, SSR_RAYCAST_WIDTH, SSR_RAYCAST_HEIGHT, reflectionFormat, false, false, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	ssrResolveTexture = createTexture(0, SSR_RESOLVE_WIDTH, SSR_RESOLVE_HEIGHT, reflectionFormat, false, false, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	ssrTemporalTextures[ssrHistoryIndex] = createTexture(0, SSR_RESOLVE_WIDTH, SSR_RESOLVE_HEIGHT, reflectionFormat, false, false, true, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	ssrTemporalTextures[1 - ssrHistoryIndex] = createTexture(0, SSR_RESOLVE_WIDTH, SSR_RESOLVE_HEIGHT, reflectionFormat, false, false, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	hdrPostProcessingTexture = createTexture(0, renderWidth, renderHeight, hdrPostProcessFormat, false, true, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	taaTextures[taaHistoryIndex] = createTexture(0, renderWidth, renderHeight, hdrPostProcessFormat, false, true, true, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	taaTextures[1 - taaHistoryIndex] = createTexture(0, renderWidth, renderHeight, hdrPostProcessFormat, false, true, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	ldrPostProcessingTexture = createTexture(0, renderWidth, renderHeight, ldrPostProcessFormat, false, true, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	D3D12_RESOURCE_DESC bloomDesc = CD3DX12_RESOURCE_DESC::Tex2D(hdrPostProcessFormat, renderWidth / 4, renderHeight / 4, 1,
		5, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	bloomTexture = createTexture(bloomDesc, 0, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	bloomTempTexture = createTexture(bloomDesc, 0, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	allocateMipUAVs(bloomTexture);
	allocateMipUAVs(bloomTempTexture);

	frameResult = createTexture(0, windowWidth, windowHeight, outputFormat, false, true, true);



	SET_NAME(hdrColorTexture->resource, "HDR Color");
	SET_NAME(prevFrameHDRColorTexture->resource, "Prev frame HDR Color");
	SET_NAME(prevFrameHDRColorTempTexture->resource, "Prev frame HDR Color Temp");
	SET_NAME(worldNormalsTexture->resource, "World normals");
	SET_NAME(screenVelocitiesTexture->resource, "Screen velocities");
	SET_NAME(reflectanceTexture->resource, "Reflectance");
	SET_NAME(depthStencilBuffer->resource, "Depth buffer");
	SET_NAME(opaqueDepthBuffer->resource, "Opaque depth buffer");
	SET_NAME(linearDepthBuffer->resource, "Linear depth buffer");

	SET_NAME(ssrRaycastTexture->resource, "SSR Raycast");
	SET_NAME(ssrResolveTexture->resource, "SSR Resolve");
	SET_NAME(ssrTemporalTextures[0]->resource, "SSR Temporal 0");
	SET_NAME(ssrTemporalTextures[1]->resource, "SSR Temporal 1");

	SET_NAME(taaTextures[0]->resource, "TAA 0");
	SET_NAME(taaTextures[1]->resource, "TAA 1");

	SET_NAME(hdrPostProcessingTexture->resource, "HDR Post processing");
	SET_NAME(ldrPostProcessingTexture->resource, "LDR Post processing");

	SET_NAME(bloomTexture->resource, "Bloom");
	SET_NAME(bloomTempTexture->resource, "Bloom Temp");

	SET_NAME(frameResult->resource, "Frame result");



	if (renderObjectIDs)
	{
		hoveredObjectIDReadbackBuffer = createReadbackBuffer(getFormatSize(objectIDsFormat), NUM_BUFFERED_FRAMES);

		objectIDsTexture = createTexture(0, renderWidth, renderHeight, objectIDsFormat, false, true, false, D3D12_RESOURCE_STATE_RENDER_TARGET);
		SET_NAME(objectIDsTexture->resource, "Object IDs");
	}
}

void dx_renderer::beginFrameCommon()
{
	checkForChangedPipelines();
}

void dx_renderer::endFrameCommon()
{
	performedSkinning = performSkinning();
}

void dx_renderer::beginFrame(uint32 windowWidth, uint32 windowHeight)
{
	pointLights = 0;
	spotLights = 0;
	numPointLights = 0;
	numSpotLights = 0;
	decals = 0;
	numDecals = 0;
	decalTextureAtlas = 0;

	if (this->windowWidth != windowWidth || this->windowHeight != windowHeight)
	{
		this->windowWidth = windowWidth;
		this->windowHeight = windowHeight;

		// Frame result.
		resizeTexture(frameResult, windowWidth, windowHeight);

		recalculateViewport(true);
	}

	if (objectIDsTexture)
	{
		uint32* id = (uint32*)mapBuffer(hoveredObjectIDReadbackBuffer, true, map_range{ dxContext.bufferedFrameID, 1 });
		hoveredObjectID = *id;
		unmapBuffer(hoveredObjectIDReadbackBuffer, false);
	}

	opaqueRenderPass = 0;
	overlayRenderPass = 0;
	transparentRenderPass = 0;
	sunShadowRenderPass = 0;
	numSpotLightShadowRenderPasses = 0;
	numPointLightShadowRenderPasses = 0;

	pointLights = 0;
	spotLights = 0;
	numPointLights = 0;
	numSpotLights = 0;

	environment = 0;
}

void dx_renderer::recalculateViewport(bool resizeTextures)
{
	if (aspectRatioMode == aspect_ratio_free)
	{
		windowXOffset = 0;
		windowYOffset = 0;
		renderWidth = windowWidth;
		renderHeight = windowHeight;
	}
	else
	{
		const float targetAspect = aspectRatioMode == aspect_ratio_fix_16_9 ? (16.f / 9.f) : (16.f / 10.f);

		float aspect = (float)windowWidth / (float)windowHeight;
		if (aspect > targetAspect)
		{
			renderWidth = (uint32)(windowHeight * targetAspect);
			renderHeight = windowHeight;
			windowXOffset = (windowWidth - renderWidth) / 2;
			windowYOffset = 0;
		}
		else
		{
			renderWidth = windowWidth;
			renderHeight = (uint32)(windowWidth / targetAspect);
			windowXOffset = 0;
			windowYOffset = (windowHeight - renderHeight) / 2;
		}
	}


	if (resizeTextures)
	{
		resizeTexture(hdrColorTexture, renderWidth, renderHeight);
		resizeTexture(prevFrameHDRColorTexture, renderWidth / 2, renderHeight / 2);
		resizeTexture(prevFrameHDRColorTempTexture, renderWidth / 2, renderHeight / 2);
		resizeTexture(worldNormalsTexture, renderWidth, renderHeight);
		resizeTexture(screenVelocitiesTexture, renderWidth, renderHeight);
		resizeTexture(reflectanceTexture, renderWidth, renderHeight);
		resizeTexture(depthStencilBuffer, renderWidth, renderHeight);
		resizeTexture(opaqueDepthBuffer, renderWidth, renderHeight);
		resizeTexture(linearDepthBuffer, renderWidth, renderHeight);

		if (objectIDsTexture)
		{
			resizeTexture(objectIDsTexture, renderWidth, renderHeight);
		}

		resizeTexture(ssrRaycastTexture, SSR_RAYCAST_WIDTH, SSR_RAYCAST_HEIGHT);
		resizeTexture(ssrResolveTexture, SSR_RESOLVE_WIDTH, SSR_RESOLVE_HEIGHT);
		resizeTexture(ssrTemporalTextures[ssrHistoryIndex], SSR_RESOLVE_WIDTH, SSR_RESOLVE_HEIGHT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		resizeTexture(ssrTemporalTextures[1 - ssrHistoryIndex], SSR_RESOLVE_WIDTH, SSR_RESOLVE_HEIGHT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		resizeTexture(hdrPostProcessingTexture, renderWidth, renderHeight);

		resizeTexture(taaTextures[taaHistoryIndex], renderWidth, renderHeight, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		resizeTexture(taaTextures[1 - taaHistoryIndex], renderWidth, renderHeight, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		resizeTexture(bloomTexture, renderWidth / 4, renderHeight / 4);
		resizeTexture(bloomTempTexture, renderWidth / 4, renderHeight / 4);

		resizeTexture(ldrPostProcessingTexture, renderWidth, renderHeight);
	}
	
	culling.allocateIfNecessary(renderWidth, renderHeight);
}

void dx_renderer::setCamera(const render_camera& camera)
{
	vec2 jitterOffset(0.f, 0.f);
	render_camera c;
	if (enableTAA)
	{
		jitterOffset = haltonSequence[dxContext.frameID % arraysize(haltonSequence)] / vec2((float)renderWidth, (float)renderHeight) * taaSettings.cameraJitterStrength;
		c = camera.getJitteredVersion(jitterOffset);
	}
	else
	{
		c = camera;
	}

	this->jitteredCamera.prevFrameViewProj = this->jitteredCamera.viewProj;
	this->jitteredCamera.viewProj = c.viewProj;
	this->jitteredCamera.view = c.view;
	this->jitteredCamera.proj = c.proj;
	this->jitteredCamera.invViewProj = c.invViewProj;
	this->jitteredCamera.invView = c.invView;
	this->jitteredCamera.invProj = c.invProj;
	this->jitteredCamera.position = vec4(c.position, 1.f);
	this->jitteredCamera.forward = vec4(c.rotation * vec3(0.f, 0.f, -1.f), 0.f);
	this->jitteredCamera.right = vec4(c.rotation * vec3(1.f, 0.f, 0.f), 0.f);
	this->jitteredCamera.up = vec4(c.rotation * vec3(0.f, 1.f, 0.f), 0.f);
	this->jitteredCamera.projectionParams = vec4(c.nearPlane, c.farPlane, c.farPlane / c.nearPlane, 1.f - c.farPlane / c.nearPlane);
	this->jitteredCamera.screenDims = vec2((float)renderWidth, (float)renderHeight);
	this->jitteredCamera.invScreenDims = vec2(1.f / renderWidth, 1.f / renderHeight);
	this->jitteredCamera.prevFrameJitter = this->jitteredCamera.jitter;
	this->jitteredCamera.jitter = jitterOffset;


	this->unjitteredCamera.prevFrameViewProj = this->unjitteredCamera.viewProj;
	this->unjitteredCamera.viewProj = camera.viewProj;
	this->unjitteredCamera.view = camera.view;
	this->unjitteredCamera.proj = camera.proj;
	this->unjitteredCamera.invViewProj = camera.invViewProj;
	this->unjitteredCamera.invView = camera.invView;
	this->unjitteredCamera.invProj = camera.invProj;
	this->unjitteredCamera.position = vec4(camera.position, 1.f);
	this->unjitteredCamera.forward = vec4(camera.rotation * vec3(0.f, 0.f, -1.f), 0.f);
	this->unjitteredCamera.right = vec4(camera.rotation * vec3(1.f, 0.f, 0.f), 0.f);
	this->unjitteredCamera.up = vec4(camera.rotation * vec3(0.f, 1.f, 0.f), 0.f);
	this->unjitteredCamera.projectionParams = vec4(camera.nearPlane, camera.farPlane, camera.farPlane / camera.nearPlane, 1.f - camera.farPlane / camera.nearPlane);
	this->unjitteredCamera.screenDims = vec2((float)renderWidth, (float)renderHeight);
	this->unjitteredCamera.invScreenDims = vec2(1.f / renderWidth, 1.f / renderHeight);
	this->unjitteredCamera.prevFrameJitter = vec2(0.f);
	this->unjitteredCamera.jitter = vec2(0.f);
}

void dx_renderer::setEnvironment(const ref<pbr_environment>& environment)
{
	this->environment = environment;
}

void dx_renderer::setSun(const directional_light& light)
{
	sun.cascadeDistances = light.cascadeDistances;
	sun.bias = light.bias;
	sun.direction = light.direction;
	sun.blendDistances = light.blendDistances;
	sun.radiance = light.color * light.intensity;
	sun.numShadowCascades = light.numShadowCascades;

	memcpy(sun.vp, light.vp, sizeof(mat4) * light.numShadowCascades);
}

void dx_renderer::setPointLights(const ref<dx_buffer>& lights, uint32 numLights, const ref<dx_buffer>& shadowInfoBuffer)
{
	pointLights = lights;
	numPointLights = numLights;
	pointLightShadowInfoBuffer = shadowInfoBuffer;
}

void dx_renderer::setSpotLights(const ref<dx_buffer>& lights, uint32 numLights, const ref<dx_buffer>& shadowInfoBuffer)
{
	spotLights = lights;
	numSpotLights = numLights;
	spotLightShadowInfoBuffer = shadowInfoBuffer;
}

void dx_renderer::setDecals(const ref<dx_buffer>& decals, uint32 numDecals, const ref<dx_texture>& textureAtlas)
{
	assert(numDecals < MAX_NUM_TOTAL_DECALS);
	this->decals = decals;
	this->numDecals = numDecals;
	this->decalTextureAtlas = textureAtlas;
}

void dx_renderer::endFrame(const user_input& input)
{
	bool aspectRatioModeChanged = aspectRatioMode != oldAspectRatioMode;
	oldAspectRatioMode = aspectRatioMode;

	if (aspectRatioModeChanged)
	{
		recalculateViewport(true);
	}

	if (sunShadowRenderPass)
	{
		for (uint32 i = 0; i < sun.numShadowCascades; ++i)
		{
			auto vp = sunShadowRenderPass->viewports[i];
			sun.viewports[i] = vec4(vp.x, vp.y, vp.size, vp.size) / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);
		}
	}


	auto jitteredCameraCBV = dxContext.uploadDynamicConstantBuffer(jitteredCamera);
	auto unjitteredCameraCBV = dxContext.uploadDynamicConstantBuffer(unjitteredCamera);
	auto sunCBV = dxContext.uploadDynamicConstantBuffer(sun);

	common_material_info materialInfo;
	if (environment)
	{
		materialInfo.sky = environment->sky;
		materialInfo.environment = environment->environment;
		materialInfo.irradiance = environment->irradiance;
	}
	else
	{
		materialInfo.sky = render_resources::blackCubeTexture;
		materialInfo.environment = render_resources::blackCubeTexture;
		materialInfo.irradiance = render_resources::blackCubeTexture;
	}
	materialInfo.environmentIntensity = environmentIntensity;
	materialInfo.skyIntensity = skyIntensity;
	materialInfo.brdf = render_resources::brdfTex;
	materialInfo.tiledCullingGrid = culling.tiledCullingGrid;
	materialInfo.tiledObjectsIndexList = culling.tiledObjectsIndexList;
	materialInfo.pointLightBuffer = pointLights;
	materialInfo.spotLightBuffer = spotLights;
	materialInfo.decalBuffer = decals;
	materialInfo.shadowMap = render_resources::shadowMap;
	materialInfo.decalTextureAtlas = decalTextureAtlas;
	materialInfo.pointLightShadowInfoBuffer = pointLightShadowInfoBuffer;
	materialInfo.spotLightShadowInfoBuffer = spotLightShadowInfoBuffer;
	materialInfo.volumetricsTexture = 0;
	materialInfo.cameraCBV = jitteredCameraCBV;
	materialInfo.sunCBV = sunCBV;



	dx_command_list* cl = dxContext.getFreeRenderCommandList();



	D3D12_RESOURCE_STATES frameResultState = D3D12_RESOURCE_STATE_COMMON;

	if (aspectRatioModeChanged)
	{
		cl->transitionBarrier(frameResult, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cl->clearRTV(frameResult, 0.f, 0.f, 0.f);
		frameResultState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	barrier_batcher(cl)
		.transitionBegin(frameResult, frameResultState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);




	if (mode == renderer_mode_rasterized)
	{
		cl->clearDepthAndStencil(depthStencilBuffer->dsvHandle);

		cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);



		if (performedSkinning)
		{
			dxContext.renderQueue.waitForOtherQueue(dxContext.computeQueue); // Wait for GPU skinning.
		}


		// ----------------------------------------
		// DEPTH-ONLY PASS
		// ----------------------------------------

		dx_render_target depthOnlyRenderTarget({ screenVelocitiesTexture, objectIDsTexture }, depthStencilBuffer);

		cl->setRenderTarget(depthOnlyRenderTarget);
		cl->setViewport(depthOnlyRenderTarget.viewport);

		{
			DX_PROFILE_BLOCK(cl, "Depth pre-pass");

			// Static.
			if (opaqueRenderPass && opaqueRenderPass->staticDepthOnlyDrawCalls.size() > 0)
			{
				DX_PROFILE_BLOCK(cl, "Static");

				cl->setPipelineState(*depthOnlyPipeline.pipeline);
				cl->setGraphicsRootSignature(*depthOnlyPipeline.rootSignature);

				cl->setGraphicsDynamicConstantBuffer(DEPTH_ONLY_RS_CAMERA, materialInfo.cameraCBV);

				for (const auto& dc : opaqueRenderPass->staticDepthOnlyDrawCalls)
				{
					const mat4& m = dc.transform;
					const submesh_info& submesh = dc.submesh;

					cl->setGraphics32BitConstants(DEPTH_ONLY_RS_OBJECT_ID, (uint32)dc.objectID);
					cl->setGraphics32BitConstants(DEPTH_ONLY_RS_MVP, depth_only_transform_cb{ jitteredCamera.viewProj * m, jitteredCamera.prevFrameViewProj * m });

					cl->setVertexBuffer(0, dc.vertexBuffer);
					cl->setIndexBuffer(dc.indexBuffer);
					cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
				}
			}

			// Dynamic.
			if (opaqueRenderPass && opaqueRenderPass->dynamicDepthOnlyDrawCalls.size() > 0)
			{
				DX_PROFILE_BLOCK(cl, "Dynamic");

				cl->setPipelineState(*depthOnlyPipeline.pipeline);
				cl->setGraphicsRootSignature(*depthOnlyPipeline.rootSignature);

				cl->setGraphicsDynamicConstantBuffer(DEPTH_ONLY_RS_CAMERA, materialInfo.cameraCBV);

				for (const auto& dc : opaqueRenderPass->dynamicDepthOnlyDrawCalls)
				{
					const mat4& m = dc.transform;
					const mat4& prevFrameM = dc.prevFrameTransform;
					const submesh_info& submesh = dc.submesh;

					cl->setGraphics32BitConstants(DEPTH_ONLY_RS_OBJECT_ID, (uint32)dc.objectID);
					cl->setGraphics32BitConstants(DEPTH_ONLY_RS_MVP, depth_only_transform_cb{ jitteredCamera.viewProj * m, jitteredCamera.prevFrameViewProj * prevFrameM });

					cl->setVertexBuffer(0, dc.vertexBuffer);
					cl->setIndexBuffer(dc.indexBuffer);
					cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
				}
			}

			// Animated.
			if (opaqueRenderPass && opaqueRenderPass->animatedDepthOnlyDrawCalls.size() > 0)
			{
				DX_PROFILE_BLOCK(cl, "Animated");

				cl->setPipelineState(*animatedDepthOnlyPipeline.pipeline);
				cl->setGraphicsRootSignature(*animatedDepthOnlyPipeline.rootSignature);

				cl->setGraphicsDynamicConstantBuffer(DEPTH_ONLY_RS_CAMERA, materialInfo.cameraCBV);

				for (const auto& dc : opaqueRenderPass->animatedDepthOnlyDrawCalls)
				{
					const mat4& m = dc.transform;
					const mat4& prevFrameM = dc.prevFrameTransform;
					const submesh_info& submesh = dc.submesh;
					const submesh_info& prevFrameSubmesh = dc.prevFrameSubmesh;
					const ref<dx_vertex_buffer>& prevFrameVertexBuffer = dc.prevFrameVertexBuffer;

					cl->setGraphics32BitConstants(DEPTH_ONLY_RS_OBJECT_ID, (uint32)dc.objectID);
					cl->setGraphics32BitConstants(DEPTH_ONLY_RS_MVP, depth_only_transform_cb{ jitteredCamera.viewProj * m, jitteredCamera.prevFrameViewProj * prevFrameM });
					cl->setRootGraphicsSRV(DEPTH_ONLY_RS_PREV_FRAME_POSITIONS, prevFrameVertexBuffer->gpuVirtualAddress + prevFrameSubmesh.baseVertex * prevFrameVertexBuffer->elementSize);

					cl->setVertexBuffer(0, dc.vertexBuffer);
					cl->setIndexBuffer(dc.indexBuffer);
					cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
				}
			}
		}



		barrier_batcher(cl)
			.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);


		// ----------------------------------------
		// LIGHT & DECAL CULLING
		// ----------------------------------------

		lightAndDecalCulling(cl, depthStencilBuffer, pointLights, spotLights, decals, culling, numPointLights, numSpotLights, numDecals, materialInfo.cameraCBV);


		// ----------------------------------------
		// LINEAR DEPTH PYRAMID
		// ----------------------------------------

		linearDepthPyramid(cl, depthStencilBuffer, linearDepthBuffer, jitteredCamera.projectionParams);

		barrier_batcher(cl)
			//.uav(linearDepthBuffer)
			.transitionBegin(linearDepthBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);




		// ----------------------------------------
		// SHADOW MAP PASS
		// ----------------------------------------

		if (sunShadowRenderPass || numSpotLightShadowRenderPasses || numPointLightShadowRenderPasses)
		{
			DX_PROFILE_BLOCK(cl, "Shadow map pass");


			clear_rect clearRects[128];
			uint32 numClearRects = 0;

			shadow_map_viewport copiesFromStaticCache[128];
			uint32 numCopiesFromStaticCache = 0;

			shadow_map_viewport copiesToStaticCache[128];
			uint32 numCopiesToStaticCache = 0;

			{
				if (sunShadowRenderPass)
				{
					for (uint32 i = 0; i < sun.numShadowCascades; ++i)
					{
						shadow_map_viewport vp = sunShadowRenderPass->viewports[i];
						if (sunShadowRenderPass->copyFromStaticCache)
						{
							copiesFromStaticCache[numCopiesFromStaticCache++] = vp;
						}
						else
						{
							clearRects[numClearRects++] = { vp.x, vp.y, vp.size, vp.size };
							copiesToStaticCache[numCopiesToStaticCache++] = vp;
						}
					}
				}

				for (uint32 i = 0; i < numSpotLightShadowRenderPasses; ++i)
				{
					shadow_map_viewport vp = spotLightShadowRenderPasses[i]->viewport;
					if (spotLightShadowRenderPasses[i]->copyFromStaticCache)
					{
						copiesFromStaticCache[numCopiesFromStaticCache++] = vp;
					}
					else
					{
						clearRects[numClearRects++] = { vp.x, vp.y, vp.size, vp.size };
						copiesToStaticCache[numCopiesToStaticCache++] = vp;
					}
				}

				for (uint32 i = 0; i < numPointLightShadowRenderPasses; ++i)
				{
					for (uint32 j = 0; j < 2; ++j)
					{
						shadow_map_viewport vp = (j == 0) ? pointLightShadowRenderPasses[i]->viewport0 : pointLightShadowRenderPasses[i]->viewport1;
						bool copy = (j == 0) ? pointLightShadowRenderPasses[i]->copyFromStaticCache0 : pointLightShadowRenderPasses[i]->copyFromStaticCache1;

						if (copy)
						{
							copiesFromStaticCache[numCopiesFromStaticCache++] = vp;
						}
						else
						{
							clearRects[numClearRects++] = { vp.x, vp.y, vp.size, vp.size };
							copiesToStaticCache[numCopiesToStaticCache++] = vp;
						}
					}
				}
			}

			if (numCopiesFromStaticCache)
			{
				DX_PROFILE_BLOCK(cl, "Copy from static shadow map cache");
				copyShadowMapParts(cl, render_resources::staticShadowMapCache, render_resources::shadowMap , copiesFromStaticCache, numCopiesFromStaticCache);
			}

			if (numClearRects)
			{
				cl->clearDepth(render_resources::shadowMap->dsvHandle, 1.f, clearRects, numClearRects);
			}

			if (numCopiesToStaticCache)
			{
				barrier_batcher(cl)
					.transitionBegin(render_resources::staticShadowMapCache, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			}


			auto renderSunCascadeShadow = [](dx_command_list* cl, const std::vector<shadow_render_pass::draw_call>& drawCalls, const mat4& viewProj)
			{
				for (const auto& dc : drawCalls)
				{
					const mat4& m = dc.transform;
					const submesh_info& submesh = dc.submesh;
					cl->setGraphics32BitConstants(SHADOW_RS_MVP, viewProj * m);

					cl->setVertexBuffer(0, dc.vertexBuffer);
					cl->setIndexBuffer(dc.indexBuffer);

					cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
				}
			};
			auto renderSpotShadow = [](dx_command_list* cl, const std::vector<shadow_render_pass::draw_call>& drawCalls, const mat4& viewProj)
			{
				for (const auto& dc : drawCalls)
				{
					const mat4& m = dc.transform;
					const submesh_info& submesh = dc.submesh;
					cl->setGraphics32BitConstants(SHADOW_RS_MVP, viewProj * m);

					cl->setVertexBuffer(0, dc.vertexBuffer);
					cl->setIndexBuffer(dc.indexBuffer);

					cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
				}
			};
			auto renderPointShadow = [](dx_command_list* cl, const std::vector<shadow_render_pass::draw_call>& drawCalls, vec3 lightPosition, float maxDistance, float flip)
			{
				for (const auto& dc : drawCalls)
				{
					const mat4& m = dc.transform;
					const submesh_info& submesh = dc.submesh;
					cl->setGraphics32BitConstants(SHADOW_RS_MVP,
						point_shadow_transform_cb
						{
							m,
							lightPosition,
							maxDistance,
							flip
						});

					cl->setVertexBuffer(0, dc.vertexBuffer);
					cl->setIndexBuffer(dc.indexBuffer);

					cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
				}
			};


			dx_render_target shadowRenderTarget({}, render_resources::shadowMap);
			cl->setRenderTarget(shadowRenderTarget);

			if (sunShadowRenderPass || numSpotLightShadowRenderPasses)
			{
				cl->setPipelineState(*shadowPipeline.pipeline);
				cl->setGraphicsRootSignature(*shadowPipeline.rootSignature);
			}

			if (sunShadowRenderPass)
			{
				DX_PROFILE_BLOCK(cl, "Sun static geometry");

				for (uint32 i = 0; i < sun.numShadowCascades; ++i)
				{
					DX_PROFILE_BLOCK(cl, (i == 0) ? "First cascade" : (i == 1) ? "Second cascade" : (i == 2) ? "Third cascade" : "Fourth cascade");

					shadow_map_viewport vp = sunShadowRenderPass->viewports[i];
					clear_rect rect = { vp.x, vp.y, vp.size, vp.size };
					cl->setViewport(vp.x, vp.y, vp.size, vp.size);

					for (uint32 cascade = 0; cascade <= i; ++cascade)
					{
						renderSunCascadeShadow(cl, sunShadowRenderPass->staticDrawCalls[cascade], sun.vp[i]);
					}
				}
			}

			if (numSpotLightShadowRenderPasses)
			{
				DX_PROFILE_BLOCK(cl, "Spot lights static geometry");

				for (uint32 i = 0; i < numSpotLightShadowRenderPasses; ++i)
				{
					DX_PROFILE_BLOCK(cl, "Single light");

					shadow_map_viewport vp = spotLightShadowRenderPasses[i]->viewport;
					cl->setViewport(vp.x, vp.y, vp.size, vp.size);

					renderSpotShadow(cl, spotLightShadowRenderPasses[i]->staticDrawCalls, spotLightShadowRenderPasses[i]->viewProjMatrix);
				}
			}

			if (numPointLightShadowRenderPasses)
			{
				DX_PROFILE_BLOCK(cl, "Point lights static geometry");

				cl->setPipelineState(*pointLightShadowPipeline.pipeline);
				cl->setGraphicsRootSignature(*pointLightShadowPipeline.rootSignature);

				for (uint32 i = 0; i < numPointLightShadowRenderPasses; ++i)
				{
					DX_PROFILE_BLOCK(cl, "Single light");

					for (uint32 v = 0; v < 2; ++v)
					{
						DX_PROFILE_BLOCK(cl, (v == 0) ? "First hemisphere" : "Second hemisphere");

						shadow_map_viewport vp = (v == 0) ? pointLightShadowRenderPasses[i]->viewport0 : pointLightShadowRenderPasses[i]->viewport1;
						cl->setViewport(vp.x, vp.y, vp.size, vp.size);

						renderPointShadow(cl, pointLightShadowRenderPasses[i]->staticDrawCalls, pointLightShadowRenderPasses[i]->lightPosition, pointLightShadowRenderPasses[i]->maxDistance, (v == 0) ? 1.f : -1.f);
					}
				}
			}

			if (numCopiesToStaticCache)
			{
				DX_PROFILE_BLOCK(cl, "Copy to static shadow map cache");

				barrier_batcher(cl)
					.transition(render_resources::shadowMap, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
					.transitionEnd(render_resources::staticShadowMapCache, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);

				copyShadowMapParts(cl, render_resources::shadowMap, render_resources::staticShadowMapCache, copiesToStaticCache, numCopiesToStaticCache);

				barrier_batcher(cl)
					.transition(render_resources::shadowMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE)
					.transition(render_resources::staticShadowMapCache, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}


			cl->setRenderTarget(shadowRenderTarget);

			if (sunShadowRenderPass || numSpotLightShadowRenderPasses)
			{
				cl->setPipelineState(*shadowPipeline.pipeline);
				cl->setGraphicsRootSignature(*shadowPipeline.rootSignature);
			}

			if (sunShadowRenderPass)
			{
				DX_PROFILE_BLOCK(cl, "Sun dynamic geometry");

				for (uint32 i = 0; i < sun.numShadowCascades; ++i)
				{
					DX_PROFILE_BLOCK(cl, (i == 0) ? "First cascade" : (i == 1) ? "Second cascade" : (i == 2) ? "Third cascade" : "Fourth cascade");

					shadow_map_viewport vp = sunShadowRenderPass->viewports[i];
					clear_rect rect = { vp.x, vp.y, vp.size, vp.size };
					cl->setViewport(vp.x, vp.y, vp.size, vp.size);

					for (uint32 cascade = 0; cascade <= i; ++cascade)
					{
						renderSunCascadeShadow(cl, sunShadowRenderPass->dynamicDrawCalls[cascade], sun.vp[i]);
					}
				}
			}

			if (numSpotLightShadowRenderPasses)
			{
				DX_PROFILE_BLOCK(cl, "Spot lights dynamic geometry");

				for (uint32 i = 0; i < numSpotLightShadowRenderPasses; ++i)
				{
					DX_PROFILE_BLOCK(cl, "Single light");

					shadow_map_viewport vp = spotLightShadowRenderPasses[i]->viewport;
					cl->setViewport(vp.x, vp.y, vp.size, vp.size);

					renderSpotShadow(cl, spotLightShadowRenderPasses[i]->dynamicDrawCalls, spotLightShadowRenderPasses[i]->viewProjMatrix);
				}
			}

			if (numPointLightShadowRenderPasses)
			{
				DX_PROFILE_BLOCK(cl, "Point lights dynamic geometry");

				cl->setPipelineState(*pointLightShadowPipeline.pipeline);
				cl->setGraphicsRootSignature(*pointLightShadowPipeline.rootSignature);

				for (uint32 i = 0; i < numPointLightShadowRenderPasses; ++i)
				{
					DX_PROFILE_BLOCK(cl, "Single light");

					for (uint32 v = 0; v < 2; ++v)
					{
						DX_PROFILE_BLOCK(cl, (v == 0) ? "First hemisphere" : "Second hemisphere");

						shadow_map_viewport vp = (v == 0) ? pointLightShadowRenderPasses[i]->viewport0 : pointLightShadowRenderPasses[i]->viewport1;
						cl->setViewport(vp.x, vp.y, vp.size, vp.size);

						renderPointShadow(cl, pointLightShadowRenderPasses[i]->dynamicDrawCalls, pointLightShadowRenderPasses[i]->lightPosition, pointLightShadowRenderPasses[i]->maxDistance, (v == 0) ? 1.f : -1.f);
					}
				}
			}
		}

		barrier_batcher(cl)
			.transition(render_resources::shadowMap, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);




		// ----------------------------------------
		// SKY PASS
		// ----------------------------------------

		dx_render_target skyRenderTarget({ hdrColorTexture, screenVelocitiesTexture, objectIDsTexture }, depthStencilBuffer);

		cl->setRenderTarget(skyRenderTarget);
		cl->setViewport(skyRenderTarget.viewport);

		{
			DX_PROFILE_BLOCK(cl, "Sky");

			if (environment)
			{
				cl->setPipelineState(*textureSkyPipeline.pipeline);
				cl->setGraphicsRootSignature(*textureSkyPipeline.rootSignature);

				cl->setGraphics32BitConstants(SKY_RS_VP, sky_cb{ jitteredCamera.proj * createSkyViewMatrix(jitteredCamera.view) });
				cl->setGraphics32BitConstants(SKY_RS_INTENSITY, sky_intensity_cb{ skyIntensity });
				cl->setDescriptorHeapSRV(SKY_RS_TEX, 0, environment->sky->defaultSRV);

				cl->drawCubeTriangleStrip();
			}
			else
			{
				cl->setPipelineState(*proceduralSkyPipeline.pipeline);
				cl->setGraphicsRootSignature(*proceduralSkyPipeline.rootSignature);

				cl->setGraphics32BitConstants(SKY_RS_VP, sky_cb{ jitteredCamera.proj * createSkyViewMatrix(jitteredCamera.view) });
				cl->setGraphics32BitConstants(SKY_RS_INTENSITY, sky_intensity_cb{ skyIntensity });

				cl->drawCubeTriangleStrip();
			}

			cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // Cube renders a triangle strip, so reset back to triangle list.
		}



		// Copy hovered object id to readback buffer.
		if (objectIDsTexture)
		{
			DX_PROFILE_BLOCK(cl, "Copy hovered object ID");

			barrier_batcher(cl)
				.transition(objectIDsTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);

			if (input.overWindow)
			{
				cl->copyTextureRegionToBuffer(objectIDsTexture, hoveredObjectIDReadbackBuffer, dxContext.bufferedFrameID, (uint32)input.mouse.x, (uint32)input.mouse.y, 1, 1);
			}

			barrier_batcher(cl)
				.transitionBegin(objectIDsTexture, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		}



		// ----------------------------------------
		// OPAQUE LIGHT PASS
		// ----------------------------------------

		dx_render_target hdrOpaqueRenderTarget({ hdrColorTexture, worldNormalsTexture, reflectanceTexture }, depthStencilBuffer);

		cl->setRenderTarget(hdrOpaqueRenderTarget);
		cl->setViewport(hdrOpaqueRenderTarget.viewport);

		if (opaqueRenderPass && opaqueRenderPass->drawCalls.size() > 0)
		{
			DX_PROFILE_BLOCK(cl, "Main opaque light pass");

			material_setup_function lastSetupFunc = 0;

			for (const auto& dc : opaqueRenderPass->drawCalls)
			{
				const mat4& m = dc.transform;
				const submesh_info& submesh = dc.submesh;

				if (dc.materialSetupFunc != lastSetupFunc)
				{
					dc.materialSetupFunc(cl, materialInfo);
					lastSetupFunc = dc.materialSetupFunc;
				}

				dc.material->prepareForRendering(cl);

				cl->setGraphics32BitConstants(0, transform_cb{ jitteredCamera.viewProj * m, m });

				cl->setVertexBuffer(0, dc.vertexBuffer.positions);
				cl->setVertexBuffer(1, dc.vertexBuffer.others);
				cl->setIndexBuffer(dc.indexBuffer);
				cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
			}
		}

		barrier_batcher(cl)
			.transitionBegin(worldNormalsTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			.transitionBegin(screenVelocitiesTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);




		{
			DX_PROFILE_BLOCK(cl, "Transition textures");

			barrier_batcher(cl)
				.transition(hdrColorTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ)
				.transitionEnd(worldNormalsTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.transitionEnd(screenVelocitiesTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.transition(reflectanceTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.transitionEnd(frameResult, frameResultState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				.transitionEnd(linearDepthBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}



		{
			DX_PROFILE_BLOCK(cl, "Copy depth buffer");
			cl->copyResource(depthStencilBuffer->resource, opaqueDepthBuffer->resource);
		}

		barrier_batcher(cl)
			.transitionBegin(opaqueDepthBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);



		// ----------------------------------------
		// SCREEN SPACE REFLECTIONS
		// ----------------------------------------

		if (enableSSR)
		{
			screenSpaceReflections(cl, hdrColorTexture, prevFrameHDRColorTexture, depthStencilBuffer, linearDepthBuffer, worldNormalsTexture,
				reflectanceTexture, screenVelocitiesTexture, ssrRaycastTexture, ssrResolveTexture, ssrTemporalTextures[ssrHistoryIndex],
				ssrTemporalTextures[1 - ssrHistoryIndex], ssrSettings, materialInfo.cameraCBV);

			ssrHistoryIndex = 1 - ssrHistoryIndex;
		}

		{
			DX_PROFILE_BLOCK(cl, "Specular ambient");

			specularAmbient(cl, hdrColorTexture, enableSSR ? ssrResolveTexture : 0, worldNormalsTexture, reflectanceTexture, 
				environment ? environment->environment : 0, hdrPostProcessingTexture, materialInfo.cameraCBV);

			barrier_batcher(cl)
				//.uav(hdrPostProcessingTexture)
				.transition(hdrPostProcessingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE); // Will be read by rest of post processing stack. 

			if (enableSSR)
			{
				barrier_batcher(cl)
					.transition(ssrResolveTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS); // For next frame.
			}
		}


		barrier_batcher(cl)
			.transitionEnd(opaqueDepthBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);



		// After this there is no more camera jittering!
		materialInfo.cameraCBV = unjitteredCameraCBV;
		materialInfo.opaqueDepth = opaqueDepthBuffer;
		materialInfo.worldNormals = worldNormalsTexture;




		ref<dx_texture> hdrResult = hdrPostProcessingTexture; // Specular highlights have been rendered to this texture. It's in read state.


		// ----------------------------------------
		// TRANSPARENT LIGHT PASS
		// ----------------------------------------


		if (transparentRenderPass && (transparentRenderPass->drawCalls.size() > 0 || transparentRenderPass->particleDrawCalls.size() > 0))
		{
			DX_PROFILE_BLOCK(cl, "Transparent light pass");

			barrier_batcher(cl)
				.transition(hdrResult, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
				.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

			dx_render_target hdrTransparentRenderTarget({ hdrResult }, depthStencilBuffer);

			cl->setRenderTarget(hdrTransparentRenderTarget);
			cl->setViewport(hdrTransparentRenderTarget.viewport);


			material_setup_function lastSetupFunc = 0;

			{
				DX_PROFILE_BLOCK(cl, "Transparent geometry");

				for (const auto& dc : transparentRenderPass->drawCalls)
				{
					const mat4& m = dc.transform;
					const submesh_info& submesh = dc.submesh;

					if (dc.materialSetupFunc != lastSetupFunc)
					{
						dc.materialSetupFunc(cl, materialInfo);
						lastSetupFunc = dc.materialSetupFunc;
					}

					dc.material->prepareForRendering(cl);

					cl->setGraphics32BitConstants(0, transform_cb{ unjitteredCamera.viewProj * m, m });

					cl->setVertexBuffer(0, dc.vertexBuffer.positions);
					cl->setVertexBuffer(1, dc.vertexBuffer.others);
					cl->setIndexBuffer(dc.indexBuffer);
					cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
				}
			}

			if (transparentRenderPass->particleDrawCalls.size() > 0)
			{
				DX_PROFILE_BLOCK(cl, "Particles");

				for (const auto& dc : transparentRenderPass->particleDrawCalls)
				{
					if (dc.materialSetupFunc != lastSetupFunc)
					{
						dc.materialSetupFunc(cl, materialInfo);
						lastSetupFunc = dc.materialSetupFunc;
					}

					dc.material->prepareForRendering(cl);

					const particle_draw_info& info = dc.drawInfo;

					cl->setRootGraphicsSRV(info.rootParameterOffset + PARTICLE_RENDERING_RS_PARTICLES, info.particleBuffer->gpuVirtualAddress);
					cl->setRootGraphicsSRV(info.rootParameterOffset + PARTICLE_RENDERING_RS_ALIVE_LIST, info.aliveList->gpuVirtualAddress + info.aliveListOffset);

					cl->setVertexBuffer(0, dc.vertexBuffer.positions);
					if (dc.vertexBuffer.others)
					{
						cl->setVertexBuffer(1, dc.vertexBuffer.others);
					}
					cl->setIndexBuffer(dc.indexBuffer);

					cl->drawIndirect(particleCommandSignature, 1, info.commandBuffer, info.commandBufferOffset);
				}
			}


			barrier_batcher(cl)
				.transition(hdrResult, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
		}





		// ----------------------------------------
		// POST PROCESSING
		// ----------------------------------------


		// TAA.
		if (enableTAA)
		{
			uint32 taaOutputIndex = 1 - taaHistoryIndex;
			temporalAntiAliasing(cl, hdrResult, screenVelocitiesTexture, opaqueDepthBuffer, taaTextures[taaHistoryIndex], taaTextures[taaOutputIndex], jitteredCamera.projectionParams);
			hdrResult = taaTextures[taaOutputIndex];
			taaHistoryIndex = taaOutputIndex;
		}

		// At this point hdrResult is either the TAA result or the hdrPostProcessingTexture. Either one is in read state.


		// Downsample scene. This is also the copy used in SSR next frame.
		downsample(cl, hdrResult, prevFrameHDRColorTexture, prevFrameHDRColorTempTexture);



		// Bloom.
		if (enableBloom)
		{
			barrier_batcher(cl)
				.transition(hdrColorTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			bloom(cl, hdrResult, hdrColorTexture, bloomTexture, bloomTempTexture, bloomSettings);
			hdrResult = hdrColorTexture;
		}

		// At this point hdrResult is either the TAA result, the hdrColorTexture, or the hdrPostProcessingTexture. Either one is in read state.



		tonemap(cl, hdrResult, ldrPostProcessingTexture, tonemapSettings);


		// ----------------------------------------
		// LDR RENDERING
		// ----------------------------------------

		bool renderingOverlays = overlayRenderPass && overlayRenderPass->drawCalls.size();
		bool renderingOutlines = opaqueRenderPass && opaqueRenderPass->outlinedObjects.size() > 0 ||
			transparentRenderPass && transparentRenderPass->outlinedObjects.size() > 0;
		bool renderingToLDRPostprocessingTexture = renderingOverlays || renderingOutlines;

		D3D12_RESOURCE_STATES ldrPostProcessingTextureState = renderingToLDRPostprocessingTexture ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;



		barrier_batcher(cl)
			//.uav(ldrPostProcessingTexture)
			.transition(ldrPostProcessingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, ldrPostProcessingTextureState)
			.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);



		// ----------------------------------------
		// OVERLAYS
		// ----------------------------------------

		dx_render_target ldrRenderTarget({ ldrPostProcessingTexture }, depthStencilBuffer);

		if (renderingOverlays)
		{
			DX_PROFILE_BLOCK(cl, "3D Overlays");

			cl->setRenderTarget(ldrRenderTarget);
			cl->setViewport(ldrRenderTarget.viewport);

			cl->clearDepth(depthStencilBuffer->dsvHandle);

			material_setup_function lastSetupFunc = 0;

			for (const auto& dc : overlayRenderPass->drawCalls)
			{
				const mat4& m = dc.transform;

				if (dc.materialSetupFunc != lastSetupFunc)
				{
					dc.materialSetupFunc(cl, materialInfo);
					lastSetupFunc = dc.materialSetupFunc;
				}

				dc.material->prepareForRendering(cl);

				if (dc.setTransform)
				{
					cl->setGraphics32BitConstants(0, transform_cb{ unjitteredCamera.viewProj * m, m });
				}

				if (dc.drawType == geometry_render_pass::draw_type_default)
				{
					const submesh_info& submesh = dc.submesh;

					cl->setVertexBuffer(0, dc.vertexBuffer.positions);
					cl->setVertexBuffer(1, dc.vertexBuffer.others);
					cl->setIndexBuffer(dc.indexBuffer);
					cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
				}
				else
				{
					cl->dispatchMesh(dc.dispatchInfo.dispatchX, dc.dispatchInfo.dispatchY, dc.dispatchInfo.dispatchZ);
				}
			}
		}



		// ----------------------------------------
		// OUTLINES
		// ----------------------------------------

		if (renderingOutlines)
		{
			DX_PROFILE_BLOCK(cl, "Outlines");

			cl->setRenderTarget(ldrRenderTarget);
			cl->setViewport(ldrRenderTarget.viewport);

			cl->setStencilReference(stencil_flag_selected_object);

			cl->setPipelineState(*outlineMarkerPipeline.pipeline);
			cl->setGraphicsRootSignature(*outlineMarkerPipeline.rootSignature);

			// Mark object in stencil.
			auto mark = [](const geometry_render_pass& rp, dx_command_list* cl, const mat4& viewProj)
			{
				for (const auto& outlined : rp.outlinedObjects)
				{
					const submesh_info& submesh = rp.drawCalls[outlined].submesh;
					const mat4& m = rp.drawCalls[outlined].transform;
					const auto& vertexBuffer = rp.drawCalls[outlined].vertexBuffer;
					const auto& indexBuffer = rp.drawCalls[outlined].indexBuffer;

					cl->setGraphics32BitConstants(OUTLINE_RS_MVP, outline_marker_cb{ viewProj * m });

					cl->setVertexBuffer(0, vertexBuffer.positions);
					cl->setIndexBuffer(indexBuffer);
					cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
				}
			};

			mark(*opaqueRenderPass, cl, unjitteredCamera.viewProj);
			//mark(*transparentRenderPass, cl, unjitteredCamera.viewProj);

			// Draw outline.
			cl->transitionBarrier(depthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ);

			cl->setPipelineState(*outlineDrawerPipeline.pipeline);
			cl->setGraphicsRootSignature(*outlineDrawerPipeline.rootSignature);

			cl->setGraphics32BitConstants(OUTLINE_RS_CB, outline_drawer_cb{ (int)renderWidth, (int)renderHeight });
			cl->setDescriptorHeapResource(OUTLINE_RS_STENCIL, 0, 1, depthStencilBuffer->stencilSRV);

			cl->drawFullscreenTriangle();

			cl->transitionBarrier(depthStencilBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}


		barrier_batcher(cl)
			.transition(ldrPostProcessingTexture, ldrPostProcessingTextureState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);



		// TODO: If we really care we should sharpenSettings before rendering overlays and outlines.

		present(cl, ldrPostProcessingTexture, frameResult, enableSharpen ? sharpenSettings : sharpen_settings{ 0.f });



		barrier_batcher(cl)
			//.uav(frameResult)
			.transition(render_resources::shadowMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE)
			.transition(hdrColorTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
			.transition(hdrPostProcessingTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.transition(worldNormalsTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
			.transition(screenVelocitiesTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
			.transitionEnd(objectIDsTexture, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
			.transition(ldrPostProcessingTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.transition(frameResult, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON)
			.transition(linearDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.transition(opaqueDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST)
			.transition(reflectanceTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

	}
	else if (dxContext.featureSupport.raytracing() && raytracer)
	{
		barrier_batcher(cl)
			.transition(hdrColorTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.transitionEnd(frameResult, frameResultState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		{
			DX_PROFILE_BLOCK(cl, "Raytracing");

			dxContext.renderQueue.waitForOtherQueue(dxContext.computeQueue); // Wait for AS-rebuilds. TODO: This is not the way to go here. We should wait for the specific value returned by executeCommandList.

			raytracer->render(cl, *tlas, hdrColorTexture, materialInfo);
		}

		cl->resetToDynamicDescriptorHeap();

		barrier_batcher(cl)
			//.uav(hdrColorTexture)
			.transition(hdrColorTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		tonemap(cl, hdrColorTexture, ldrPostProcessingTexture, tonemapSettings);

		barrier_batcher(cl)
			.transition(ldrPostProcessingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		present(cl, ldrPostProcessingTexture, frameResult, { 0.f });

		barrier_batcher(cl)
			.transition(hdrColorTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
			.transition(ldrPostProcessingTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.transition(frameResult, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
	}


	dxContext.executeCommandList(cl);
}
