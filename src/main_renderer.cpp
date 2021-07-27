#include "pch.h"
#include "main_renderer.h"
#include "dx_command_list.h"
#include "dx_render_target.h"
#include "dx_pipeline.h"
#include "dx_texture.h"
#include "dx_barrier_batcher.h"
#include "dx_profiling.h"
#include "render_utils.h"

#include "raytracing.h"


#define SSR_RAYCAST_WIDTH (renderWidth / 2)
#define SSR_RAYCAST_HEIGHT (renderHeight / 2)

#define SSR_RESOLVE_WIDTH (renderWidth / 2)
#define SSR_RESOLVE_HEIGHT (renderHeight / 2)



void main_renderer::initialize(uint32 windowWidth, uint32 windowHeight, bool renderObjectIDs)
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

void main_renderer::setCamera(const render_camera& camera)
{
	buildCameraConstantBuffer(camera, taaSettings.cameraJitterStrength * enableTAA, this->jitteredCamera);
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

	memcpy(sun.vp, light.vp, sizeof(mat4) * light.numShadowCascades);
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

	assignSunShadowMapViewports(sunShadowRenderPass, sun);

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

		waitForSkinningToFinish();


		// ----------------------------------------
		// DEPTH-ONLY PASS
		// ----------------------------------------

		dx_render_target depthOnlyRenderTarget({ screenVelocitiesTexture, objectIDsTexture }, depthStencilBuffer);
		depthPrePass(cl, depthOnlyRenderTarget, depthOnlyPipeline, animatedDepthOnlyPipeline, opaqueRenderPass,
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
			.transitionBegin(linearDepthBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);


		// ----------------------------------------
		// SHADOW MAP PASS
		// ----------------------------------------

		shadowPasses(cl, shadowPipeline, pointLightShadowPipeline,
			sunShadowRenderPass, sun,
			spotLightShadowRenderPasses, numSpotLightShadowRenderPasses,
			pointLightShadowRenderPasses, numPointLightShadowRenderPasses);




		barrier_batcher(cl)
			.transition(render_resources::shadowMap, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);




		// ----------------------------------------
		// SKY PASS
		// ----------------------------------------

		dx_render_target skyRenderTarget({ hdrColorTexture, screenVelocitiesTexture, objectIDsTexture }, depthStencilBuffer);
		if (environment)
		{
			texturedSky(cl, skyRenderTarget, textureSkyPipeline, jitteredCamera.proj, jitteredCamera.view, environment->sky, skyIntensity);
		}
		else
		{
			//proceduralSky(cl, skyRenderTarget, textureSkyPipeline, jitteredCamera.proj, jitteredCamera.view, skyIntensity);
			preethamSky(cl, skyRenderTarget, preethamSkyPipeline, jitteredCamera.proj, jitteredCamera.view, sun.direction, skyIntensity);
		}

		cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // Sky renders a triangle strip, so reset back to triangle list.



		// Copy hovered object id to readback buffer.
		if (objectIDsTexture)
		{
			DX_PROFILE_BLOCK(cl, "Copy hovered object ID");

			barrier_batcher(cl)
				.transition(objectIDsTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);

			if (input.overWindow 
				&& (input.mouse.x - (int32)windowXOffset >= 0)
				&& (input.mouse.x - (int32)windowXOffset < (int32)renderWidth)
				&& (input.mouse.y - (int32)windowYOffset >= 0)
				&& (input.mouse.y - (int32)windowYOffset < (int32)renderHeight))
			{
				cl->copyTextureRegionToBuffer(objectIDsTexture, hoveredObjectIDReadbackBuffer, dxContext.bufferedFrameID, (uint32)input.mouse.x - windowXOffset, (uint32)input.mouse.y - windowYOffset, 1, 1);
			}

			barrier_batcher(cl)
				.transitionBegin(objectIDsTexture, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		}



		// ----------------------------------------
		// OPAQUE LIGHT PASS
		// ----------------------------------------

		dx_render_target hdrOpaqueRenderTarget({ hdrColorTexture, worldNormalsTexture, reflectanceTexture }, depthStencilBuffer);
		opaqueLightPass(cl, hdrOpaqueRenderTarget, opaqueRenderPass, materialInfo, jitteredCamera.viewProj);


		{
			DX_PROFILE_BLOCK(cl, "Transition textures");

			barrier_batcher(cl)
				.transition(hdrColorTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ)
				.transition(worldNormalsTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.transition(screenVelocitiesTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
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


		// ----------------------------------------
		// SPECULAR AMBIENT
		// ----------------------------------------

		specularAmbient(cl, hdrColorTexture, enableSSR ? ssrResolveTexture : 0, worldNormalsTexture, reflectanceTexture, 
			environment ? environment->environment : 0, hdrPostProcessingTexture, materialInfo.cameraCBV);

		barrier_batcher(cl)
			//.uav(hdrPostProcessingTexture)
			.transition(hdrPostProcessingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) // Will be read by rest of post processing stack. 
			.transitionEnd(opaqueDepthBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		if (enableSSR)
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


		if (transparentRenderPass && (transparentRenderPass->drawCalls.size() > 0 || transparentRenderPass->particleDrawCalls.size() > 0))
		{
			barrier_batcher(cl)
				.transition(hdrResult, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
				.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

			dx_render_target hdrTransparentRenderTarget({ hdrResult }, depthStencilBuffer);

			transparentLightPass(cl, hdrTransparentRenderTarget, transparentRenderPass, materialInfo, particleCommandSignature, unjitteredCamera.viewProj);

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
		if (renderingOverlays || renderingOutlines)
		{
			barrier_batcher(cl)
				//.uav(ldrPostProcessingTexture)
				.transition(ldrPostProcessingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET)
				.transition(depthStencilBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

			dx_render_target ldrRenderTarget({ ldrPostProcessingTexture }, depthStencilBuffer);
			if (renderingOverlays)
			{
				overlays(cl, ldrRenderTarget, overlayRenderPass, materialInfo, unjitteredCamera.viewProj);
			}
			if (renderingOutlines)
			{
				outlines(cl, ldrRenderTarget, depthStencilBuffer, outlineMarkerPipeline, outlineDrawerPipeline, opaqueRenderPass, unjitteredCamera.viewProj, stencil_flag_selected_object);
			}

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
