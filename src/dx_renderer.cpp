#include "pch.h"
#include "dx_renderer.h"
#include "dx_command_list.h"
#include "dx_pipeline.h"
#include "camera.h"
#include "geometry.h"
#include "imgui.h"
#include "texture.h"
#include "mesh.h"
#include "random.h"
#include "texture_preprocessing.h"

#include "model_rs.hlsl"
#include "present_rs.hlsl"
#include "sky_rs.hlsl"

#include <iostream>


dx_cbv_srv_uav_descriptor_heap dx_renderer::globalDescriptorHeap;

dx_render_target dx_renderer::hdrRenderTarget;
dx_texture dx_renderer::hdrColorTexture;
dx_descriptor_handle dx_renderer::hdrColorTextureSRV;
dx_texture dx_renderer::depthBuffer;

uint32 dx_renderer::renderWidth;
uint32 dx_renderer::renderHeight;
uint32 dx_renderer::windowWidth;
uint32 dx_renderer::windowHeight;

dx_render_target dx_renderer::windowRenderTarget;
dx_descriptor_handle dx_renderer::frameResultSRV;
dx_texture dx_renderer::frameResult;

D3D12_VIEWPORT dx_renderer::windowViewport;

aspect_ratio_mode dx_renderer::aspectRatioMode;


static dx_pipeline textureSkyPipeline;
static dx_pipeline proceduralSkyPipeline;
static dx_pipeline presentPipeline;
static dx_pipeline modelPipeline;




// The following stuff will eventually move into a different file.

static render_camera camera;

static composite_mesh suzanne;
static dx_texture texture;
static dx_descriptor_handle textureHandle;
static trs* suzanneTransforms;
static mat4* suzanneModelMatrices;
static const uint32 numSuzannes = 1024;

static dx_mesh skyMesh;
static pbr_environment environment;


static tonemap_cb tonemap = defaultTonemapParameters();



void dx_renderer::initialize(uint32 windowWidth, uint32 windowHeight)
{
	dx_renderer::windowWidth = windowWidth;
	dx_renderer::windowHeight = windowHeight;

	recalculateViewport(false);

	globalDescriptorHeap = createDescriptorHeap(2048);

	initializeTexturePreprocessing();

	// Frame result.
	{
		frameResult = createTexture(0, windowWidth, windowHeight, DXGI_FORMAT_R8G8B8A8_UNORM, true);
		SET_NAME(frameResult.resource, "Frame result");
		frameResultSRV = globalDescriptorHeap.push2DTextureSRV(frameResult);

		windowRenderTarget.pushColorAttachment(frameResult);
	}

	// HDR render target.
	{
		depthBuffer = createDepthTexture(renderWidth, renderHeight, DXGI_FORMAT_D32_FLOAT);
		SET_NAME(depthBuffer.resource, "HDR depth buffer");

		hdrColorTexture = createTexture(0, renderWidth, renderHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, true);
		hdrColorTextureSRV = globalDescriptorHeap.push2DTextureSRV(hdrColorTexture);

		hdrRenderTarget.pushColorAttachment(hdrColorTexture);
		hdrRenderTarget.pushDepthStencilAttachment(depthBuffer);
	}


	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(windowRenderTarget.renderTargetFormat)
			.depthSettings(false, false);

		presentPipeline = createReloadablePipeline(desc, { "fullscreen_triangle_vs", "present_ps" });
	}

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position_uv_normal)
			.rasterizeCounterClockwise()
			.renderTargets(hdrRenderTarget.renderTargetFormat, hdrRenderTarget.depthStencilFormat);

		modelPipeline = createReloadablePipeline(desc, { "model_vs", "model_ps" });
	}

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position)
			.renderTargets(hdrRenderTarget.renderTargetFormat)
			.depthSettings(false, false);

		proceduralSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_procedural_ps" });
		textureSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_texture_ps" });
	}

	createAllReloadablePipelines();





	camera.position = vec3(0.f, 0.f, 4.f);
	camera.rotation = quat::identity;
	camera.verticalFOV = deg2rad(70.f);
	camera.nearPlane = 0.1f;

	{
		const aiScene* scene = loadAssimpSceneFile("assets/meshes/suzanne.fbx");
		suzanne = createCompositeMeshFromScene(scene);
		freeAssimpScene(scene);
	}

	{
		cpu_mesh mesh(mesh_creation_flags_with_positions);
		mesh.pushCube(1.f);
		skyMesh = mesh.createDXMesh();
	}

	random_number_generator rng = { 6718923 };

	suzanneTransforms = new trs[numSuzannes];
	suzanneModelMatrices = new mat4[numSuzannes];
	for (uint32 i = 0; i < numSuzannes; ++i)
	{
		suzanneTransforms[i].position.x = rng.randomFloatBetween(-30.f, 30.f);
		suzanneTransforms[i].position.y = rng.randomFloatBetween(-30.f, 30.f);
		suzanneTransforms[i].position.z = rng.randomFloatBetween(-30.f, 30.f);

		vec3 rotationAxis = normalize(vec3(rng.randomFloatBetween(-1.f, 1.f), rng.randomFloatBetween(-1.f, 1.f), rng.randomFloatBetween(-1.f, 1.f)));
		suzanneTransforms[i].rotation = quat(rotationAxis, rng.randomFloatBetween(0.f, 2.f * M_PI));

		suzanneTransforms[i].scale = rng.randomFloatBetween(0.2f, 3.f);
	}


	texture = loadTextureFromFile("assets/textures/hallo.png");
	textureHandle = globalDescriptorHeap.push2DTextureSRV(texture);

	{
		dx_command_list* cl = dxContext.getFreeRenderCommandList();
		dx_texture equiSky = loadTextureFromFile("assets/textures/hdri/aircraft_workshop_01_4k.hdr",
			texture_load_flags_noncolor | texture_load_flags_cache_to_dds | texture_load_flags_allocate_full_mipchain);
		dxContext.renderQueue.waitForOtherQueue(dxContext.copyQueue);
		generateMipMapsOnGPU(cl, equiSky);
		environment.sky = equirectangularToCubemap(cl, equiSky, 2048, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
		environment.prefiltered = prefilterEnvironment(cl, environment.sky, 128);
		environment.irradiance = cubemapToIrradiance(cl, environment.sky);
		dxContext.executeCommandList(cl);

		environment.skyHandle = globalDescriptorHeap.pushCubemapSRV(environment.sky);
		environment.prefilteredHandle = globalDescriptorHeap.pushCubemapSRV(environment.prefiltered);
		environment.irradianceHandle = globalDescriptorHeap.pushCubemapSRV(environment.irradiance);

		dxContext.retireObject(equiSky.resource);
	}
}

void dx_renderer::beginFrame(uint32 windowWidth, uint32 windowHeight)
{
	if (dx_renderer::windowWidth != windowWidth || dx_renderer::windowHeight != windowHeight)
	{
		dx_renderer::windowWidth = windowWidth;
		dx_renderer::windowHeight = windowHeight;

		// Frame result.
		{
			resizeTexture(frameResult, windowWidth, windowHeight);
			globalDescriptorHeap.create2DTextureSRV(frameResult, frameResultSRV);
			windowRenderTarget.notifyOnTextureResize(windowWidth, windowHeight);
		}

		recalculateViewport(true);
	}

	checkForChangedPipelines();

	camera.recalculateMatrices(renderWidth, renderHeight);
}

void dx_renderer::recalculateViewport(bool resizeTextures)
{
	if (aspectRatioMode == aspect_ratio_free)
	{
		windowViewport = { 0.f, 0.f, (float)windowWidth, (float)windowHeight, 0.f, 1.f };
	}
	else
	{
		const float targetAspect = aspectRatioMode == aspect_ratio_fix_16_9 ? (16.f / 9.f) : (16.f / 10.f);

		float aspect = (float)windowWidth / (float)windowHeight;
		if (aspect > targetAspect)
		{
			float width = windowHeight * targetAspect;
			float widthOffset = (windowWidth - width) * 0.5f;
			windowViewport = { widthOffset, 0.f, width, (float)windowHeight, 0.f, 1.f };
		}
		else
		{
			float height = windowWidth / targetAspect;
			float heightOffset = (windowHeight - height) * 0.5f;
			windowViewport = { 0.f, heightOffset, (float)windowWidth, height, 0.f, 1.f };
		}
	}

	renderWidth = (uint32)windowViewport.Width;
	renderHeight = (uint32)windowViewport.Height;

	if (resizeTextures)
	{
		resizeTexture(hdrColorTexture, renderWidth, renderHeight);
		resizeTexture(depthBuffer, renderWidth, renderHeight);
		globalDescriptorHeap.create2DTextureSRV(hdrColorTexture, hdrColorTextureSRV);
		hdrRenderTarget.notifyOnTextureResize(renderWidth, renderHeight);
	}
}

static float acesFilmic(float x, float A, float B, float C, float D, float E, float F)
{
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - (E / F);
}

static float filmicTonemapping(float color, tonemap_cb tonemap)
{
	float expExposure = exp2(tonemap.exposure);
	color *= expExposure;

	float r = acesFilmic(color, tonemap.A, tonemap.B, tonemap.C, tonemap.D, tonemap.E, tonemap.F) /
		acesFilmic(tonemap.linearWhite, tonemap.A, tonemap.B, tonemap.C, tonemap.D, tonemap.E, tonemap.F);

	return r;
}

void dx_renderer::dummyRender(float dt)
{
	static uint32 suzanneLOD = 0;
	static float suzanneSpeed = 1.f;

	DXGI_QUERY_VIDEO_MEMORY_INFO memoryInfo;
	checkResult(dxContext.adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memoryInfo));

	ImGui::Begin("Settings");
	ImGui::Text("%f ms, %u FPS", dt, (uint32)(1.f / dt));

	bool aspectRatioModeChanged = ImGui::Dropdown("Aspect Ratio", aspectRatioNames, aspect_ratio_mode_count, (uint32&)aspectRatioMode);

	ImGui::SliderInt("LOD", (int*)&suzanneLOD, 0, (int)suzanne.lods.size() - 1);
	ImGui::SliderFloat("Speed", &suzanneSpeed, -2.f, 2.f);
	ImGui::Text("Video memory available: %uMB", (uint32)BYTE_TO_MB(memoryInfo.Budget));
	ImGui::Text("Video memory used: %uMB", (uint32)BYTE_TO_MB(memoryInfo.CurrentUsage));

	ImGui::PlotLines("ACES",
		[](void* data, int idx)
		{
			float t = idx * 0.01f;
			tonemap_cb& aces = *(tonemap_cb*)data;

			return filmicTonemapping(t, aces);
		},
		&tonemap, 100,
			0, 0, 0.f, 1.f, ImVec2(100.f, 100.f));

	ImGui::SliderFloat("[ACES] Shoulder strength", &tonemap.A, 0.f, 1.f);
	ImGui::SliderFloat("[ACES] Linear strength", &tonemap.B, 0.f, 1.f);
	ImGui::SliderFloat("[ACES] Linear angle", &tonemap.C, 0.f, 1.f);
	ImGui::SliderFloat("[ACES] Toe strength", &tonemap.D, 0.f, 1.f);
	ImGui::SliderFloat("[ACES] Tone numerator", &tonemap.E, 0.f, 1.f);
	ImGui::SliderFloat("[ACES] Toe denominator", &tonemap.F, 0.f, 1.f);
	ImGui::SliderFloat("[ACES] Linear white", &tonemap.linearWhite, 0.f, 100.f);
	ImGui::SliderFloat("[ACES] Exposure", &tonemap.exposure, -3.f, 3.f);

	ImGui::End();

	if (aspectRatioModeChanged)
	{
		recalculateViewport(true);
	}


	quat suzanneDeltaRotation(vec3(0.f, 1.f, 0.f), 2.f * M_PI * 0.1f * suzanneSpeed * dt);
	for (uint32 i = 0; i < numSuzannes; ++i)
	{
		vec3 position = suzanneTransforms[i].position;
		position = suzanneDeltaRotation * position;

		suzanneTransforms[i].rotation = suzanneDeltaRotation * suzanneTransforms[i].rotation;
		suzanneTransforms[i].position = position;

		suzanneModelMatrices[i] = trsToMat4(suzanneTransforms[i]);
	}


	dx_command_list* cl = dxContext.getFreeRenderCommandList();

	CD3DX12_RECT scissorRect(0, 0, LONG_MAX, LONG_MAX);

	cl->setScissor(scissorRect);

	barrier_batcher(cl)
		.transition(hdrColorTexture.resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.transition(frameResult.resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cl->setDescriptorHeap(globalDescriptorHeap);


	cl->setRenderTarget(hdrRenderTarget);
	cl->setViewport(hdrRenderTarget.viewport);
	cl->clearDepth(hdrRenderTarget.dsvHandle);


	// Sky.
	cl->setPipelineState(*textureSkyPipeline.pipeline);
	cl->setGraphicsRootSignature(*textureSkyPipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cl->setGraphics32BitConstants(SKY_RS_VP, sky_cb{ camera.proj * createSkyViewMatrix(camera.view) });
	cl->setGraphicsDescriptorTable(SKY_RS_TEX, environment.skyHandle);

	cl->setVertexBuffer(0, skyMesh.vertexBuffer);
	cl->setIndexBuffer(skyMesh.indexBuffer);
	cl->drawIndexed(skyMesh.indexBuffer.elementCount, 1, 0, 0, 0);


	// Models.
	cl->setPipelineState(*modelPipeline.pipeline);
	cl->setGraphicsRootSignature(*modelPipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cl->setVertexBuffer(0, suzanne.mesh.vertexBuffer);
	cl->setIndexBuffer(suzanne.mesh.indexBuffer);
	cl->setGraphicsDescriptorTable(MODEL_RS_ALBEDO, textureHandle);

	for (uint32 i = 0; i < numSuzannes; ++i)
	{
		mat4& m = suzanneModelMatrices[i];
		cl->setGraphics32BitConstants(MODEL_RS_MVP, transform_cb{ camera.viewProj * m, m });
		auto submesh = suzanne.singleMeshes[suzanneLOD].submesh;
		cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
	}


	// Present.
	barrier_batcher(cl)
		.transition(hdrColorTexture.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	cl->setRenderTarget(windowRenderTarget);
	cl->setViewport(windowViewport);
	if (aspectRatioModeChanged)
	{
		cl->clearRTV(windowRenderTarget.rtvHandles[0], 0.f, 0.f, 0.f);
	}

	cl->setPipelineState(*presentPipeline.pipeline);
	cl->setGraphicsRootSignature(*presentPipeline.rootSignature);

	cl->setGraphics32BitConstants(PRESENT_RS_TONEMAP, tonemap);
	cl->setGraphics32BitConstants(PRESENT_RS_PRESENT, present_cb{ 0, 0.f });
	cl->setGraphicsDescriptorTable(PRESENT_RS_TEX, hdrColorTextureSRV);
	cl->drawFullscreenTriangle();


	barrier_batcher(cl)
		.transition(hdrColorTexture.resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON)
		.transition(frameResult.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);

	dxContext.executeCommandList(cl);
}
