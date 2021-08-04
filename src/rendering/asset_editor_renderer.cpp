#include "pch.h"
#include "asset_editor_renderer.h"
#include "render_utils.h"
#include "render_resources.h"
#include "dx/dx_barrier_batcher.h"
#include "dx/dx_profiling.h"

void asset_editor_renderer::initialize(uint32 renderWidth, uint32 renderHeight)
{
	this->renderWidth = renderWidth;
	this->renderHeight = renderHeight;

	hdrColorTexture = createTexture(0, renderWidth, renderHeight, hdrFormat, false, true, true, D3D12_RESOURCE_STATE_RENDER_TARGET);
	worldNormalsTexture = createTexture(0, renderWidth, renderHeight, worldNormalsFormat, false, true, false, D3D12_RESOURCE_STATE_RENDER_TARGET);
	reflectanceTexture = createTexture(0, renderWidth, renderHeight, reflectanceFormat, false, true, false, D3D12_RESOURCE_STATE_RENDER_TARGET);
	depthStencilBuffer = createDepthTexture(renderWidth, renderHeight, hdrDepthStencilFormat);

	hdrPostProcessingTexture = createTexture(0, renderWidth, renderHeight, hdrPostProcessFormat, false, true, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	ldrPostProcessingTexture = createTexture(0, renderWidth, renderHeight, ldrPostProcessFormat, false, true, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	frameResult = createTexture(0, renderWidth, renderHeight, outputFormat, false, true, true);



	SET_NAME(hdrColorTexture->resource, "HDR Color");
	SET_NAME(worldNormalsTexture->resource, "World normals");
	SET_NAME(reflectanceTexture->resource, "Reflectance");
	SET_NAME(depthStencilBuffer->resource, "Depth buffer");

	SET_NAME(hdrPostProcessingTexture->resource, "HDR Post processing");
	SET_NAME(ldrPostProcessingTexture->resource, "LDR Post processing");

	SET_NAME(frameResult->resource, "Frame result");
}

void asset_editor_renderer::beginFrame(uint32 renderWidth, uint32 renderHeight)
{
	if (this->renderWidth != renderWidth || this->renderHeight != renderHeight)
	{
		this->renderWidth = renderWidth;
		this->renderHeight = renderHeight;

		resizeTexture(frameResult, renderWidth, renderHeight);

		resizeTexture(hdrColorTexture, renderWidth, renderHeight);
		resizeTexture(worldNormalsTexture, renderWidth, renderHeight);
		resizeTexture(reflectanceTexture, renderWidth, renderHeight);
		resizeTexture(depthStencilBuffer, renderWidth, renderHeight);
		resizeTexture(hdrPostProcessingTexture, renderWidth, renderHeight);
		resizeTexture(ldrPostProcessingTexture, renderWidth, renderHeight);
	}


	opaqueRenderPass = 0;
	environment = 0;
}

void asset_editor_renderer::setCamera(const render_camera& camera)
{
	buildCameraConstantBuffer(camera, 0.f, this->camera);
}

void asset_editor_renderer::setEnvironment(const ref<pbr_environment>& environment)
{
	this->environment = environment;
}

void asset_editor_renderer::setSun(const directional_light& light)
{
	sun.direction = light.direction;
	sun.radiance = light.color * light.intensity;
	sun.numShadowCascades = 0;
}

void asset_editor_renderer::endFrame()
{
	auto cameraCBV = dxContext.uploadDynamicConstantBuffer(camera);
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
	materialInfo.tiledCullingGrid = 0;
	materialInfo.tiledObjectsIndexList = 0;
	materialInfo.pointLightBuffer = 0;
	materialInfo.spotLightBuffer = 0;
	materialInfo.decalBuffer = 0;
	materialInfo.shadowMap = render_resources::shadowMap;
	materialInfo.decalTextureAtlas = 0;
	materialInfo.pointLightShadowInfoBuffer = 0;
	materialInfo.spotLightShadowInfoBuffer = 0;
	materialInfo.volumetricsTexture = 0;
	materialInfo.cameraCBV = cameraCBV;
	materialInfo.sunCBV = sunCBV;


	dx_command_list* cl = dxContext.getFreeRenderCommandList();

	barrier_batcher(cl)
		.transitionBegin(frameResult, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		.transition(render_resources::shadowMap, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	cl->clearDepthAndStencil(depthStencilBuffer->dsvHandle);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	waitForSkinningToFinish();


	// ----------------------------------------
	// DEPTH-ONLY PASS
	// ----------------------------------------

	dx_render_target depthOnlyRenderTarget({  }, depthStencilBuffer);
	depthPrePass(cl, depthOnlyRenderTarget, depthOnlyPipeline, animatedDepthOnlyPipeline, opaqueRenderPass,
		camera.viewProj, camera.prevFrameViewProj, camera.jitter, camera.prevFrameJitter);



	// ----------------------------------------
	// SKY PASS
	// ----------------------------------------

	dx_render_target skyRenderTarget({ hdrColorTexture }, depthStencilBuffer);
	if (environment)
	{
		texturedSky(cl, skyRenderTarget, textureSkyPipeline, camera.proj, camera.view, environment->sky, skyIntensity);
	}
	else
	{
		proceduralSky(cl, skyRenderTarget, textureSkyPipeline, camera.proj, camera.view, skyIntensity);
	}

	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // Sky renders a triangle strip, so reset back to triangle list.



	// ----------------------------------------
	// OPAQUE LIGHT PASS
	// ----------------------------------------

	dx_render_target hdrOpaqueRenderTarget({ hdrColorTexture, worldNormalsTexture, reflectanceTexture }, depthStencilBuffer);
	opaqueLightPass(cl, hdrOpaqueRenderTarget, opaqueRenderPass, materialInfo, camera.viewProj);


	{
		DX_PROFILE_BLOCK(cl, "Transition textures");

		barrier_batcher(cl)
			.transition(hdrColorTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			.transition(worldNormalsTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			.transition(reflectanceTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			.transitionEnd(frameResult, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}


	specularAmbient(cl, hdrColorTexture, 0, worldNormalsTexture, reflectanceTexture,
		environment ? environment->environment : 0, hdrPostProcessingTexture, materialInfo.cameraCBV);

	barrier_batcher(cl)
		//.uav(hdrPostProcessingTexture)
		.transition(hdrPostProcessingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	tonemap(cl, hdrPostProcessingTexture, ldrPostProcessingTexture, tonemapSettings);

	barrier_batcher(cl)
		.transition(ldrPostProcessingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	present(cl, ldrPostProcessingTexture, frameResult, sharpen_settings{ 0.f });

	barrier_batcher(cl)
		//.uav(frameResult)
		.transition(hdrColorTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.transition(hdrPostProcessingTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		.transition(worldNormalsTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.transition(ldrPostProcessingTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		.transition(frameResult, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON)
		.transition(reflectanceTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.transition(render_resources::shadowMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);


	dxContext.executeCommandList(cl);
}
