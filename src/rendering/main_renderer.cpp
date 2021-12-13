#include "pch.h"
#include "main_renderer.h"
#include "dx/dx_command_list.h"
#include "dx/dx_render_target.h"
#include "dx/dx_pipeline.h"
#include "dx/dx_texture.h"
#include "dx/dx_barrier_batcher.h"
#include "dx/dx_profiling.h"

#include "raytracing.h"



#define SSR_RAYCAST_WIDTH (renderWidth / 2)
#define SSR_RAYCAST_HEIGHT (renderHeight / 2)

#define SSR_RESOLVE_WIDTH (renderWidth / 2)
#define SSR_RESOLVE_HEIGHT (renderHeight / 2)



void main_renderer::initialize(color_depth colorDepth, uint32 windowWidth, uint32 windowHeight, renderer_spec spec)
{
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;
	const_cast<renderer_spec&>(this->spec) = spec;

	settings.enableSharpen = spec.allowTAA;

	recalculateViewport(false);

	hdrColorTexture = createTexture(0, renderWidth, renderHeight, hdrFormat, false, true, true, D3D12_RESOURCE_STATE_RENDER_TARGET);
	SET_NAME(hdrColorTexture->resource, "HDR Color");

	if (spec.allowSSR)
	{
		D3D12_RESOURCE_DESC prevFrameHDRColorDesc = CD3DX12_RESOURCE_DESC::Tex2D(hdrFormat, renderWidth / 2, renderHeight / 2, 1,
			8, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		prevFrameHDRColorTexture = createTexture(prevFrameHDRColorDesc, 0, 0, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, true);
		prevFrameHDRColorTempTexture = createTexture(prevFrameHDRColorDesc, 0, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

		SET_NAME(prevFrameHDRColorTexture->resource, "Prev frame HDR Color");
		SET_NAME(prevFrameHDRColorTempTexture->resource, "Prev frame HDR Color Temp");
	}

	// If object picking is allowed, but none of the others, we still have to allocate screen velocities due to dumb internal reasons with our render targets.
	if (spec.allowSSR || spec.allowTAA || spec.allowAO || spec.allowObjectPicking)
	{
		screenVelocitiesTexture = createTexture(0, renderWidth, renderHeight, screenVelocitiesFormat, false, true, false, D3D12_RESOURCE_STATE_RENDER_TARGET);
		SET_NAME(screenVelocitiesTexture->resource, "Screen velocities");
	}


	worldNormalsTexture = createTexture(0, renderWidth, renderHeight, worldNormalsFormat, false, true, false, D3D12_RESOURCE_STATE_RENDER_TARGET);
	SET_NAME(worldNormalsTexture->resource, "World normals");


	reflectanceTexture = createTexture(0, renderWidth, renderHeight, reflectanceFormat, false, true, false, D3D12_RESOURCE_STATE_RENDER_TARGET);
	SET_NAME(reflectanceTexture->resource, "Reflectance");


	depthStencilBuffer = createDepthTexture(renderWidth, renderHeight, depthStencilFormat);
	opaqueDepthBuffer = createDepthTexture(renderWidth, renderHeight, depthStencilFormat, 1, D3D12_RESOURCE_STATE_COPY_DEST);
	D3D12_RESOURCE_DESC linearDepthDesc = CD3DX12_RESOURCE_DESC::Tex2D(linearDepthFormat, renderWidth, renderHeight, 1,
		6, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	linearDepthBuffer = createTexture(linearDepthDesc, 0, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
	SET_NAME(depthStencilBuffer->resource, "Depth buffer");
	SET_NAME(opaqueDepthBuffer->resource, "Opaque depth buffer");
	SET_NAME(linearDepthBuffer->resource, "Linear depth buffer");


	if (spec.allowAO)
	{
		aoCalculationTexture = createTexture(0, renderWidth / 2, renderHeight / 2, aoFormat, false, false, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		aoBlurTempTexture = createTexture(0, renderWidth, renderHeight, aoFormat, false, false, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		aoTextures[aoHistoryIndex] = createTexture(0, renderWidth, renderHeight, aoFormat, false, false, true, D3D12_RESOURCE_STATE_GENERIC_READ);
		aoTextures[1 - aoHistoryIndex] = createTexture(0, renderWidth, renderHeight, aoFormat, false, false, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		SET_NAME(aoCalculationTexture->resource, "AO Calculation");
		SET_NAME(aoBlurTempTexture->resource, "AO Temp");
		SET_NAME(aoTextures[0]->resource, "AO 0");
		SET_NAME(aoTextures[1]->resource, "AO 1");
	}

	if (spec.allowSSS)
	{
		sssCalculationTexture = createTexture(0, renderWidth / 2, renderHeight / 2, sssFormat, false, false, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		sssBlurTempTexture = createTexture(0, renderWidth, renderHeight, sssFormat, false, false, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		sssTextures[sssHistoryIndex] = createTexture(0, renderWidth, renderHeight, sssFormat, false, false, true, D3D12_RESOURCE_STATE_GENERIC_READ);
		sssTextures[1 - sssHistoryIndex] = createTexture(0, renderWidth, renderHeight, sssFormat, false, false, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		SET_NAME(sssCalculationTexture->resource, "SSS Calculation");
		SET_NAME(sssBlurTempTexture->resource, "SSS Temp");
		SET_NAME(sssTextures[0]->resource, "SSS 0");
		SET_NAME(sssTextures[1]->resource, "SSS 1");
	}

	if (spec.allowSSR)
	{
		ssrRaycastTexture = createTexture(0, SSR_RAYCAST_WIDTH, SSR_RAYCAST_HEIGHT, hdrFormat, false, false, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ssrResolveTexture = createTexture(0, SSR_RESOLVE_WIDTH, SSR_RESOLVE_HEIGHT, hdrFormat, false, false, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ssrTemporalTextures[ssrHistoryIndex] = createTexture(0, SSR_RESOLVE_WIDTH, SSR_RESOLVE_HEIGHT, hdrFormat, false, false, true, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		ssrTemporalTextures[1 - ssrHistoryIndex] = createTexture(0, SSR_RESOLVE_WIDTH, SSR_RESOLVE_HEIGHT, hdrFormat, false, false, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		SET_NAME(ssrRaycastTexture->resource, "SSR Raycast");
		SET_NAME(ssrResolveTexture->resource, "SSR Resolve");
		SET_NAME(ssrTemporalTextures[0]->resource, "SSR Temporal 0");
		SET_NAME(ssrTemporalTextures[1]->resource, "SSR Temporal 1");
	}


	hdrPostProcessingTexture = createTexture(0, renderWidth, renderHeight, hdrFormat, false, true, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	SET_NAME(hdrPostProcessingTexture->resource, "HDR Post processing");


	ldrPostProcessingTexture = createTexture(0, renderWidth, renderHeight, ldrFormat, false, true, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	SET_NAME(ldrPostProcessingTexture->resource, "LDR Post processing");


	if (spec.allowTAA)
	{
		taaTextures[taaHistoryIndex] = createTexture(0, renderWidth, renderHeight, hdrFormat, false, true, true, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		taaTextures[1 - taaHistoryIndex] = createTexture(0, renderWidth, renderHeight, hdrFormat, false, true, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		SET_NAME(taaTextures[0]->resource, "TAA 0");
		SET_NAME(taaTextures[1]->resource, "TAA 1");
	}

	if (spec.allowBloom)
	{
		D3D12_RESOURCE_DESC bloomDesc = CD3DX12_RESOURCE_DESC::Tex2D(hdrFormat, renderWidth / 4, renderHeight / 4, 1,
			5, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		bloomTexture = createTexture(bloomDesc, 0, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
		bloomTempTexture = createTexture(bloomDesc, 0, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

		SET_NAME(bloomTexture->resource, "Bloom");
		SET_NAME(bloomTempTexture->resource, "Bloom Temp");
	}


	frameResult = createTexture(0, windowWidth, windowHeight, colorDepthToFormat(colorDepth), false, true, true);
	SET_NAME(frameResult->resource, "Frame result");



	if (spec.allowObjectPicking)
	{
		hoveredObjectIDReadbackBuffer = createReadbackBuffer(getFormatSize(objectIDsFormat), NUM_BUFFERED_FRAMES);

		objectIDsTexture = createTexture(0, renderWidth, renderHeight, objectIDsFormat, false, true, false, D3D12_RESOURCE_STATE_RENDER_TARGET);
		SET_NAME(objectIDsTexture->resource, "Object IDs");
	}

	if (dxContext.featureSupport.raytracing())
	{
		pathTracer.initialize();
	}
}

void main_renderer::beginFrame(uint32 windowWidth, uint32 windowHeight)
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

	if (objectIDsTexture && windowHovered)
	{
		uint32* id = (uint32*)mapBuffer(hoveredObjectIDReadbackBuffer, true, map_range{ dxContext.bufferedFrameID, 1 });
		hoveredObjectID = *id;
		unmapBuffer(hoveredObjectIDReadbackBuffer, false);
	}
	else
	{
		hoveredObjectID = -1;
	}

	opaqueRenderPass = 0;
	transparentRenderPass = 0;
	ldrRenderPass = 0;

	pointLights = 0;
	spotLights = 0;
	numPointLights = 0;
	numSpotLights = 0;

	environment = 0;
}

void main_renderer::recalculateViewport(bool resizeTextures)
{
	if (aspectRatioMode == aspect_ratio_free)
	{
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
			windowXOffset = (int32)(windowWidth - renderWidth) / 2;
			windowYOffset = 0;
		}
		else
		{
			renderWidth = windowWidth;
			renderHeight = (uint32)(windowWidth / targetAspect);
			windowXOffset = 0;
			windowYOffset = (int32)(windowHeight - renderHeight) / 2;
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

		resizeTexture(aoCalculationTexture, renderWidth / 2, renderHeight / 2);
		resizeTexture(aoBlurTempTexture, renderWidth, renderHeight);
		resizeTexture(aoTextures[aoHistoryIndex], renderWidth, renderHeight, D3D12_RESOURCE_STATE_GENERIC_READ);
		resizeTexture(aoTextures[1 - aoHistoryIndex], renderWidth, renderHeight, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		resizeTexture(sssCalculationTexture, renderWidth / 2, renderHeight / 2);
		resizeTexture(sssBlurTempTexture, renderWidth, renderHeight);
		resizeTexture(sssTextures[sssHistoryIndex], renderWidth, renderHeight, D3D12_RESOURCE_STATE_GENERIC_READ);
		resizeTexture(sssTextures[1 - sssHistoryIndex], renderWidth, renderHeight, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		resizeTexture(objectIDsTexture, renderWidth, renderHeight);

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

void main_renderer::setCamera(const render_camera& camera)
{
	buildCameraConstantBuffer(camera, settings.taaSettings.cameraJitterStrength * settings.enableTAA, this->jitteredCamera);
	buildCameraConstantBuffer(camera, 0.f, this->unjitteredCamera);
}

void main_renderer::setEnvironment(const ref<pbr_environment>& environment)
{
	this->environment = environment;
}

void main_renderer::setSun(const directional_light& light)
{
	sun.cascadeDistances = light.cascadeDistances;
	sun.bias = light.bias;
	sun.direction = light.direction;
	sun.blendDistances = light.blendDistances;
	sun.radiance = light.color * light.intensity;
	sun.numShadowCascades = light.numShadowCascades;

	memcpy(sun.viewProjs, light.viewProjs, sizeof(mat4) * light.numShadowCascades);
	memcpy(sun.viewports, light.shadowMapViewports, sizeof(vec4) * light.numShadowCascades);
}

void main_renderer::setPointLights(const ref<dx_buffer>& lights, uint32 numLights, const ref<dx_buffer>& shadowInfoBuffer)
{
	pointLights = lights;
	numPointLights = numLights;
	pointLightShadowInfoBuffer = shadowInfoBuffer;
}

void main_renderer::setSpotLights(const ref<dx_buffer>& lights, uint32 numLights, const ref<dx_buffer>& shadowInfoBuffer)
{
	spotLights = lights;
	numSpotLights = numLights;
	spotLightShadowInfoBuffer = shadowInfoBuffer;
}

void main_renderer::setDecals(const ref<dx_buffer>& decals, uint32 numDecals, const ref<dx_texture>& textureAtlas)
{
	assert(numDecals < MAX_NUM_TOTAL_DECALS);
	this->decals = decals;
	this->numDecals = numDecals;
	this->decalTextureAtlas = textureAtlas;
}

void main_renderer::endFrame(const user_input& input)
{
	bool aspectRatioModeChanged = aspectRatioMode != oldAspectRatioMode;
	oldAspectRatioMode = aspectRatioMode;

	if (aspectRatioModeChanged)
	{
		recalculateViewport(true);
	}

	auto jitteredCameraCBV = dxContext.uploadDynamicConstantBuffer(jitteredCamera);
	auto unjitteredCameraCBV = dxContext.uploadDynamicConstantBuffer(unjitteredCamera);
	auto sunCBV = dxContext.uploadDynamicConstantBuffer(sun);


	settings.enableAO &= spec.allowAO;
	settings.enableSSS &= spec.allowSSS;
	settings.enableSSR &= spec.allowSSR;
	settings.enableBloom &= spec.allowBloom;
	settings.enableTAA &= spec.allowTAA;

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
	materialInfo.environmentIntensity = settings.environmentIntensity;
	materialInfo.skyIntensity = settings.skyIntensity;
	materialInfo.aoTexture = settings.enableAO ? aoTextures[1 - aoHistoryIndex] : render_resources::whiteTexture;
	materialInfo.sssTexture = settings.enableSSS ? sssTextures[1 - sssHistoryIndex] : render_resources::whiteTexture;
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



	if (mode == renderer_mode_rasterized)
	{
		dx_command_list* cls[3];


		thread_job_context context;

		context.addWork([&]()
		{
			dx_command_list* cl = dxContext.getFreeRenderCommandList();
			PROFILE_ALL(cl, "Render thread 1");

			if (aspectRatioModeChanged)
			{
				cl->transitionBarrier(frameResult, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
				cl->clearRTV(frameResult, 0.f, 0.f, 0.f);
				cl->transitionBarrier(frameResult, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
			}



			cl->clearDepthAndStencil(depthStencilBuffer);

			waitForSkinningToFinish();


			// ----------------------------------------
			// DEPTH-ONLY PASS
			// ----------------------------------------

			auto depthOnlyRenderTarget = dx_render_target(renderWidth, renderHeight)
				.colorAttachment(screenVelocitiesTexture, render_resources::nullScreenVelocitiesRTV)
				.colorAttachment(objectIDsTexture, render_resources::nullObjectIDsRTV)
				.depthAttachment(depthStencilBuffer);

			depthPrePass(cl, depthOnlyRenderTarget, opaqueRenderPass,
				jitteredCamera.viewProj, jitteredCamera.prevFrameViewProj, jitteredCamera.jitter, jitteredCamera.prevFrameJitter);


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
				.transition(linearDepthBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);

			cls[0] = cl;
		});

		context.addWork([&]()
		{
			dx_command_list* cl = dxContext.getFreeRenderCommandList();
			PROFILE_ALL(cl, "Render thread 2");


			// ----------------------------------------
			// SKY PASS
			// ----------------------------------------

			auto skyRenderTarget = dx_render_target(renderWidth, renderHeight)
				.colorAttachment(hdrColorTexture)
				.colorAttachment(screenVelocitiesTexture, render_resources::nullScreenVelocitiesRTV)
				.colorAttachment(objectIDsTexture, render_resources::nullObjectIDsRTV)
				.depthAttachment(depthStencilBuffer);

			if (environment)
			{
				texturedSky(cl, skyRenderTarget, jitteredCamera.proj, jitteredCamera.view, environment->sky, settings.skyIntensity);
			}
			else
			{
				//proceduralSky(cl, skyRenderTarget, textureSkyPipeline, jitteredCamera.proj, jitteredCamera.view, skyIntensity);
				preethamSky(cl, skyRenderTarget, jitteredCamera.proj, jitteredCamera.view, sun.direction, settings.skyIntensity);
			}



			// Copy hovered object id to readback buffer.
			if (objectIDsTexture)
			{
				windowHovered = input.overWindow
					&& (input.mouse.x - windowXOffset >= 0)
					&& (input.mouse.x - windowXOffset < (int32)renderWidth)
					&& (input.mouse.y - windowYOffset >= 0)
					&& (input.mouse.y - windowYOffset < (int32)renderHeight);

				if (windowHovered)
				{
					PROFILE_ALL(cl, "Copy hovered object ID");

					cl->transitionBarrier(objectIDsTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
					cl->copyTextureRegionToBuffer(objectIDsTexture, hoveredObjectIDReadbackBuffer, dxContext.bufferedFrameID, input.mouse.x - windowXOffset, input.mouse.y - windowYOffset, 1, 1);
					cl->transitionBarrier(objectIDsTexture, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
				}
			}


			barrier_batcher(cl)
				.transition(screenVelocitiesTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);


			// ----------------------------------------
			// AMBIENT OCCLUSION
			// ----------------------------------------

			if (settings.enableAO)
			{
				uint32 aoResultIndex = 1 - aoHistoryIndex;
				ref<dx_texture> aoHistory = aoWasOnLastFrame ? aoTextures[aoHistoryIndex] : render_resources::whiteTexture;
				ref<dx_texture> aoResult = aoTextures[aoResultIndex];
				ambientOcclusion(cl, linearDepthBuffer, screenVelocitiesTexture, aoCalculationTexture, aoBlurTempTexture, aoHistory, aoResult, settings.aoSettings, materialInfo.cameraCBV);

				barrier_batcher(cl)
					// UAV barrier is done by hbao-function.
					.transition(aoTextures[aoResultIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ) // Read by opaque pass and next frame as history.
					.transition(aoTextures[aoHistoryIndex], D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS); // For next frame.

				aoHistoryIndex = aoResultIndex;
			}


			// ----------------------------------------
			// SCREEN SPACE SHADOWS
			// ----------------------------------------

			if (settings.enableSSS)
			{
				uint32 sssResultIndex = 1 - sssHistoryIndex;
				ref<dx_texture> sssHistory = sssWasOnLastFrame ? sssTextures[sssHistoryIndex] : render_resources::whiteTexture;
				ref<dx_texture> sssResult = sssTextures[sssResultIndex];
				screenSpaceShadows(cl, linearDepthBuffer, screenVelocitiesTexture, sssCalculationTexture, sssBlurTempTexture, sssHistory, sssResult, sun.direction, settings.sssSettings, jitteredCamera.view, materialInfo.cameraCBV);

				barrier_batcher(cl)
					// UAV barrier is done by sss-function.
					.transition(sssTextures[sssResultIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ) // Read by opaque pass and next frame as history.
					.transition(sssTextures[sssHistoryIndex], D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS); // For next frame.

				sssHistoryIndex = sssResultIndex;
			}


			// ----------------------------------------
			// OPAQUE LIGHT PASS
			// ----------------------------------------

			auto hdrOpaqueRenderTarget = dx_render_target(renderWidth, renderHeight)
				.colorAttachment(hdrColorTexture)
				.colorAttachment(worldNormalsTexture)
				.colorAttachment(reflectanceTexture)
				.depthAttachment(depthStencilBuffer);

			opaqueLightPass(cl, hdrOpaqueRenderTarget, opaqueRenderPass, materialInfo, jitteredCamera.viewProj);


			barrier_batcher(cl)
				.transition(hdrColorTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ)
				.transition(worldNormalsTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.transition(reflectanceTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.transition(frameResult, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			{
				PROFILE_ALL(cl, "Copy depth buffer");
				cl->copyResource(depthStencilBuffer->resource, opaqueDepthBuffer->resource);
			}

			cls[1] = cl;
		});

		context.addWork([&]()
		{
			dx_command_list* cl = dxContext.getFreeRenderCommandList();
			PROFILE_ALL(cl, "Render thread 3");

			barrier_batcher(cl)
				.transitionBegin(opaqueDepthBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);



			// ----------------------------------------
			// SCREEN SPACE REFLECTIONS
			// ----------------------------------------

			if (settings.enableSSR)
			{
				screenSpaceReflections(cl, hdrColorTexture, prevFrameHDRColorTexture, depthStencilBuffer, linearDepthBuffer, worldNormalsTexture,
					reflectanceTexture, screenVelocitiesTexture, ssrRaycastTexture, ssrResolveTexture, ssrTemporalTextures[ssrHistoryIndex],
					ssrTemporalTextures[1 - ssrHistoryIndex], settings.ssrSettings, materialInfo.cameraCBV);

				ssrHistoryIndex = 1 - ssrHistoryIndex;
			}


			// ----------------------------------------
			// SPECULAR AMBIENT
			// ----------------------------------------

			specularAmbient(cl, hdrColorTexture, settings.enableSSR ? ssrResolveTexture : 0, worldNormalsTexture, reflectanceTexture,
				environment ? environment->environment : 0, materialInfo.aoTexture, hdrPostProcessingTexture, materialInfo.cameraCBV);

			barrier_batcher(cl)
				//.uav(hdrPostProcessingTexture)
				.transition(hdrPostProcessingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) // Will be read by rest of post processing stack. 
				.transitionEnd(opaqueDepthBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

			if (settings.enableSSR)
			{
				barrier_batcher(cl)
					.transition(ssrResolveTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS); // For next frame.
			}





			// After this there is no more camera jittering!
			materialInfo.cameraCBV = unjitteredCameraCBV;
			materialInfo.opaqueDepth = opaqueDepthBuffer;
			materialInfo.worldNormals = worldNormalsTexture;




			ref<dx_texture> hdrResult = hdrPostProcessingTexture; // Specular highlights have been rendered to this texture. It's in read state.


			// ----------------------------------------
			// TRANSPARENT LIGHT PASS
			// ----------------------------------------


			if (transparentRenderPass && transparentRenderPass->pass.size() > 0)
			{
				barrier_batcher(cl)
					.transition(hdrResult, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
					.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

				auto hdrTransparentRenderTarget = dx_render_target(renderWidth, renderHeight)
					.colorAttachment(hdrResult)
					.depthAttachment(depthStencilBuffer);

				transparentLightPass(cl, hdrTransparentRenderTarget, transparentRenderPass, materialInfo, unjitteredCamera.viewProj);

				barrier_batcher(cl)
					.transition(hdrResult, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
					.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
			}





			// ----------------------------------------
			// POST PROCESSING
			// ----------------------------------------


			// TAA.
			if (settings.enableTAA)
			{
				uint32 taaOutputIndex = 1 - taaHistoryIndex;
				temporalAntiAliasing(cl, hdrResult, screenVelocitiesTexture, opaqueDepthBuffer, taaTextures[taaHistoryIndex], taaTextures[taaOutputIndex], jitteredCamera.projectionParams);
				hdrResult = taaTextures[taaOutputIndex];
				taaHistoryIndex = taaOutputIndex;
			}

			// At this point hdrResult is either the TAA result or the hdrPostProcessingTexture. Either one is in read state.


			// Downsample scene. This is also the copy used in SSR next frame.
			if (prevFrameHDRColorTexture)
			{
				downsample(cl, hdrResult, prevFrameHDRColorTexture, prevFrameHDRColorTempTexture);
			}



			// Bloom.
			if (settings.enableBloom)
			{
				barrier_batcher(cl)
					.transition(hdrColorTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

				bloom(cl, hdrResult, hdrColorTexture, bloomTexture, bloomTempTexture, settings.bloomSettings);
				hdrResult = hdrColorTexture;
			}

			// At this point hdrResult is either the TAA result, the hdrColorTexture, or the hdrPostProcessingTexture. Either one is in read state.



			tonemap(cl, hdrResult, ldrPostProcessingTexture, settings.tonemapSettings);


			// ----------------------------------------
			// LDR RENDERING
			// ----------------------------------------

			bool renderingLDR = ldrRenderPass && ldrRenderPass->ldrPass.size();
			bool renderingOverlays = ldrRenderPass && ldrRenderPass->overlays.size();
			bool renderingOutlines = ldrRenderPass && ldrRenderPass->outlines.size();
			if (renderingLDR || renderingOverlays || renderingOutlines)
			{
				barrier_batcher(cl)
					//.uav(ldrPostProcessingTexture)
					.transition(ldrPostProcessingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET)
					.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

				auto ldrRenderTarget = dx_render_target(renderWidth, renderHeight)
					.colorAttachment(ldrPostProcessingTexture)
					.depthAttachment(depthStencilBuffer);

				ldrPass(cl, ldrRenderTarget, depthStencilBuffer, ldrRenderPass, materialInfo, unjitteredCamera.viewProj);

				barrier_batcher(cl)
					.transition(ldrPostProcessingTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			}
			else
			{
				barrier_batcher(cl)
					.transition(ldrPostProcessingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
					.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			}


			// TODO: If we really care we should sharpen before rendering overlays and outlines.

			present(cl, ldrPostProcessingTexture, frameResult, settings.enableSharpen ? settings.sharpenSettings : sharpen_settings{ 0.f });



			barrier_batcher(cl)
				//.uav(frameResult)
				.transition(hdrColorTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
				.transition(hdrPostProcessingTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				.transition(worldNormalsTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
				.transition(screenVelocitiesTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
				.transition(ldrPostProcessingTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				.transition(frameResult, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON)
				.transition(linearDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				.transition(opaqueDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST)
				.transition(reflectanceTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

			cls[2] = cl;
		});

		context.waitForWorkCompletion();

		dxContext.executeCommandLists(cls, arraysize(cls));
	}
	else if (mode == renderer_mode_visualize_sun_shadow_cascades)
	{
		dx_command_list* cl = dxContext.getFreeRenderCommandList();

		if (aspectRatioModeChanged)
		{
			cl->transitionBarrier(frameResult, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
			cl->clearRTV(frameResult, 0.f, 0.f, 0.f);
			cl->transitionBarrier(frameResult, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
		}

		cl->clearDepthAndStencil(depthStencilBuffer);

		waitForSkinningToFinish();

		auto depthOnlyRenderTarget = dx_render_target(renderWidth, renderHeight)
			// No screen space velocities or object IDs needed.
			.depthAttachment(depthStencilBuffer);

		depthPrePass(cl, depthOnlyRenderTarget, opaqueRenderPass,
			unjitteredCamera.viewProj, unjitteredCamera.prevFrameViewProj, unjitteredCamera.jitter, unjitteredCamera.prevFrameJitter);


		barrier_batcher(cl)
			.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			.transition(frameResult, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		visualizeSunShadowCascades(cl, depthStencilBuffer, frameResult, sunCBV, unjitteredCamera.invViewProj, unjitteredCamera.position.xyz, unjitteredCamera.forward.xyz);

		barrier_batcher(cl)
			//.uav(frameResult)
			.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE)
			.transition(frameResult, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);

		dxContext.executeCommandList(cl);
	}
	else if (mode == renderer_mode_pathtraced && dxContext.featureSupport.raytracing())
	{
		pathTracer.rebuildBindingTable();

		dx_command_list* cl = dxContext.getFreeRenderCommandList();


		D3D12_RESOURCE_STATES frameResultState = D3D12_RESOURCE_STATE_COMMON;

		if (aspectRatioModeChanged)
		{
			cl->transitionBarrier(frameResult, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
			cl->clearRTV(frameResult, 0.f, 0.f, 0.f);
			frameResultState = D3D12_RESOURCE_STATE_RENDER_TARGET;
		}

		barrier_batcher(cl)
			.transition(hdrColorTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.transitionEnd(frameResult, frameResultState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		{
			PROFILE_ALL(cl, "Raytracing");

			dxContext.renderQueue.waitForOtherQueue(dxContext.computeQueue); // Wait for AS-rebuilds. TODO: This is not the way to go here. We should wait for the specific value returned by executeCommandList.

			pathTracer.render(cl, *tlas, hdrColorTexture, materialInfo);
		}

		cl->resetToDynamicDescriptorHeap();

		barrier_batcher(cl)
			//.uav(hdrColorTexture)
			.transition(hdrColorTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		tonemap(cl, hdrColorTexture, ldrPostProcessingTexture, settings.tonemapSettings);

		barrier_batcher(cl)
			.transition(ldrPostProcessingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		present(cl, ldrPostProcessingTexture, frameResult, { 0.f });

		barrier_batcher(cl)
			.transition(hdrColorTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
			.transition(ldrPostProcessingTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.transition(frameResult, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);


		dxContext.executeCommandList(cl);
	}

	aoWasOnLastFrame = settings.enableAO;
	sssWasOnLastFrame = settings.enableSSS;
}
