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
#include "light_culling.hlsl"

#include "camera.hlsl"


dx_cbv_srv_uav_descriptor_heap dx_renderer::globalDescriptorHeap;
dx_cbv_srv_uav_descriptor_heap dx_renderer::globalDescriptorHeapCPU;



dx_dynamic_constant_buffer dx_renderer::cameraCBV;

dx_texture dx_renderer::whiteTexture;
dx_descriptor_handle dx_renderer::whiteTextureSRV;

dx_render_target dx_renderer::hdrRenderTarget;
dx_texture dx_renderer::hdrColorTexture;
dx_descriptor_handle dx_renderer::hdrColorTextureSRV;
dx_texture dx_renderer::depthBuffer;
dx_descriptor_handle dx_renderer::depthBufferSRV;

light_culling_buffers dx_renderer::lightCullingBuffers;

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
static dx_pipeline flatUnlitPipeline;

static dx_pipeline worldSpaceFrustaPipeline;
static dx_pipeline lightCullingPipeline;

static dx_mesh gizmoMesh;
static dx_mesh positionOnlyMesh;

static union
{
	struct
	{
		submesh_info noneGizmoSubmesh;
		submesh_info translationGizmoSubmesh;
		submesh_info rotationGizmoSubmesh;
		submesh_info scaleGizmoSubmesh;
	};

	submesh_info gizmoSubmeshes[4];
};

static submesh_info cubeMesh;
static submesh_info sphereMesh;


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

	globalDescriptorHeap = createDescriptorHeap(2048);
	globalDescriptorHeapCPU = createDescriptorHeap(2048, false);

	recalculateViewport(false);

	uint8 white[] = { 255, 255, 255, 255 };
	whiteTexture = createTexture(white, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
	whiteTextureSRV = globalDescriptorHeap.push2DTextureSRV(whiteTexture);


	initializeTexturePreprocessing();

	// HDR render target.
	{
		depthBuffer = createDepthTexture(renderWidth, renderHeight, DXGI_FORMAT_D24_UNORM_S8_UINT);
		depthBufferSRV = globalDescriptorHeap.pushDepthTextureSRV(depthBuffer);

		hdrColorTexture = createTexture(0, renderWidth, renderHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, true);
		hdrColorTextureSRV = globalDescriptorHeap.push2DTextureSRV(hdrColorTexture);

		hdrRenderTarget.pushColorAttachment(hdrColorTexture);
		hdrRenderTarget.pushDepthStencilAttachment(depthBuffer);
	}

	// Frame result.
	{
		frameResult = createTexture(0, windowWidth, windowHeight, DXGI_FORMAT_R8G8B8A8_UNORM, true);
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

	// Flat unlit.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position)
			.renderTargets(hdrRenderTarget.renderTargetFormat, hdrRenderTarget.depthStencilFormat)
			.wireframe();

		flatUnlitPipeline = createReloadablePipeline(desc, { "flat_unlit_vs", "flat_unlit_ps" }, "flat_unlit_vs");
	}

	// Present.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(windowRenderTarget.renderTargetFormat)
			.depthSettings(false, false);

		presentPipeline = createReloadablePipeline(desc, { "fullscreen_triangle_vs", "present_ps" });
	}

	// Light culling.
	{
		worldSpaceFrustaPipeline = createReloadablePipeline("world_space_tiled_frusta_cs");
		lightCullingPipeline = createReloadablePipeline("light_culling_cs");
	}

	createAllReloadablePipelines();





	camera.position = vec3(0.f, 3.f, 4.f);
	camera.rotation = quat(vec3(1.f, 0.f, 0.f), deg2rad(-20.f));
	camera.verticalFOV = deg2rad(70.f);
	camera.nearPlane = 0.1f;


	{
		cpu_mesh mesh(mesh_creation_flags_with_positions);
		cubeMesh = mesh.pushCube(1.f);
		sphereMesh = mesh.pushSphere(10, 10, 1.f);
		positionOnlyMesh = mesh.createDXMesh();
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
	//cpu_mesh m(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents);
	//mesh.singleMeshes.push_back({ m.pushQuad(10000.f) });
	//mesh.mesh = m.createDXMesh();

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

void dx_renderer::fillCameraConstantBuffer(camera_cb& cb)
{
	cb.vp = camera.viewProj;
	cb.v = camera.view;
	cb.p = camera.proj;
	cb.invVP = camera.invViewProj;
	cb.invV = camera.invView;
	cb.invP = camera.invProj;
	cb.position = vec4(camera.position, 1.f);
	cb.forward = vec4(camera.rotation * vec3(0.f, 0.f, -1.f), 0.f);
	cb.projectionParams = vec4(camera.nearPlane, camera.farPlane, camera.farPlane / camera.nearPlane, 1.f - camera.farPlane / camera.nearPlane);
	cb.screenDims = vec2((float)renderWidth, (float)renderHeight);
	cb.invScreenDims = vec2(1.f / renderWidth, 1.f / renderHeight);
}

void dx_renderer::beginFrame(uint32 windowWidth, uint32 windowHeight, float dt)
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

	camera_cb cameraCB;
	fillCameraConstantBuffer(cameraCB);
	cameraCBV = dxContext.uploadDynamicConstantBuffer(cameraCB);
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
		globalDescriptorHeap.createDepthTextureSRV(depthBuffer, depthBufferSRV);
		hdrRenderTarget.notifyOnTextureResize(renderWidth, renderHeight);
	}

	allocateLightCullingBuffers();
}

void dx_renderer::allocateLightCullingBuffers()
{
	dxContext.retireObject(lightCullingBuffers.tiledFrusta.resource);
	dxContext.retireObject(lightCullingBuffers.pointLightBoundingVolumes.resource);
	dxContext.retireObject(lightCullingBuffers.spotLightBoundingVolumes.resource);
	dxContext.retireObject(lightCullingBuffers.opaqueLightIndexCounter.resource);
	dxContext.retireObject(lightCullingBuffers.opaqueLightIndexList.resource);
	dxContext.retireObject(lightCullingBuffers.opaqueLightGrid.resource);

	lightCullingBuffers.numTilesX = bucketize(renderWidth, LIGHT_CULLING_TILE_SIZE);
	lightCullingBuffers.numTilesY = bucketize(renderHeight, LIGHT_CULLING_TILE_SIZE);

	bool firstAllocation = lightCullingBuffers.opaqueLightGrid.resource == nullptr;

	lightCullingBuffers.opaqueLightGrid = createTexture(0, lightCullingBuffers.numTilesX, lightCullingBuffers.numTilesY,
		DXGI_FORMAT_R32G32_UINT, false, true);
	lightCullingBuffers.opaqueLightIndexCounter = createBuffer(sizeof(uint32), 1, 0, true);
	lightCullingBuffers.opaqueLightIndexList = createBuffer(sizeof(uint32), 
		lightCullingBuffers.numTilesX * lightCullingBuffers.numTilesY * MAX_NUM_LIGHTS_PER_TILE, 0, true);
	lightCullingBuffers.tiledFrusta = createBuffer(sizeof(light_culling_view_frustum), lightCullingBuffers.numTilesX * lightCullingBuffers.numTilesY, 0, true);

	lightCullingBuffers.pointLightBoundingVolumes = createUploadBuffer(sizeof(point_light_bounding_volume), MAX_NUM_POINT_LIGHTS_PER_FRAME, 0); // TODO: Don't allocate here.
	lightCullingBuffers.spotLightBoundingVolumes = createUploadBuffer(sizeof(spot_light_bounding_volume), MAX_NUM_SPOT_LIGHTS_PER_FRAME, 0); // TODO: Don't allocate here.

	if (firstAllocation)
	{
		lightCullingBuffers.opaqueLightGridSRV = globalDescriptorHeap.push2DTextureSRV(lightCullingBuffers.opaqueLightGrid);
		globalDescriptorHeap.pushBufferSRV(lightCullingBuffers.opaqueLightIndexList);

		// SRVs.
		lightCullingBuffers.resourceHandle = globalDescriptorHeap.pushBufferSRV(lightCullingBuffers.pointLightBoundingVolumes);
		globalDescriptorHeap.pushBufferSRV(lightCullingBuffers.spotLightBoundingVolumes);
		globalDescriptorHeap.pushBufferSRV(lightCullingBuffers.tiledFrusta);

		// UAVs.
		globalDescriptorHeap.pushBufferUAV(lightCullingBuffers.opaqueLightIndexCounter);
		globalDescriptorHeap.pushBufferUAV(lightCullingBuffers.opaqueLightIndexList);
		globalDescriptorHeap.push2DTextureUAV(lightCullingBuffers.opaqueLightGrid);

		lightCullingBuffers.opaqueLightIndexCounterUAVGPU = globalDescriptorHeap.pushBufferUintUAV(lightCullingBuffers.opaqueLightIndexCounter);
		lightCullingBuffers.opaqueLightIndexCounterUAVCPU = globalDescriptorHeapCPU.pushBufferUintUAV(lightCullingBuffers.opaqueLightIndexCounter);
	}
	else
	{
		globalDescriptorHeap.create2DTextureSRV(lightCullingBuffers.opaqueLightGrid, lightCullingBuffers.opaqueLightGridSRV);
		globalDescriptorHeap.createBufferSRV(lightCullingBuffers.opaqueLightIndexList, globalDescriptorHeap.getOffsetted(lightCullingBuffers.opaqueLightGridSRV, 1));

		auto base = lightCullingBuffers.resourceHandle;
		globalDescriptorHeap.createBufferSRV(lightCullingBuffers.pointLightBoundingVolumes, globalDescriptorHeap.getOffsetted(base, 0));
		globalDescriptorHeap.createBufferSRV(lightCullingBuffers.spotLightBoundingVolumes, globalDescriptorHeap.getOffsetted(base, 1));
		globalDescriptorHeap.createBufferSRV(lightCullingBuffers.tiledFrusta, globalDescriptorHeap.getOffsetted(base, 2));

		globalDescriptorHeap.createBufferUAV(lightCullingBuffers.opaqueLightIndexCounter, globalDescriptorHeap.getOffsetted(base, 3));
		globalDescriptorHeap.createBufferUAV(lightCullingBuffers.opaqueLightIndexList, globalDescriptorHeap.getOffsetted(base, 4));
		globalDescriptorHeap.create2DTextureUAV(lightCullingBuffers.opaqueLightGrid, globalDescriptorHeap.getOffsetted(base, 5));

		globalDescriptorHeap.createBufferUintUAV(lightCullingBuffers.opaqueLightIndexCounter, lightCullingBuffers.opaqueLightIndexCounterUAVGPU);
		globalDescriptorHeapCPU.createBufferUintUAV(lightCullingBuffers.opaqueLightIndexCounter, lightCullingBuffers.opaqueLightIndexCounterUAVCPU);
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

	cl->setVertexBuffer(0, positionOnlyMesh.vertexBuffer);
	cl->setIndexBuffer(positionOnlyMesh.indexBuffer);
	cl->drawIndexed(cubeMesh.numTriangles * 3, 1, cubeMesh.firstTriangle * 3, cubeMesh.baseVertex, 0);



	auto submesh = mesh.singleMeshes[0].submesh;
	mat4 m = trsToMat4(meshTransform);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);



	// ----------------------------------------
	// DEPTH-ONLY PASS
	// ----------------------------------------

	// Models.
	cl->setPipelineState(*modelDepthOnlyPipeline.pipeline);
	cl->setGraphicsRootSignature(*modelDepthOnlyPipeline.rootSignature);
	cl->setGraphics32BitConstants(MODEL_RS_MVP, transform_cb{ camera.viewProj * m, m });

	cl->setVertexBuffer(0, mesh.mesh.vertexBuffer);
	cl->setIndexBuffer(mesh.mesh.indexBuffer);
	cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);

	// Gizmos.
	if (gizmoType != gizmo_type_none)
	{
		cl->setVertexBuffer(0, gizmoMesh.vertexBuffer);
		cl->setIndexBuffer(gizmoMesh.indexBuffer);

		for (uint32 i = 0; i < 3; ++i)
		{
			mat4 m = createModelMatrix(meshTransform.position, gizmoRotations[i]);
			cl->setGraphics32BitConstants(MODEL_RS_MVP, transform_cb{ camera.viewProj * m, m });
			cl->drawIndexed(gizmoSubmeshes[gizmoType].numTriangles * 3, 1, gizmoSubmeshes[gizmoType].firstTriangle * 3, gizmoSubmeshes[gizmoType].baseVertex, 0);
		}
	}



	// ----------------------------------------
	// LIGHT CULLING
	// ----------------------------------------

#if 1
	point_light_bounding_volume* pointLightBVs = (point_light_bounding_volume*)mapBuffer(lightCullingBuffers.pointLightBoundingVolumes);

	for (uint32 z = 0, i = 0; z < 8; ++z)
	{
		for (uint32 x = 0; x < 8; ++x, ++i)
		{
			pointLightBVs[i].position = (vec3((float)x, 0.f, (float)z) - vec3(4.f, 0.f, 4.f)) * 10.f;
			pointLightBVs[i].radius = 10.f;
		}
	}

	unmapBuffer(lightCullingBuffers.pointLightBoundingVolumes);


	// Tiled frusta.
	cl->setPipelineState(*worldSpaceFrustaPipeline.pipeline);
	cl->setComputeRootSignature(*worldSpaceFrustaPipeline.rootSignature);
	cl->setComputeDynamicConstantBuffer(WORLD_SPACE_TILED_FRUSTA_RS_CAMERA, cameraCBV);
	cl->setCompute32BitConstants(WORLD_SPACE_TILED_FRUSTA_RS_CB, frusta_cb{ lightCullingBuffers.numTilesX, lightCullingBuffers.numTilesY });
	cl->setComputeUAV(WORLD_SPACE_TILED_FRUSTA_RS_FRUSTA_UAV, lightCullingBuffers.tiledFrusta);
	cl->dispatch(bucketize(lightCullingBuffers.numTilesX, 16), bucketize(lightCullingBuffers.numTilesY, 16));

	// Light culling.
	cl->clearUAV(lightCullingBuffers.opaqueLightIndexCounter.resource, lightCullingBuffers.opaqueLightIndexCounterUAVCPU.cpuHandle, lightCullingBuffers.opaqueLightIndexCounterUAVGPU.gpuHandle, 0.f);
	cl->setPipelineState(*lightCullingPipeline.pipeline);
	cl->setComputeRootSignature(*lightCullingPipeline.rootSignature);
	cl->setComputeDynamicConstantBuffer(LIGHT_CULLING_RS_CAMERA, cameraCBV);
	cl->setCompute32BitConstants(LIGHT_CULLING_RS_CB, light_culling_cb{ lightCullingBuffers.numTilesX, 64, 0 }); // TODO: Number of lights.
	cl->setComputeDescriptorTable(LIGHT_CULLING_RS_DEPTH_BUFFER, depthBufferSRV);
	cl->setComputeDescriptorTable(LIGHT_CULLING_RS_SRV_UAV, lightCullingBuffers.resourceHandle);
	cl->dispatch(lightCullingBuffers.numTilesX, lightCullingBuffers.numTilesY);
#endif


	// ----------------------------------------
	// LIGHT PASS
	// ----------------------------------------


	// Models.
	cl->setPipelineState(*modelPipeline.pipeline);
	cl->setGraphicsRootSignature(*modelPipeline.rootSignature);
	cl->setGraphicsDescriptorTable(MODEL_RS_PBR_TEXTURES, textureSRV);
	cl->setGraphicsDescriptorTable(MODEL_RS_ENVIRONMENT_TEXTURES, environment.irradianceSRV);
	cl->setGraphicsDescriptorTable(MODEL_RS_BRDF, brdfTexSRV);
	cl->setGraphicsDescriptorTable(MODEL_RS_LIGHTS, lightCullingBuffers.opaqueLightGridSRV);
	cl->setGraphicsDynamicConstantBuffer(MODEL_RS_CAMERA, cameraCBV);
	cl->setGraphics32BitConstants(MODEL_RS_MATERIAL, pbr_material_cb{ vec4(1.f, 1.f, 1.f, 1.f), 0.f, 0.f, USE_ALBEDO_TEXTURE | USE_NORMAL_TEXTURE | USE_ROUGHNESS_TEXTURE | USE_METALLIC_TEXTURE });


	cl->setGraphics32BitConstants(MODEL_RS_MVP, transform_cb{ camera.viewProj * m, m });

	cl->setStencilReference(1);
	cl->setVertexBuffer(0, mesh.mesh.vertexBuffer);
	cl->setIndexBuffer(mesh.mesh.indexBuffer);
	cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
	cl->setStencilReference(0);


	// Gizmos.
	if (gizmoType != gizmo_type_none)
	{
		cl->setVertexBuffer(0, gizmoMesh.vertexBuffer);
		cl->setIndexBuffer(gizmoMesh.indexBuffer);

		for (uint32 i = 0; i < 3; ++i)
		{
			mat4 m = createModelMatrix(meshTransform.position, gizmoRotations[i]);
			cl->setGraphics32BitConstants(MODEL_RS_MVP, transform_cb{ camera.viewProj * m, m });
			cl->setGraphics32BitConstants(MODEL_RS_MATERIAL, pbr_material_cb{ gizmoColors[i], 1.f, 0.f, 0 });
			cl->drawIndexed(gizmoSubmeshes[gizmoType].numTriangles * 3, 1, gizmoSubmeshes[gizmoType].firstTriangle * 3, gizmoSubmeshes[gizmoType].baseVertex, 0);
		}
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


#if 0
	// Light volumes.
	cl->setPipelineState(*flatUnlitPipeline.pipeline);
	cl->setGraphicsRootSignature(*flatUnlitPipeline.rootSignature);
	cl->setVertexBuffer(0, positionOnlyMesh.vertexBuffer);
	cl->setIndexBuffer(positionOnlyMesh.indexBuffer);

	for (uint32 z = 0, i = 0; z < 8; ++z)
	{
		for (uint32 x = 0; x < 8; ++x, ++i)
		{
			float radius = 10.f;
			cl->setGraphics32BitConstants(0, camera.viewProj * createModelMatrix((vec3((float)x, 0.f, (float)z) - vec3(4.f, 0.f, 4.f)) * 10.f, quat::identity, vec3(radius, radius, radius)));
			cl->drawIndexed(sphereMesh.numTriangles * 3, 1, sphereMesh.firstTriangle * 3, sphereMesh.baseVertex, 0);
		}
	}
#endif


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
