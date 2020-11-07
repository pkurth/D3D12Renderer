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
#include "outline_rs.hlsl"
#include "present_rs.hlsl"
#include "sky_rs.hlsl"

#include <iostream>


dx_cbv_srv_uav_descriptor_heap dx_renderer::globalDescriptorHeap;

dx_texture dx_renderer::whiteTexture;
dx_descriptor_handle dx_renderer::whiteTextureSRV;

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
static dx_pipeline modelDepthOnlyPipeline;
static dx_pipeline outlinePipeline;

static dx_mesh skyMesh;
static dx_mesh gizmoMesh;

static union
{
	struct
	{
		submesh_info translationGizmoSubmesh;
		submesh_info rotationGizmoSubmesh;
		submesh_info scaleGizmoSubmesh;
	};

	submesh_info gizmoSubmeshes[3];
};

static quat gizmoRotations[] =
{
	quat(vec3(0.f, 0.f, -1.f), deg2rad(90.f)),
	quat::identity,
	quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f)),
};

static vec4 gizmoColors[] =
{
	vec4(1.f, 0.f, 0.f, 1.f),
	vec4(0.f, 1.f, 0.f, 1.f),
	vec4(0.f, 0.f, 1.f, 1.f),
};


static dx_texture brdfTex;
static dx_descriptor_handle brdfTexSRV;


// The following stuff will eventually move into a different file.

static render_camera camera;

static composite_mesh mesh;
static dx_texture albedoTex;
static dx_texture normalTex;
static dx_texture roughTex;
static dx_texture metalTex;
static dx_descriptor_handle textureSRV;

static trs meshTransform;


static pbr_environment environment;


static tonemap_cb tonemap = defaultTonemapParameters();



void dx_renderer::initialize(uint32 windowWidth, uint32 windowHeight)
{
	dx_renderer::windowWidth = windowWidth;
	dx_renderer::windowHeight = windowHeight;

	recalculateViewport(false);

	globalDescriptorHeap = createDescriptorHeap(2048);

	uint8 white[] = { 255, 255, 255, 255 };
	whiteTexture = createTexture(white, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
	whiteTextureSRV = globalDescriptorHeap.push2DTextureSRV(whiteTexture);


	initializeTexturePreprocessing();

	// HDR render target.
	{
		depthBuffer = createDepthTexture(renderWidth, renderHeight, DXGI_FORMAT_D24_UNORM_S8_UINT);
		SET_NAME(depthBuffer.resource, "HDR depth buffer");

		hdrColorTexture = createTexture(0, renderWidth, renderHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, true);
		hdrColorTextureSRV = globalDescriptorHeap.push2DTextureSRV(hdrColorTexture);

		hdrRenderTarget.pushColorAttachment(hdrColorTexture);
		hdrRenderTarget.pushDepthStencilAttachment(depthBuffer);
	}

	// Frame result.
	{
		frameResult = createTexture(0, windowWidth, windowHeight, DXGI_FORMAT_R8G8B8A8_UNORM, true);
		SET_NAME(frameResult.resource, "Frame result");
		frameResultSRV = globalDescriptorHeap.push2DTextureSRV(frameResult);

		windowRenderTarget.pushColorAttachment(frameResult);
	}

	// Sky.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position)
			.renderTargets(hdrRenderTarget.renderTargetFormat)
			.depthSettings(false, false)
			.cullFrontFaces();

		proceduralSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_procedural_ps" });
		textureSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_texture_ps" });
	}

	// Model.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(0, 0, hdrRenderTarget.depthStencilFormat)
			.inputLayout(inputLayout_position_uv_normal_tangent);

		modelDepthOnlyPipeline = createReloadablePipeline(desc, { "model_vs" }, "model_vs"); // The depth-only RS is baked into the vertex shader.


		desc
			.renderTargets(hdrRenderTarget.renderTargetFormat, hdrRenderTarget.depthStencilFormat)
			.stencilSettings(D3D12_COMPARISON_FUNC_ALWAYS, D3D12_STENCIL_OP_REPLACE, D3D12_STENCIL_OP_REPLACE) // Mark areas in stencil, for example for outline.
			.depthSettings(true, false, D3D12_COMPARISON_FUNC_EQUAL);

		modelPipeline = createReloadablePipeline(desc, { "model_vs", "model_ps" });
	}

	// Outline.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position_uv_normal_tangent)
			.renderTargets(hdrRenderTarget.renderTargetFormat, hdrRenderTarget.depthStencilFormat)
			.stencilSettings(D3D12_COMPARISON_FUNC_NOT_EQUAL);

		outlinePipeline = createReloadablePipeline(desc, { "outline_vs", "outline_ps" }, "outline_vs");
	}

	// Present.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(windowRenderTarget.renderTargetFormat)
			.depthSettings(false, false);

		presentPipeline = createReloadablePipeline(desc, { "fullscreen_triangle_vs", "present_ps" });
	}

	createAllReloadablePipelines();





	camera.position = vec3(0.f, 0.f, 4.f);
	camera.rotation = quat::identity;
	camera.verticalFOV = deg2rad(70.f);
	camera.nearPlane = 0.1f;

	{
		cpu_mesh mesh(mesh_creation_flags_with_positions);
		mesh.pushCube(1.f);
		skyMesh = mesh.createDXMesh();
	}

	{
		cpu_mesh mesh(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents);
		float shaftLength = 2.f;
		float headLength = 0.4f;
		float radius = 0.06f;
		float headRadius = 0.13f;
		translationGizmoSubmesh = mesh.pushArrow(6, radius, headRadius, shaftLength, headLength);
		rotationGizmoSubmesh = mesh.pushTorus(6, 64, shaftLength, radius);
		scaleGizmoSubmesh = mesh.pushMace(6, radius, headRadius, shaftLength, headLength);
		gizmoMesh = mesh.createDXMesh();
	}

	{
		dx_command_list* cl = dxContext.getFreeRenderCommandList();
		brdfTex = integrateBRDF(cl);
		brdfTexSRV = globalDescriptorHeap.push2DTextureSRV(brdfTex);
		dxContext.executeCommandList(cl);
	}

	mesh = createCompositeMeshFromFile("assets/meshes/cerberus.fbx", mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents);

	albedoTex = loadTextureFromFile("assets/textures/cerberus_a.tga");
	normalTex = loadTextureFromFile("assets/textures/cerberus_n.tga", texture_load_flags_default | texture_load_flags_noncolor);
	roughTex = loadTextureFromFile("assets/textures/cerberus_r.tga", texture_load_flags_default | texture_load_flags_noncolor);
	metalTex = loadTextureFromFile("assets/textures/cerberus_m.tga", texture_load_flags_default | texture_load_flags_noncolor);
	textureSRV = globalDescriptorHeap.push2DTextureSRV(albedoTex);
	globalDescriptorHeap.push2DTextureSRV(normalTex);
	globalDescriptorHeap.push2DTextureSRV(roughTex);
	globalDescriptorHeap.push2DTextureSRV(metalTex);

	meshTransform = trs::identity;
	meshTransform.rotation = quat(vec3(1.f, 0.f, 0.f), deg2rad(-90.f));
	meshTransform.scale = 0.04f;

	{
		dx_texture equiSky = loadTextureFromFile("assets/textures/hdri/leadenhall_market_4k.hdr",
			texture_load_flags_noncolor | texture_load_flags_cache_to_dds | texture_load_flags_allocate_full_mipchain);

		dxContext.renderQueue.waitForOtherQueue(dxContext.copyQueue);
		dx_command_list* cl = dxContext.getFreeRenderCommandList();
		generateMipMapsOnGPU(cl, equiSky);
		environment.sky = equirectangularToCubemap(cl, equiSky, 2048, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
		environment.prefiltered = prefilterEnvironment(cl, environment.sky, 128);
		environment.irradiance = cubemapToIrradiance(cl, environment.sky);
		dxContext.executeCommandList(cl);

		environment.skySRV = globalDescriptorHeap.pushCubemapSRV(environment.sky);
		environment.irradianceSRV = globalDescriptorHeap.pushCubemapSRV(environment.irradiance);
		environment.prefilteredSRV = globalDescriptorHeap.pushCubemapSRV(environment.prefiltered);

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

void dx_renderer::dummyRender(float dt)
{
	static float meshRotationSpeed = 1.f;
	static gizmo_type gizmoType;
	static bool showOutline = false;
	static vec4 outlineColor(1.f, 1.f, 0.f, 1.f);

	DXGI_QUERY_VIDEO_MEMORY_INFO memoryInfo;
	checkResult(dxContext.adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memoryInfo));

	ImGui::Begin("Settings");
	ImGui::Text("%f ms, %u FPS", dt, (uint32)(1.f / dt));

	bool aspectRatioModeChanged = ImGui::Dropdown("Aspect ratio", aspectRatioNames, aspect_ratio_mode_count, (uint32&)aspectRatioMode);
	ImGui::Dropdown("Gizmo type", gizmoTypeNames, gizmo_type_count, (uint32&)gizmoType);

	ImGui::Checkbox("Show outline", &showOutline);
	ImGui::ColorEdit4("Outline color", outlineColor.data);

	ImGui::SliderFloat("Rotation speed", &meshRotationSpeed, -2.f, 2.f);
	ImGui::Text("Video memory available: %uMB", (uint32)BYTE_TO_MB(memoryInfo.Budget));
	ImGui::Text("Video memory used: %uMB", (uint32)BYTE_TO_MB(memoryInfo.CurrentUsage));

	ImGui::PlotLines("Tone map",
		[](void* data, int idx)
		{
			float t = idx * 0.01f;
			tonemap_cb& aces = *(tonemap_cb*)data;

			return filmicTonemapping(t, aces);
		},
		&tonemap, 100, 0, 0, 0.f, 1.f, ImVec2(100.f, 100.f));

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


	quat deltaRotation(vec3(0.f, 1.f, 0.f), 2.f * M_PI * 0.1f * meshRotationSpeed * dt);
	meshTransform.rotation = deltaRotation * meshTransform.rotation;


	dx_command_list* cl = dxContext.getFreeRenderCommandList();

	CD3DX12_RECT scissorRect(0, 0, LONG_MAX, LONG_MAX);

	cl->setScissor(scissorRect);

	barrier_batcher(cl)
		.transition(hdrColorTexture.resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.transition(frameResult.resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cl->setDescriptorHeap(globalDescriptorHeap);


	cl->setRenderTarget(hdrRenderTarget);
	cl->setViewport(hdrRenderTarget.viewport);
	cl->clearDepthAndStencil(hdrRenderTarget.dsvHandle);


	// Sky.
	cl->setPipelineState(*textureSkyPipeline.pipeline);
	cl->setGraphicsRootSignature(*textureSkyPipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cl->setGraphics32BitConstants(SKY_RS_VP, sky_cb{ camera.proj * createSkyViewMatrix(camera.view) });
	cl->setGraphicsDescriptorTable(SKY_RS_TEX, environment.skySRV);

	cl->setVertexBuffer(0, skyMesh.vertexBuffer);
	cl->setIndexBuffer(skyMesh.indexBuffer);
	cl->drawIndexed(skyMesh.indexBuffer.elementCount, 1, 0, 0, 0);


	// Models.

	auto submesh = mesh.singleMeshes[0].submesh;
	mat4 m = trsToMat4(meshTransform);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cl->setVertexBuffer(0, mesh.mesh.vertexBuffer);
	cl->setIndexBuffer(mesh.mesh.indexBuffer);

	// Depth-only pass.
	cl->setPipelineState(*modelDepthOnlyPipeline.pipeline);
	cl->setGraphicsRootSignature(*modelDepthOnlyPipeline.rootSignature);
	cl->setGraphics32BitConstants(MODEL_RS_MVP, transform_cb{ camera.viewProj * m, m });
	cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);


	// Light pass.
	cl->setPipelineState(*modelPipeline.pipeline);
	cl->setGraphicsRootSignature(*modelPipeline.rootSignature);
	cl->setGraphicsDescriptorTable(MODEL_RS_PBR_TEXTURES, textureSRV);
	cl->setGraphicsDescriptorTable(MODEL_RS_ENVIRONMENT_TEXTURES, environment.irradianceSRV);
	cl->setGraphicsDescriptorTable(MODEL_RS_BRDF, brdfTexSRV);
	cl->setGraphics32BitConstants(MODEL_RS_MATERIAL, pbr_material_cb{ vec4(1.f, 1.f, 1.f, 1.f), 0.f, 0.f, USE_ALBEDO_TEXTURE | USE_NORMAL_TEXTURE | USE_ROUGHNESS_TEXTURE | USE_METALLIC_TEXTURE });

	cl->setGraphics32BitConstants(MODEL_RS_MVP, transform_cb{ camera.viewProj * m, m });

	cl->setStencilReference(1);
	cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);


	// Gizmos.
	cl->setVertexBuffer(0, gizmoMesh.vertexBuffer);
	cl->setIndexBuffer(gizmoMesh.indexBuffer);

	for (uint32 i = 0; i < 3; ++i)
	{
		mat4 m = createModelMatrix(meshTransform.position, gizmoRotations[i]);
		cl->setGraphics32BitConstants(MODEL_RS_MVP, transform_cb{ camera.viewProj * m, m });
		cl->setGraphics32BitConstants(MODEL_RS_MATERIAL, pbr_material_cb{ gizmoColors[i], 1.f, 0.f, 0 });
		cl->drawIndexed(gizmoSubmeshes[gizmoType].numTriangles * 3, 1, gizmoSubmeshes[gizmoType].firstTriangle * 3, gizmoSubmeshes[gizmoType].baseVertex, 0);
	}


	// Outline.
	if (showOutline)
	{
		cl->setPipelineState(*outlinePipeline.pipeline);
		cl->setGraphicsRootSignature(*outlinePipeline.rootSignature);
		cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cl->setVertexBuffer(0, mesh.mesh.vertexBuffer);
		cl->setIndexBuffer(mesh.mesh.indexBuffer);

		cl->setGraphics32BitConstants(OUTLINE_RS_MVP, outline_cb{ camera.viewProj * m, outlineColor });

		cl->setStencilReference(1);
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
