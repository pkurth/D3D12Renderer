#include "pch.h"
#include "dx_renderer.h"
#include "dx_command_list.h"
#include "dx_render_target.h"
#include "dx_pipeline.h"
#include "geometry.h"
#include "dx_texture.h"
#include "dx_barrier_batcher.h"
#include "texture_preprocessing.h"
#include "skinning.h"
#include "dx_context.h"
#include "dx_profiling.h"
#include "random.h"
#include "particles.h"

#include "depth_only_rs.hlsli"
#include "outline_rs.hlsli"
#include "sky_rs.hlsli"
#include "light_culling_rs.hlsli"
#include "camera.hlsli"
#include "transform.hlsli"
#include "random.h"

#include "raytracing.h"


static ref<dx_texture> whiteTexture;
static ref<dx_texture> blackTexture;
static ref<dx_texture> blackCubeTexture;
static ref<dx_texture> noiseTexture;

static ref<dx_texture> shadowMap;
static ref<dx_texture> staticShadowMapCache;

static dx_pipeline depthOnlyPipeline;
static dx_pipeline animatedDepthOnlyPipeline;
static dx_pipeline shadowPipeline;
static dx_pipeline pointLightShadowPipeline;

static dx_pipeline textureSkyPipeline;
static dx_pipeline proceduralSkyPipeline;

static dx_pipeline atmospherePipeline;

static dx_pipeline outlineMarkerPipeline;
static dx_pipeline outlineDrawerPipeline;

static dx_pipeline worldSpaceFrustaPipeline;
static dx_pipeline lightCullingPipeline;

static dx_pipeline ssrRaycastPipeline;
static dx_pipeline ssrResolvePipeline;
static dx_pipeline ssrTemporalPipeline;
static dx_pipeline ssrMedianBlurPipeline;

static dx_pipeline taaPipeline;
static dx_pipeline bloomThresholdPipeline;
static dx_pipeline bloomCombinePipeline;
static dx_pipeline gaussianBlur9x9Pipeline;
static dx_pipeline gaussianBlur5x5Pipeline;
static dx_pipeline specularAmbientPipeline;
static dx_pipeline blitPipeline;
static dx_pipeline hierarchicalLinearDepthPipeline;
static dx_pipeline tonemapPipeline;
static dx_pipeline presentPipeline;


static ref<dx_texture> brdfTex;



DXGI_FORMAT dx_renderer::screenFormat;

dx_cpu_descriptor_handle dx_renderer::nullTextureSRV;
dx_cpu_descriptor_handle dx_renderer::nullBufferSRV;


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


void dx_renderer::initializeCommon(DXGI_FORMAT screenFormat)
{
	dx_renderer::screenFormat = screenFormat;

	{
		uint8 white[] = { 255, 255, 255, 255 };
		whiteTexture = createTexture(white, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
		SET_NAME(whiteTexture->resource, "White");
	}
	{
		uint8 black[] = { 0, 0, 0, 255 };
		blackTexture = createTexture(black, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
		SET_NAME(blackTexture->resource, "Black");

		blackCubeTexture = createCubeTexture(black, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
		SET_NAME(blackCubeTexture->resource, "Black cube");
	}
	{
		noiseTexture = loadTextureFromFile("assets/noise/blue_noise.dds", texture_load_flags_noncolor); // Already compressed and in DDS format.
	}

	nullTextureSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().createNullTextureSRV();
	nullBufferSRV = dxContext.descriptorAllocatorCPU.getFreeHandle().createNullBufferSRV();

	initializeTexturePreprocessing();
	initializeSkinning();


	shadowMap = createDepthTexture(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, shadowDepthFormat);
	SET_NAME(shadowMap->resource, "Shadow map");

	staticShadowMapCache = createDepthTexture(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, shadowDepthFormat, 1, D3D12_RESOURCE_STATE_COPY_SOURCE);
	SET_NAME(staticShadowMapCache->resource, "Static shadow map cache");


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
			.inputLayout(inputLayout_position_uv_normal_tangent);

		depthOnlyPipeline = createReloadablePipeline(desc, { "depth_only_vs", "depth_only_ps" }, rs_in_vertex_shader);
		animatedDepthOnlyPipeline = createReloadablePipeline(desc, { "depth_only_animated_vs", "depth_only_ps" }, rs_in_vertex_shader);
	}

	// Shadow.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(0, 0, shadowDepthFormat)
			.inputLayout(inputLayout_position_uv_normal_tangent)
			//.cullFrontFaces()
			;

		shadowPipeline = createReloadablePipeline(desc, { "shadow_vs" }, rs_in_vertex_shader);
		pointLightShadowPipeline = createReloadablePipeline(desc, { "shadow_point_light_vs", "shadow_point_light_ps" }, rs_in_vertex_shader);
	}

	// Outline.
	{
		auto markerDesc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position_uv_normal_tangent)
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

	// Light culling.
	{
		worldSpaceFrustaPipeline = createReloadablePipeline("world_space_tiled_frusta_cs");
		lightCullingPipeline = createReloadablePipeline("light_culling_cs");
	}

	// Atmosphere.
	{
		atmospherePipeline = createReloadablePipeline("atmosphere_cs");
	}

	// Post processing.
	{
		taaPipeline = createReloadablePipeline("taa_cs");
		bloomThresholdPipeline = createReloadablePipeline("bloom_threshold_cs");
		bloomCombinePipeline = createReloadablePipeline("bloom_combine_cs");
		gaussianBlur9x9Pipeline = createReloadablePipeline("gaussian_blur_9x9_cs");
		gaussianBlur5x5Pipeline = createReloadablePipeline("gaussian_blur_5x5_cs");
		blitPipeline = createReloadablePipeline("blit_cs");
		specularAmbientPipeline = createReloadablePipeline("specular_ambient_cs");
		hierarchicalLinearDepthPipeline = createReloadablePipeline("hierarchical_linear_depth_cs");
		tonemapPipeline = createReloadablePipeline("tonemap_cs");
		presentPipeline = createReloadablePipeline("present_cs");
	}

	// SSR.
	{
		ssrRaycastPipeline = createReloadablePipeline("ssr_raycast_cs");
		ssrResolvePipeline = createReloadablePipeline("ssr_resolve_cs");
		ssrTemporalPipeline = createReloadablePipeline("ssr_temporal_cs");
		ssrMedianBlurPipeline = createReloadablePipeline("ssr_median_blur_cs");
	}


	pbr_material::initializePipeline();
	particle_material::initializePipeline();

	createAllPendingReloadablePipelines();


	{
		dx_command_list* cl = dxContext.getFreeRenderCommandList();
		brdfTex = integrateBRDF(cl);
		dxContext.executeCommandList(cl);
	}

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

	frameResult = createTexture(0, windowWidth, windowHeight, screenFormat, false, true, true);



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
	if (settings.aspectRatioMode == aspect_ratio_free)
	{
		windowXOffset = 0;
		windowYOffset = 0;
		renderWidth = windowWidth;
		renderHeight = windowHeight;
	}
	else
	{
		const float targetAspect = settings.aspectRatioMode == aspect_ratio_fix_16_9 ? (16.f / 9.f) : (16.f / 10.f);

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
	
	allocateLightCullingBuffers();
}

void dx_renderer::allocateLightCullingBuffers()
{
	numCullingTilesX = bucketize(renderWidth, LIGHT_CULLING_TILE_SIZE);
	numCullingTilesY = bucketize(renderHeight, LIGHT_CULLING_TILE_SIZE);

	bool firstAllocation = tiledCullingGrid == nullptr;

	if (firstAllocation)
	{
		tiledCullingGrid = createTexture(0, numCullingTilesX, numCullingTilesY,
			DXGI_FORMAT_R32G32B32A32_UINT, false, false, true);

		tiledCullingIndexCounter = createBuffer(sizeof(uint32), 1, 0, true, true);
		tiledObjectsIndexList = createBuffer(sizeof(uint32),
			numCullingTilesX * numCullingTilesY * MAX_NUM_INDICES_PER_TILE * 2, 0, true);
		tiledWorldSpaceFrustaBuffer = createBuffer(sizeof(light_culling_view_frustum), numCullingTilesX * numCullingTilesY, 0, true);

		SET_NAME(tiledCullingGrid->resource, "Tiled culling grid");
		SET_NAME(tiledCullingIndexCounter->resource, "Tiled index counter");
		SET_NAME(tiledObjectsIndexList->resource, "Tiled index list");
		SET_NAME(tiledWorldSpaceFrustaBuffer->resource, "Tiled frusta");
	}
	else
	{
		resizeTexture(tiledCullingGrid, numCullingTilesX, numCullingTilesY);
		resizeBuffer(tiledObjectsIndexList, numCullingTilesX * numCullingTilesY * MAX_NUM_INDICES_PER_TILE * 2);
		resizeBuffer(tiledWorldSpaceFrustaBuffer, numCullingTilesX * numCullingTilesY);
	}
}

void dx_renderer::gaussianBlur(dx_command_list* cl, ref<dx_texture> inputOutput, ref<dx_texture> temp, uint32 inputMip, uint32 outputMip, gaussian_blur_kernel_size kernel, uint32 numIterations)
{
	DX_PROFILE_BLOCK(cl, "Gaussian Blur");

	auto& pipeline =
		(kernel == gaussian_blur_5x5) ? gaussianBlur5x5Pipeline :
		(kernel == gaussian_blur_9x9) ? gaussianBlur9x9Pipeline : 
		gaussianBlur9x9Pipeline; // TODO: Emit error!

	cl->setPipelineState(*pipeline.pipeline);
	cl->setComputeRootSignature(*pipeline.rootSignature);

	uint32 outputWidth = inputOutput->width >> outputMip;
	uint32 outputHeight = inputOutput->height >> outputMip;

	uint32 widthBuckets = bucketize(outputWidth, POST_PROCESSING_BLOCK_SIZE);
	uint32 heightBuckets = bucketize(outputHeight, POST_PROCESSING_BLOCK_SIZE);

	assert((outputMip == 0) || ((uint32)inputOutput->mipUAVs.size() >= outputMip));
	assert((outputMip == 0) || ((uint32)temp->mipUAVs.size() >= outputMip));
	assert(inputMip <= outputMip); // Currently only downsampling supported.

	float scale = 1.f / (1 << (outputMip - inputMip));

	uint32 sourceMip = inputMip;
	gaussian_blur_cb cb = { vec2(1.f / outputWidth, 1.f / outputHeight), scale };

	for (uint32 i = 0; i < numIterations; ++i)
	{
		DX_PROFILE_BLOCK(cl, "Iteration");

		{
			DX_PROFILE_BLOCK(cl, "Vertical");

			dx_cpu_descriptor_handle tempUAV = (outputMip == 0) ? temp->defaultUAV : temp->mipUAVs[outputMip - 1];

			// Vertical pass.
			cb.directionAndSourceMipLevel = (1 << 16) | sourceMip;
			cl->setCompute32BitConstants(GAUSSIAN_BLUR_RS_CB, cb);
			cl->setDescriptorHeapUAV(GAUSSIAN_BLUR_RS_TEXTURES, 0, tempUAV);
			cl->setDescriptorHeapSRV(GAUSSIAN_BLUR_RS_TEXTURES, 1, inputOutput);

			cl->dispatch(widthBuckets, heightBuckets);

			barrier_batcher(cl)
				.uav(temp)
				.transition(temp, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.transition(inputOutput, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		cb.stepScale = 1.f;
		sourceMip = outputMip; // From here on we sample from the output mip.

		{
			DX_PROFILE_BLOCK(cl, "Horizontal");

			dx_cpu_descriptor_handle outputUAV = (outputMip == 0) ? inputOutput->defaultUAV : inputOutput->mipUAVs[outputMip - 1];

			// Horizontal pass.
			cb.directionAndSourceMipLevel = (0 << 16) | sourceMip;
			cl->setCompute32BitConstants(GAUSSIAN_BLUR_RS_CB, cb);
			cl->setDescriptorHeapUAV(GAUSSIAN_BLUR_RS_TEXTURES, 0, outputUAV);
			cl->setDescriptorHeapSRV(GAUSSIAN_BLUR_RS_TEXTURES, 1, temp);

			cl->dispatch(widthBuckets, heightBuckets);

			barrier_batcher(cl)
				.uav(inputOutput)
				.transition(temp, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				.transition(inputOutput, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}
	}
}

void dx_renderer::specularAmbient(dx_command_list* cl, dx_dynamic_constant_buffer cameraCBV, const ref<dx_texture>& hdrInput, const ref<dx_texture>& ssr, const ref<dx_texture>& output)
{
	cl->setPipelineState(*specularAmbientPipeline.pipeline);
	cl->setComputeRootSignature(*specularAmbientPipeline.rootSignature);

	cl->setCompute32BitConstants(SPECULAR_AMBIENT_RS_CB, specular_ambient_cb{ vec2(1.f / renderWidth, 1.f / renderHeight) });
	cl->setComputeDynamicConstantBuffer(SPECULAR_AMBIENT_RS_CAMERA, cameraCBV);

	cl->setDescriptorHeapUAV(SPECULAR_AMBIENT_RS_TEXTURES, 0, output);
	cl->setDescriptorHeapSRV(SPECULAR_AMBIENT_RS_TEXTURES, 1, hdrInput);
	cl->setDescriptorHeapSRV(SPECULAR_AMBIENT_RS_TEXTURES, 2, worldNormalsTexture);
	cl->setDescriptorHeapSRV(SPECULAR_AMBIENT_RS_TEXTURES, 3, reflectanceTexture);
	cl->setDescriptorHeapSRV(SPECULAR_AMBIENT_RS_TEXTURES, 4, ssr ? ssr->defaultSRV : nullTextureSRV);
	cl->setDescriptorHeapSRV(SPECULAR_AMBIENT_RS_TEXTURES, 5, environment->environment);
	cl->setDescriptorHeapSRV(SPECULAR_AMBIENT_RS_TEXTURES, 6, brdfTex);

	cl->dispatch(bucketize(renderWidth, POST_PROCESSING_BLOCK_SIZE), bucketize(renderHeight, POST_PROCESSING_BLOCK_SIZE));
}

void dx_renderer::tonemapAndPresent(dx_command_list* cl, const ref<dx_texture>& hdrResult)
{
	tonemap(cl, hdrResult, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	present(cl);
}

void dx_renderer::tonemap(dx_command_list* cl, const ref<dx_texture>& hdrResult, D3D12_RESOURCE_STATES transitionLDR)
{
	DX_PROFILE_BLOCK(cl, "Tonemapping");

	cl->setPipelineState(*tonemapPipeline.pipeline);
	cl->setComputeRootSignature(*tonemapPipeline.rootSignature);

	cl->setDescriptorHeapUAV(TONEMAP_RS_TEXTURES, 0, ldrPostProcessingTexture);
	cl->setDescriptorHeapSRV(TONEMAP_RS_TEXTURES, 1, hdrResult);
	cl->setCompute32BitConstants(TONEMAP_RS_CB, settings.tonemap);

	cl->dispatch(bucketize(renderWidth, POST_PROCESSING_BLOCK_SIZE), bucketize(renderHeight, POST_PROCESSING_BLOCK_SIZE));

	barrier_batcher(cl)
		.uav(ldrPostProcessingTexture)
		.transition(ldrPostProcessingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, transitionLDR);
}

void dx_renderer::present(dx_command_list* cl)
{
	DX_PROFILE_BLOCK(cl, "Present");

	cl->setPipelineState(*presentPipeline.pipeline);
	cl->setComputeRootSignature(*presentPipeline.rootSignature);

	cl->setDescriptorHeapUAV(PRESENT_RS_TEXTURES, 0, frameResult);
	cl->setDescriptorHeapSRV(PRESENT_RS_TEXTURES, 1, ldrPostProcessingTexture);
	cl->setCompute32BitConstants(PRESENT_RS_CB, present_cb{ PRESENT_SDR, 0.f, settings.sharpenStrength * settings.enableSharpen, (windowXOffset << 16) | windowYOffset });

	cl->dispatch(bucketize(renderWidth, POST_PROCESSING_BLOCK_SIZE), bucketize(renderHeight, POST_PROCESSING_BLOCK_SIZE));
}

void dx_renderer::setCamera(const render_camera& camera)
{
	vec2 jitterOffset(0.f, 0.f);
	render_camera c;
	if (settings.enableTemporalAntialiasing)
	{
		jitterOffset = haltonSequence[dxContext.frameID % arraysize(haltonSequence)] / vec2((float)renderWidth, (float)renderHeight) * settings.cameraJitterStrength;
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

ref<dx_texture> dx_renderer::getWhiteTexture()
{
	return whiteTexture;
}

ref<dx_texture> dx_renderer::getBlackTexture()
{
	return blackTexture;
}

void dx_renderer::endFrame(const user_input& input)
{
	bool aspectRatioModeChanged = settings.aspectRatioMode != oldSettings.aspectRatioMode;
	oldSettings = settings;

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
		materialInfo.sky = blackCubeTexture;
		materialInfo.environment = blackCubeTexture;
		materialInfo.irradiance = blackCubeTexture;
	}
	materialInfo.environmentIntensity = settings.environmentIntensity;
	materialInfo.skyIntensity = settings.skyIntensity;
	materialInfo.brdf = brdfTex;
	materialInfo.tiledCullingGrid = tiledCullingGrid;
	materialInfo.tiledObjectsIndexList = tiledObjectsIndexList;
	materialInfo.pointLightBuffer = pointLights;
	materialInfo.spotLightBuffer = spotLights;
	materialInfo.decalBuffer = decals;
	materialInfo.shadowMap = shadowMap;
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

		if (numPointLights || numSpotLights || numDecals)
		{
			DX_PROFILE_BLOCK(cl, "Cull lights & decals");

			// Tiled frusta.
			{
				DX_PROFILE_BLOCK(cl, "Create world space frusta");

				cl->setPipelineState(*worldSpaceFrustaPipeline.pipeline);
				cl->setComputeRootSignature(*worldSpaceFrustaPipeline.rootSignature);
				cl->setComputeDynamicConstantBuffer(WORLD_SPACE_TILED_FRUSTA_RS_CAMERA, materialInfo.cameraCBV);
				cl->setCompute32BitConstants(WORLD_SPACE_TILED_FRUSTA_RS_CB, frusta_cb{ numCullingTilesX, numCullingTilesY });
				cl->setRootComputeUAV(WORLD_SPACE_TILED_FRUSTA_RS_FRUSTA_UAV, tiledWorldSpaceFrustaBuffer);
				cl->dispatch(bucketize(numCullingTilesX, 16), bucketize(numCullingTilesY, 16));
			}

			barrier_batcher(cl)
				.uav(tiledWorldSpaceFrustaBuffer);

			// Culling.
			{
				DX_PROFILE_BLOCK(cl, "Sort objects into tiles");

				cl->clearUAV(tiledCullingIndexCounter, 0.f);
				//cl->uavBarrier(tiledCullingIndexCounter);
				cl->setPipelineState(*lightCullingPipeline.pipeline);
				cl->setComputeRootSignature(*lightCullingPipeline.rootSignature);
				cl->setComputeDynamicConstantBuffer(LIGHT_CULLING_RS_CAMERA, materialInfo.cameraCBV);
				cl->setCompute32BitConstants(LIGHT_CULLING_RS_CB, light_culling_cb{ numCullingTilesX, numPointLights, numSpotLights, numDecals });
				cl->setDescriptorHeapSRV(LIGHT_CULLING_RS_SRV_UAV, 0, depthStencilBuffer);
				cl->setDescriptorHeapSRV(LIGHT_CULLING_RS_SRV_UAV, 1, tiledWorldSpaceFrustaBuffer);
				cl->setDescriptorHeapSRV(LIGHT_CULLING_RS_SRV_UAV, 2, pointLights ? pointLights->defaultSRV : nullBufferSRV);
				cl->setDescriptorHeapSRV(LIGHT_CULLING_RS_SRV_UAV, 3, spotLights ? spotLights->defaultSRV : nullBufferSRV);
				cl->setDescriptorHeapSRV(LIGHT_CULLING_RS_SRV_UAV, 4, decals ? decals->defaultSRV : nullBufferSRV);
				cl->setDescriptorHeapUAV(LIGHT_CULLING_RS_SRV_UAV, 5, tiledCullingGrid);
				cl->setDescriptorHeapUAV(LIGHT_CULLING_RS_SRV_UAV, 6, tiledCullingIndexCounter);
				cl->setDescriptorHeapUAV(LIGHT_CULLING_RS_SRV_UAV, 7, tiledObjectsIndexList);
				cl->dispatch(numCullingTilesX, numCullingTilesY);
			}

			barrier_batcher(cl)
				.uav(tiledCullingGrid)
				.uav(tiledObjectsIndexList);
		}


		// ----------------------------------------
		// LINEAR DEPTH PYRAMID
		// ----------------------------------------

		{
			DX_PROFILE_BLOCK(cl, "Linear depth pyramid");

			cl->setPipelineState(*hierarchicalLinearDepthPipeline.pipeline);
			cl->setComputeRootSignature(*hierarchicalLinearDepthPipeline.rootSignature);

			float width = ceilf(renderWidth * 0.5f);
			float height = ceilf(renderHeight * 0.5f);

			cl->setCompute32BitConstants(HIERARCHICAL_LINEAR_DEPTH_RS_CB, hierarchical_linear_depth_cb{ vec2(1.f / width, 1.f / height) });
			cl->setComputeDynamicConstantBuffer(HIERARCHICAL_LINEAR_DEPTH_RS_CAMERA, materialInfo.cameraCBV);
			cl->setDescriptorHeapUAV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 0, linearDepthBuffer->defaultUAV);
			cl->setDescriptorHeapUAV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 1, linearDepthBuffer->mipUAVs[0]);
			cl->setDescriptorHeapUAV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 2, linearDepthBuffer->mipUAVs[1]);
			cl->setDescriptorHeapUAV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 3, linearDepthBuffer->mipUAVs[2]);
			cl->setDescriptorHeapUAV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 4, linearDepthBuffer->mipUAVs[3]);
			cl->setDescriptorHeapUAV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 5, linearDepthBuffer->mipUAVs[4]);
			cl->setDescriptorHeapSRV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 6, depthStencilBuffer);

			cl->dispatch(bucketize((uint32)width, POST_PROCESSING_BLOCK_SIZE), bucketize((uint32)height, POST_PROCESSING_BLOCK_SIZE));

			barrier_batcher(cl)
				.uav(linearDepthBuffer)
				.transitionBegin(linearDepthBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}




		// ----------------------------------------
		// SHADOW MAP PASS
		// ----------------------------------------

		{
			DX_PROFILE_BLOCK(cl, "Shadow map pass");

			barrier_batcher(cl)
				.transition(shadowMap, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COPY_DEST);

			dx_render_target shadowRenderTarget({}, shadowMap);

			clear_rect clearRects[1024];
			uint32 numClearRects = 0;

			{
				DX_PROFILE_BLOCK(cl, "Copy from static shadow map cache");

				if (sunShadowRenderPass)
				{
					for (uint32 i = 0; i < sun.numShadowCascades; ++i)
					{
						shadow_map_viewport vp = sunShadowRenderPass->viewports[i];
						if (sunShadowRenderPass->copyFromStaticCache)
						{
							cl->copyTextureRegionToTexture(staticShadowMapCache, shadowMap, vp.x, vp.y, vp.x, vp.y, vp.size, vp.size);
						}
						else
						{
							clearRects[numClearRects++] = { vp.x, vp.y, vp.size, vp.size };
						}
					}
				}

				for (uint32 i = 0; i < numSpotLightShadowRenderPasses; ++i)
				{
					shadow_map_viewport vp = spotLightShadowRenderPasses[i]->viewport;
					if (spotLightShadowRenderPasses[i]->copyFromStaticCache)
					{
						cl->copyTextureRegionToTexture(staticShadowMapCache, shadowMap, vp.x, vp.y, vp.x, vp.y, vp.size, vp.size);
					}
					else
					{
						clearRects[numClearRects++] = { vp.x, vp.y, vp.size, vp.size };
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
							cl->copyTextureRegionToTexture(staticShadowMapCache, shadowMap, vp.x, vp.y, vp.x, vp.y, vp.size, vp.size);
						}
						else
						{
							clearRects[numClearRects++] = { vp.x, vp.y, vp.size, vp.size };
						}
					}
				}
			}

			barrier_batcher(cl)
				.transition(shadowMap, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_DEPTH_WRITE)
				.transitionBegin(staticShadowMapCache, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

			if (numClearRects)
			{
				cl->clearDepth(shadowRenderTarget, 1.f, clearRects, numClearRects);
			}

			cl->setPipelineState(*shadowPipeline.pipeline);
			cl->setGraphicsRootSignature(*shadowPipeline.rootSignature);

			cl->setRenderTarget(shadowRenderTarget);


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

			{
				DX_PROFILE_BLOCK(cl, "Sun static geometry");

				if (sunShadowRenderPass)
				{
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
			}

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

			cl->setPipelineState(*pointLightShadowPipeline.pipeline);
			cl->setGraphicsRootSignature(*pointLightShadowPipeline.rootSignature);

			{
				DX_PROFILE_BLOCK(cl, "Point lights static geometry");

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

			barrier_batcher(cl)
				.transition(shadowMap, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COPY_SOURCE)
				.transitionEnd(staticShadowMapCache, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

			{
				DX_PROFILE_BLOCK(cl, "Copy to static shadow map cache");

				if (sunShadowRenderPass)
				{
					if (sunShadowRenderPass->copyToStaticCache)
					{
						for (uint32 i = 0; i < sun.numShadowCascades; ++i)
						{
							shadow_map_viewport vp = sunShadowRenderPass->viewports[i];
							cl->copyTextureRegionToTexture(shadowMap, staticShadowMapCache, vp.x, vp.y, vp.x, vp.y, vp.size, vp.size);
						}
					}
				}

				for (uint32 i = 0; i < numSpotLightShadowRenderPasses; ++i)
				{
					shadow_map_viewport vp = spotLightShadowRenderPasses[i]->viewport;
					if (spotLightShadowRenderPasses[i]->copyToStaticCache)
					{
						cl->copyTextureRegionToTexture(shadowMap, staticShadowMapCache, vp.x, vp.y, vp.x, vp.y, vp.size, vp.size);
					}
				}

				for (uint32 i = 0; i < numPointLightShadowRenderPasses; ++i)
				{
					for (uint32 j = 0; j < 2; ++j)
					{
						shadow_map_viewport vp = (j == 0) ? pointLightShadowRenderPasses[i]->viewport0 : pointLightShadowRenderPasses[i]->viewport1;
						bool copy = (j == 0) ? pointLightShadowRenderPasses[i]->copyToStaticCache0 : pointLightShadowRenderPasses[i]->copyToStaticCache1;

						if (copy)
						{
							cl->copyTextureRegionToTexture(shadowMap, staticShadowMapCache, vp.x, vp.y, vp.x, vp.y, vp.size, vp.size);
						}
					}
				}
			}

			barrier_batcher(cl)
				.transition(shadowMap, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);

			cl->setPipelineState(*shadowPipeline.pipeline);
			cl->setGraphicsRootSignature(*shadowPipeline.rootSignature);

			{
				DX_PROFILE_BLOCK(cl, "Sun dynamic geometry");

				if (sunShadowRenderPass)
				{
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
			}

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

			cl->setPipelineState(*pointLightShadowPipeline.pipeline);
			cl->setGraphicsRootSignature(*pointLightShadowPipeline.rootSignature);

			{
				DX_PROFILE_BLOCK(cl, "Point lights dynamic geometry");

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
			.transition(shadowMap, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
			.transition(staticShadowMapCache, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);




		// ----------------------------------------
		// VOLUMETRICS
		// ----------------------------------------

#if 0
		cl->setPipelineState(*atmospherePipeline.pipeline);
		cl->setComputeRootSignature(*atmospherePipeline.rootSignature);
		cl->setComputeDynamicConstantBuffer(0, cameraCBV);
		cl->setComputeDynamicConstantBuffer(1, sunCBV);
		cl->setDescriptorHeapSRV(2, 0, depthBuffer);
		for (uint32 i = 0; i < MAX_NUM_SUN_SHADOW_CASCADES; ++i)
		{
			cl->setDescriptorHeapSRV(2, i + 1, sunShadowCascadeTextures[i]);
		}
		cl->setDescriptorHeapUAV(2, 5, volumetricsTexture);

		cl->dispatch(bucketize(renderWidth, 16), bucketize(renderHeight, 16));
#endif


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
				cl->setGraphics32BitConstants(SKY_RS_INTENSITY, sky_intensity_cb{ settings.skyIntensity });
				cl->setDescriptorHeapSRV(SKY_RS_TEX, 0, environment->sky->defaultSRV);

				cl->drawCubeTriangleStrip();
			}
			else
			{
				cl->setPipelineState(*proceduralSkyPipeline.pipeline);
				cl->setGraphicsRootSignature(*proceduralSkyPipeline.rootSignature);

				cl->setGraphics32BitConstants(SKY_RS_VP, sky_cb{ jitteredCamera.proj * createSkyViewMatrix(jitteredCamera.view) });
				cl->setGraphics32BitConstants(SKY_RS_INTENSITY, sky_intensity_cb{ settings.skyIntensity });

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

				cl->setVertexBuffer(0, dc.vertexBuffer);
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


		ref<dx_texture> hdrResult = hdrColorTexture;
		D3D12_RESOURCE_STATES hdrPostProcessingTextureState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		D3D12_RESOURCE_STATES hdrColorTextureState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;


		{
			DX_PROFILE_BLOCK(cl, "Copy depth buffer");

			cl->copyResource(depthStencilBuffer->resource, opaqueDepthBuffer->resource);
		}

		barrier_batcher(cl)
			.transitionBegin(opaqueDepthBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);



		// ----------------------------------------
		// SCREEN SPACE REFLECTIONS
		// ----------------------------------------

		if (settings.enableSSR)
		{
			DX_PROFILE_BLOCK(cl, "Screen space reflections");

			{
				DX_PROFILE_BLOCK(cl, "Raycast");

				cl->setPipelineState(*ssrRaycastPipeline.pipeline);
				cl->setComputeRootSignature(*ssrRaycastPipeline.rootSignature);

				settings.ssr.dimensions = vec2((float)ssrRaycastTexture->width, (float)ssrRaycastTexture->height);
				settings.ssr.invDimensions = vec2(1.f / ssrRaycastTexture->width, 1.f / ssrRaycastTexture->height);
				settings.ssr.frameIndex = (uint32)dxContext.frameID;

				cl->setCompute32BitConstants(SSR_RAYCAST_RS_CB, settings.ssr);
				cl->setComputeDynamicConstantBuffer(SSR_RAYCAST_RS_CAMERA, materialInfo.cameraCBV);
				cl->setDescriptorHeapUAV(SSR_RAYCAST_RS_TEXTURES, 0, ssrRaycastTexture);
				cl->setDescriptorHeapSRV(SSR_RAYCAST_RS_TEXTURES, 1, depthStencilBuffer);
				cl->setDescriptorHeapSRV(SSR_RAYCAST_RS_TEXTURES, 2, linearDepthBuffer);
				cl->setDescriptorHeapSRV(SSR_RAYCAST_RS_TEXTURES, 3, worldNormalsTexture);
				cl->setDescriptorHeapSRV(SSR_RAYCAST_RS_TEXTURES, 4, reflectanceTexture);
				cl->setDescriptorHeapSRV(SSR_RAYCAST_RS_TEXTURES, 5, noiseTexture);

				cl->dispatch(bucketize(SSR_RAYCAST_WIDTH, SSR_BLOCK_SIZE), bucketize(SSR_RAYCAST_HEIGHT, SSR_BLOCK_SIZE));

				barrier_batcher(cl)
					.uav(ssrRaycastTexture)
					.transition(ssrRaycastTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			}

			{
				DX_PROFILE_BLOCK(cl, "Resolve");

				cl->setPipelineState(*ssrResolvePipeline.pipeline);
				cl->setComputeRootSignature(*ssrResolvePipeline.rootSignature);

				cl->setCompute32BitConstants(SSR_RESOLVE_RS_CB, ssr_resolve_cb{ vec2((float)ssrResolveTexture->width, (float)ssrResolveTexture->height), vec2(1.f / ssrResolveTexture->width, 1.f / ssrResolveTexture->height) });
				cl->setComputeDynamicConstantBuffer(SSR_RESOLVE_RS_CAMERA, materialInfo.cameraCBV);

				cl->setDescriptorHeapUAV(SSR_RESOLVE_RS_TEXTURES, 0, ssrResolveTexture);
				cl->setDescriptorHeapSRV(SSR_RESOLVE_RS_TEXTURES, 1, depthStencilBuffer);
				cl->setDescriptorHeapSRV(SSR_RESOLVE_RS_TEXTURES, 2, worldNormalsTexture);
				cl->setDescriptorHeapSRV(SSR_RESOLVE_RS_TEXTURES, 3, reflectanceTexture);
				cl->setDescriptorHeapSRV(SSR_RESOLVE_RS_TEXTURES, 4, ssrRaycastTexture);
				cl->setDescriptorHeapSRV(SSR_RESOLVE_RS_TEXTURES, 5, prevFrameHDRColorTexture);
				cl->setDescriptorHeapSRV(SSR_RESOLVE_RS_TEXTURES, 6, screenVelocitiesTexture);

				cl->dispatch(bucketize(SSR_RESOLVE_WIDTH, SSR_BLOCK_SIZE), bucketize(SSR_RESOLVE_HEIGHT, SSR_BLOCK_SIZE));

				barrier_batcher(cl)
					.uav(ssrResolveTexture)
					.transition(ssrResolveTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
					.transitionBegin(ssrRaycastTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			}


			uint32 ssrOutputIndex = 1 - ssrHistoryIndex;

			{
				DX_PROFILE_BLOCK(cl, "Temporal");

				cl->setPipelineState(*ssrTemporalPipeline.pipeline);
				cl->setComputeRootSignature(*ssrTemporalPipeline.rootSignature);

				cl->setCompute32BitConstants(SSR_TEMPORAL_RS_CB, ssr_temporal_cb{ vec2(1.f / ssrResolveTexture->width, 1.f / ssrResolveTexture->height) });

				cl->setDescriptorHeapUAV(SSR_TEMPORAL_RS_TEXTURES, 0, ssrTemporalTextures[ssrOutputIndex]);
				cl->setDescriptorHeapSRV(SSR_TEMPORAL_RS_TEXTURES, 1, ssrResolveTexture);
				cl->setDescriptorHeapSRV(SSR_TEMPORAL_RS_TEXTURES, 2, ssrTemporalTextures[ssrHistoryIndex]);
				cl->setDescriptorHeapSRV(SSR_TEMPORAL_RS_TEXTURES, 3, screenVelocitiesTexture);

				cl->dispatch(bucketize(SSR_RESOLVE_WIDTH, SSR_BLOCK_SIZE), bucketize(SSR_RESOLVE_HEIGHT, SSR_BLOCK_SIZE));

				barrier_batcher(cl)
					.uav(ssrTemporalTextures[ssrOutputIndex])
					.transition(ssrTemporalTextures[ssrOutputIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
					.transitionBegin(ssrTemporalTextures[ssrHistoryIndex], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
					.transition(ssrResolveTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			}

			{
				DX_PROFILE_BLOCK(cl, "Median Blur");

				cl->setPipelineState(*ssrMedianBlurPipeline.pipeline);
				cl->setComputeRootSignature(*ssrMedianBlurPipeline.rootSignature);

				cl->setCompute32BitConstants(SSR_MEDIAN_BLUR_RS_CB, ssr_median_blur_cb{ vec2(1.f / ssrResolveTexture->width, 1.f / ssrResolveTexture->height) });

				cl->setDescriptorHeapUAV(SSR_MEDIAN_BLUR_RS_TEXTURES, 0, ssrResolveTexture); // We reuse the resolve texture here.
				cl->setDescriptorHeapSRV(SSR_MEDIAN_BLUR_RS_TEXTURES, 1, ssrTemporalTextures[ssrOutputIndex]);

				cl->dispatch(bucketize(SSR_RESOLVE_WIDTH, SSR_BLOCK_SIZE), bucketize(SSR_RESOLVE_HEIGHT, SSR_BLOCK_SIZE));

				barrier_batcher(cl)
					.uav(ssrResolveTexture)
					.transition(ssrResolveTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			}

			{
				DX_PROFILE_BLOCK(cl, "Combine");

				specularAmbient(cl, materialInfo.cameraCBV, hdrResult, ssrResolveTexture, hdrPostProcessingTexture);
			}

			barrier_batcher(cl)
				.uav(hdrPostProcessingTexture)
				.transition(hdrPostProcessingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE); // Will be read by rest of post processing stack. 

		}
		else
		{
			DX_PROFILE_BLOCK(cl, "Specular ambient");

			specularAmbient(cl, materialInfo.cameraCBV, hdrResult, 0, hdrPostProcessingTexture);

			barrier_batcher(cl)
				.uav(hdrPostProcessingTexture)
				.transition(hdrPostProcessingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE); // Will be read by rest of post processing stack. 

		}

		hdrResult = hdrPostProcessingTexture;
		hdrPostProcessingTextureState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;






		barrier_batcher(cl)
			.transitionEnd(opaqueDepthBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);



		// After this there is no more camera jittering!
		materialInfo.cameraCBV = unjitteredCameraCBV;
		materialInfo.opaqueDepth = opaqueDepthBuffer;
		materialInfo.worldNormals = worldNormalsTexture;





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

					cl->setVertexBuffer(0, dc.vertexBuffer);
					cl->setIndexBuffer(dc.indexBuffer);
					cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
				}
			}

			if (transparentRenderPass->particleDrawCalls.size() > 0)
			{
				cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

				DX_PROFILE_BLOCK(cl, "Particles");

				for (const auto& dc : transparentRenderPass->particleDrawCalls)
				{
					const mat4& m = dc.transform;

					if (dc.materialSetupFunc != lastSetupFunc)
					{
						dc.materialSetupFunc(cl, materialInfo);
						lastSetupFunc = dc.materialSetupFunc;
					}

					dc.material->prepareForRendering(cl);

					cl->setGraphics32BitConstants(0, unjitteredCamera.viewProj * m);

					cl->setVertexBuffer(0, dc.vertexBuffer);
					cl->setVertexBuffer(1, dc.instanceBuffer);
					cl->draw(4, dc.numParticles, 0, 0);
				}

				cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			}


			barrier_batcher(cl)
				.transition(hdrResult, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
		}



		if (settings.enableSSR)
		{
			barrier_batcher(cl)
				.transitionEnd(ssrRaycastTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS) // For next frame.
				.transitionEnd(ssrTemporalTextures[ssrHistoryIndex], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS) // For next frame.
				.transition(ssrResolveTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS); // For next frame.

			ssrHistoryIndex = 1 - ssrHistoryIndex;
		}





		// ----------------------------------------
		// POST PROCESSING
		// ----------------------------------------



		// TAA.
		if (settings.enableTemporalAntialiasing)
		{
			DX_PROFILE_BLOCK(cl, "Temporal anti-aliasing");

			uint32 taaOutputIndex = 1 - taaHistoryIndex;

			cl->setPipelineState(*taaPipeline.pipeline);
			cl->setComputeRootSignature(*taaPipeline.rootSignature);

			cl->setDescriptorHeapUAV(TAA_RS_TEXTURES, 0, taaTextures[taaOutputIndex]);
			cl->setDescriptorHeapSRV(TAA_RS_TEXTURES, 1, hdrResult);
			cl->setDescriptorHeapSRV(TAA_RS_TEXTURES, 2, taaTextures[taaHistoryIndex]);
			cl->setDescriptorHeapSRV(TAA_RS_TEXTURES, 3, screenVelocitiesTexture);
			cl->setDescriptorHeapSRV(TAA_RS_TEXTURES, 4, opaqueDepthBuffer);

			cl->setCompute32BitConstants(TAA_RS_CB, taa_cb{ jitteredCamera.projectionParams, vec2((float)renderWidth, (float)renderHeight) });

			cl->dispatch(bucketize(renderWidth, POST_PROCESSING_BLOCK_SIZE), bucketize(renderHeight, POST_PROCESSING_BLOCK_SIZE));

			hdrResult = taaTextures[taaOutputIndex];

			barrier_batcher(cl)
				.uav(taaTextures[taaOutputIndex])
				.transition(taaTextures[taaOutputIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) // Will be read by rest of post processing stack. Can stay in read state, since it is read as history next frame.
				.transition(taaTextures[taaHistoryIndex], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS); // Will be used as UAV next frame.

			taaHistoryIndex = taaOutputIndex;
		}

		// At this point hdrResult is either the TAA result, the hdrColorTexture, or the hdrPostProcessingTexture. All of these are in read state.




		// Downsample scene. This is also the copy used in SSR next frame.
		{
			DX_PROFILE_BLOCK(cl, "Downsample scene");

			barrier_batcher(cl)
				.transition(prevFrameHDRColorTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			cl->setPipelineState(*blitPipeline.pipeline);
			cl->setComputeRootSignature(*blitPipeline.rootSignature);

			cl->setCompute32BitConstants(BLIT_RS_CB, blit_cb{ vec2(1.f / prevFrameHDRColorTexture->width, 1.f / prevFrameHDRColorTexture->height) });
			cl->setDescriptorHeapUAV(BLIT_RS_TEXTURES, 0, prevFrameHDRColorTexture);
			cl->setDescriptorHeapSRV(BLIT_RS_TEXTURES, 1, hdrResult);

			cl->dispatch(bucketize(prevFrameHDRColorTexture->width, POST_PROCESSING_BLOCK_SIZE), bucketize(prevFrameHDRColorTexture->height, POST_PROCESSING_BLOCK_SIZE));

			barrier_batcher(cl)
				.uav(prevFrameHDRColorTexture)
				.transition(prevFrameHDRColorTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

			for (uint32 i = 0; i < prevFrameHDRColorTexture->numMipLevels - 1; ++i)
			{
				gaussianBlur(cl, prevFrameHDRColorTexture, prevFrameHDRColorTempTexture, i, i + 1, gaussian_blur_5x5);
			}
		}








		// Bloom.
		if (settings.enableBloom)
		{
			DX_PROFILE_BLOCK(cl, "Bloom");

			{
				DX_PROFILE_BLOCK(cl, "Threshold");

				cl->setPipelineState(*bloomThresholdPipeline.pipeline);
				cl->setComputeRootSignature(*bloomThresholdPipeline.rootSignature);

				cl->setDescriptorHeapUAV(BLOOM_THRESHOLD_RS_TEXTURES, 0, bloomTexture);
				cl->setDescriptorHeapSRV(BLOOM_THRESHOLD_RS_TEXTURES, 1, hdrResult);

				cl->setCompute32BitConstants(BLOOM_THRESHOLD_RS_CB, bloom_threshold_cb{ vec2(1.f / bloomTexture->width, 1.f / bloomTexture->height), settings.bloomThreshold });

				cl->dispatch(bucketize(bloomTexture->width, POST_PROCESSING_BLOCK_SIZE), bucketize(bloomTexture->height, POST_PROCESSING_BLOCK_SIZE));
			}

			ref<dx_texture> bloomResult = (hdrResult == hdrColorTexture) ? hdrPostProcessingTexture : hdrColorTexture;
			D3D12_RESOURCE_STATES& state = (hdrResult == hdrColorTexture) ? hdrPostProcessingTextureState : hdrColorTextureState;

			{
				barrier_batcher batcher(cl);
				batcher.uav(bloomTexture);
				batcher.transition(bloomTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

				if (state != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				{
					batcher.transition(bloomResult, state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				}
			}

			for (uint32 i = 0; i < bloomTexture->numMipLevels - 1; ++i)
			{
				gaussianBlur(cl, bloomTexture, bloomTempTexture, i, i + 1, gaussian_blur_9x9);
			}

			{
				DX_PROFILE_BLOCK(cl, "Combine");

				cl->setPipelineState(*bloomCombinePipeline.pipeline);
				cl->setComputeRootSignature(*bloomCombinePipeline.rootSignature);

				cl->setDescriptorHeapUAV(BLOOM_COMBINE_RS_TEXTURES, 0, bloomResult);
				cl->setDescriptorHeapSRV(BLOOM_COMBINE_RS_TEXTURES, 1, hdrResult);
				cl->setDescriptorHeapSRV(BLOOM_COMBINE_RS_TEXTURES, 2, bloomTexture);

				cl->setCompute32BitConstants(BLOOM_COMBINE_RS_CB, bloom_combine_cb{ vec2(1.f / renderWidth, 1.f / renderHeight), settings.bloomStrength });

				cl->dispatch(bucketize(renderWidth, POST_PROCESSING_BLOCK_SIZE), bucketize(renderHeight, POST_PROCESSING_BLOCK_SIZE));
			}

			hdrResult = bloomResult;

			barrier_batcher(cl)
				.uav(hdrResult)
				.transition(bloomResult, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) // Will be read by rest of post processing stack. 
				.transition(bloomTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS); // For next frame.

			state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}

		// At this point hdrResult is either the TAA result, the hdrColorTexture, or the hdrPostProcessingTexture. All of these are in read state.





		// ----------------------------------------
		// LDR RENDERING
		// ----------------------------------------



		bool renderingOverlays = overlayRenderPass && overlayRenderPass->drawCalls.size();
		bool renderingOutlines = opaqueRenderPass && opaqueRenderPass->outlinedObjects.size() > 0 ||
			transparentRenderPass && transparentRenderPass->outlinedObjects.size() > 0;
		bool renderingToLDRPostprocessingTexture = renderingOverlays || renderingOutlines;

		D3D12_RESOURCE_STATES ldrPostProcessingTextureState = renderingToLDRPostprocessingTexture ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		tonemap(cl, hdrResult, ldrPostProcessingTextureState);



		cl->transitionBarrier(depthStencilBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);



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

					cl->setVertexBuffer(0, dc.vertexBuffer);
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

					cl->setVertexBuffer(0, vertexBuffer);
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




		// TODO: If we really care we should sharpen before rendering overlays and outlines.

		present(cl);



		barrier_batcher(cl)
			.uav(frameResult)
			.transition(shadowMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE)
			.transition(hdrColorTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
			.transition(hdrPostProcessingTexture, hdrPostProcessingTextureState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS) // If texture is unused, this results in a NOP.
			.transition(worldNormalsTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
			.transition(screenVelocitiesTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
			.transitionEnd(objectIDsTexture, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
			.transition(ldrPostProcessingTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.transition(frameResult, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON)
			.transition(linearDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.transition(opaqueDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST)
			.transition(reflectanceTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

	}
	else if (dxContext.raytracingSupported && raytracer)
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
			.uav(hdrColorTexture)
			.transition(hdrColorTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);


		tonemapAndPresent(cl, hdrColorTexture);

		barrier_batcher(cl)
			.transition(hdrColorTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
			.transition(ldrPostProcessingTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.transition(frameResult, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
	}


	dxContext.executeCommandList(cl);
}
