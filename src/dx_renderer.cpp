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
#include "perlin.h"

#include "model_rs.hlsl"
#include "outline_rs.hlsl"
#include "present_rs.hlsl"
#include "sky_rs.hlsl"
#include "light_culling.hlsl"
#include "light_source.hlsl"

#include "camera.hlsl"


dx_dynamic_constant_buffer dx_renderer::cameraCBV;

dx_texture dx_renderer::whiteTexture;

dx_render_target dx_renderer::hdrRenderTarget;
dx_texture dx_renderer::hdrColorTexture;
dx_texture dx_renderer::depthBuffer;

light_culling_buffers dx_renderer::lightCullingBuffers;
dx_buffer dx_renderer::pointLightBoundingVolumes[NUM_BUFFERED_FRAMES];
dx_buffer dx_renderer::spotLightBoundingVolumes[NUM_BUFFERED_FRAMES];

uint32 dx_renderer::renderWidth;
uint32 dx_renderer::renderHeight;
uint32 dx_renderer::windowWidth;
uint32 dx_renderer::windowHeight;

dx_render_target dx_renderer::windowRenderTarget;
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


// The following stuff will eventually move into a different file.

static render_camera camera;

static composite_mesh mesh;
static dx_texture albedoTex;
static dx_texture normalTex;
static dx_texture roughTex;
static dx_texture metalTex;

static trs meshTransform;

static const uint32 numLights = 4096;
static vec3* lightVelocities;
static point_light_cb* lights;


static pbr_environment environment;


static tonemap_cb tonemap = defaultTonemapParameters();


void dx_renderer::initialize(uint32 windowWidth, uint32 windowHeight)
{
	dx_renderer::windowWidth = windowWidth;
	dx_renderer::windowHeight = windowHeight;

	recalculateViewport(false);

	uint8 white[] = { 255, 255, 255, 255 };
	whiteTexture = createTexture(white, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

	for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; ++i)
	{
		pointLightBoundingVolumes[i] = createUploadBuffer(sizeof(point_light_cb), MAX_NUM_POINT_LIGHTS_PER_FRAME, 0);
		spotLightBoundingVolumes[i] = createUploadBuffer(sizeof(spot_light_cb), MAX_NUM_SPOT_LIGHTS_PER_FRAME, 0);
	}

	initializeTexturePreprocessing();

	// HDR render target.
	{
		depthBuffer = createDepthTexture(renderWidth, renderHeight, DXGI_FORMAT_D24_UNORM_S8_UINT);
		hdrColorTexture = createTexture(0, renderWidth, renderHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, true);

		hdrRenderTarget.pushColorAttachment(hdrColorTexture);
		hdrRenderTarget.pushDepthStencilAttachment(depthBuffer);
	}

	// Frame result.
	{
		frameResult = createTexture(0, windowWidth, windowHeight, DXGI_FORMAT_R8G8B8A8_UNORM, true);

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





	camera.position = vec3(0.f, 30.f, 40.f);
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
		dxContext.executeCommandList(cl);
	}

	//mesh = createCompositeMeshFromFile("assets/meshes/cerberus.fbx", mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents);
	cpu_mesh m(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents);
	mesh.singleMeshes.push_back({ m.pushQuad(10000.f) });
	mesh.mesh = m.createDXMesh();

	albedoTex = loadTextureFromFile("assets/textures/cerberus_a.tga");
	normalTex = loadTextureFromFile("assets/textures/cerberus_n.tga", texture_load_flags_default | texture_load_flags_noncolor);
	roughTex = loadTextureFromFile("assets/textures/cerberus_r.tga", texture_load_flags_default | texture_load_flags_noncolor);
	metalTex = loadTextureFromFile("assets/textures/cerberus_m.tga", texture_load_flags_default | texture_load_flags_noncolor);

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

		dxContext.retireObject(equiSky.resource);
	}

	lights = new point_light_cb[numLights];
	lightVelocities = new vec3[numLights];

	random_number_generator rng = { 14878213 };
	for (uint32 i = 0; i < numLights; ++i)
	{
		lights[i] = 
		{
			{
				rng.randomFloatBetween(-100.f, 100.f),
				0.f,
				rng.randomFloatBetween(-100.f, 100.f),
			}
			// Rest is set in update step.
		};

		lightVelocities[i] =
		{
			rng.randomFloatBetween(-1.f, 1.f),
			0.f,
			rng.randomFloatBetween(-1.f, 1.f),
		};
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
		hdrRenderTarget.notifyOnTextureResize(renderWidth, renderHeight);
	}

	allocateLightCullingBuffers();
}

void dx_renderer::allocateLightCullingBuffers()
{
	dxContext.retireObject(lightCullingBuffers.tiledFrusta.resource);
	dxContext.retireObject(lightCullingBuffers.opaqueLightIndexCounter.resource);
	dxContext.retireObject(lightCullingBuffers.opaqueLightIndexList.resource);
	dxContext.retireObject(lightCullingBuffers.opaqueLightGrid.resource);

	lightCullingBuffers.numTilesX = bucketize(renderWidth, LIGHT_CULLING_TILE_SIZE);
	lightCullingBuffers.numTilesY = bucketize(renderHeight, LIGHT_CULLING_TILE_SIZE);

	bool firstAllocation = lightCullingBuffers.opaqueLightGrid.resource == nullptr;

	if (firstAllocation)
	{
		lightCullingBuffers.opaqueLightGrid = createTexture(0, lightCullingBuffers.numTilesX, lightCullingBuffers.numTilesY,
			DXGI_FORMAT_R32G32_UINT, false, true);
		lightCullingBuffers.opaqueLightIndexCounter = createBuffer(sizeof(uint32), 1, 0, true, true);
		lightCullingBuffers.opaqueLightIndexList = createBuffer(sizeof(uint32),
			lightCullingBuffers.numTilesX * lightCullingBuffers.numTilesY * MAX_NUM_LIGHTS_PER_TILE, 0, true);
		lightCullingBuffers.tiledFrusta = createBuffer(sizeof(light_culling_view_frustum), lightCullingBuffers.numTilesX * lightCullingBuffers.numTilesY, 0, true);
	}
	else
	{
		resizeTexture(lightCullingBuffers.opaqueLightGrid, lightCullingBuffers.numTilesX, lightCullingBuffers.numTilesY);
		resizeBuffer(lightCullingBuffers.opaqueLightIndexList, lightCullingBuffers.numTilesX * lightCullingBuffers.numTilesY * MAX_NUM_LIGHTS_PER_TILE);
		resizeBuffer(lightCullingBuffers.tiledFrusta, lightCullingBuffers.numTilesX * lightCullingBuffers.numTilesY);
	}
}

void dx_renderer::dummyRender(float dt)
{
	static float meshRotationSpeed = 0.f;
	static gizmo_type gizmoType;
	static bool showOutline = false;
	static vec4 outlineColor(1.f, 1.f, 0.f, 1.f);
	static bool showLightVolumes = false;
	static float lightIntensity = 5.f;
	static float lightRadius = 10.f;


	DXGI_QUERY_VIDEO_MEMORY_INFO memoryInfo;
	checkResult(dxContext.adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memoryInfo));

	ImGui::Begin("Settings");
	ImGui::Text("%f ms, %u FPS", dt, (uint32)(1.f / dt));

	bool aspectRatioModeChanged = ImGui::Dropdown("Aspect ratio", aspectRatioNames, aspect_ratio_mode_count, (uint32&)aspectRatioMode);
	ImGui::Dropdown("Gizmo type", gizmoTypeNames, gizmo_type_count, (uint32&)gizmoType);

	ImGui::Checkbox("Show outline", &showOutline);
	ImGui::ColorEdit4("Outline color", outlineColor.data);

	ImGui::Checkbox("Show light volumes", &showLightVolumes);

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

	ImGui::SliderFloat("Light intensity", &lightIntensity, 0.1f, 400.f);
	ImGui::SliderFloat("Light radius", &lightRadius, 1.f, 20.f);

	ImGui::End();

	if (aspectRatioModeChanged)
	{
		recalculateViewport(true);
	}

	const vec3 vortexCenter(0.f, 0.f, 0.f);
	const float vortexSpeed = 1.f;
	const float vortexSize = 60.f;
	for (uint32 i = 0; i < numLights; ++i)
	{
		lightVelocities[i] *= (1.f - 0.005f);
		lights[i].position += lightVelocities[i] * dt;

		vec3 d = lights[i].position - vortexCenter;
		vec3 v = vec3(-d.z, 0.f, d.x) * vortexSpeed;
		float factor = 1.f / (1.f + (d.x * d.x + d.z * d.z) / vortexSize);

		lightVelocities[i] += (v - lightVelocities[i]) * factor;

		lights[i].radiance = vec3(1.f, 1.f, 1.f) * lightIntensity;
		lights[i].radius = lightRadius;
	}


	quat deltaRotation(vec3(0.f, 1.f, 0.f), 2.f * M_PI * 0.1f * meshRotationSpeed * dt);
	meshTransform.rotation = deltaRotation * meshTransform.rotation;


	dx_command_list* cl = dxContext.getFreeRenderCommandList();

	CD3DX12_RECT scissorRect(0, 0, LONG_MAX, LONG_MAX);

	cl->setScissor(scissorRect);

	barrier_batcher(cl)
		.transition(hdrColorTexture.resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.transition(frameResult.resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);

	//cl->setDescriptorHeap(globalDescriptorHeap);


	cl->setRenderTarget(hdrRenderTarget);
	cl->setViewport(hdrRenderTarget.viewport);
	cl->clearDepthAndStencil(hdrRenderTarget.dsvHandle);


	// Sky.
	cl->setPipelineState(*textureSkyPipeline.pipeline);
	cl->setGraphicsRootSignature(*textureSkyPipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cl->setGraphics32BitConstants(SKY_RS_VP, sky_cb{ camera.proj * createSkyViewMatrix(camera.view) });
	cl->setDescriptorHeapSRV(SKY_RS_TEX, 0, environment.sky);

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
	point_light_cb* pointLightBVs = (point_light_cb*)mapBuffer(pointLightBoundingVolumes[dxContext.bufferedFrameID]);
	memcpy(pointLightBVs, lights, sizeof(point_light_cb) * numLights);
	unmapBuffer(pointLightBoundingVolumes[dxContext.bufferedFrameID]);


	// Tiled frusta.
	cl->setPipelineState(*worldSpaceFrustaPipeline.pipeline);
	cl->setComputeRootSignature(*worldSpaceFrustaPipeline.rootSignature);
	cl->setComputeDynamicConstantBuffer(WORLD_SPACE_TILED_FRUSTA_RS_CAMERA, cameraCBV);
	cl->setCompute32BitConstants(WORLD_SPACE_TILED_FRUSTA_RS_CB, frusta_cb{ lightCullingBuffers.numTilesX, lightCullingBuffers.numTilesY });
	cl->setRootComputeUAV(WORLD_SPACE_TILED_FRUSTA_RS_FRUSTA_UAV, lightCullingBuffers.tiledFrusta);
	cl->dispatch(bucketize(lightCullingBuffers.numTilesX, 16), bucketize(lightCullingBuffers.numTilesY, 16));

	// Light culling.
	cl->clearUAV(lightCullingBuffers.opaqueLightIndexCounter, 0.f);
	cl->setPipelineState(*lightCullingPipeline.pipeline);
	cl->setComputeRootSignature(*lightCullingPipeline.rootSignature);
	cl->setComputeDynamicConstantBuffer(LIGHT_CULLING_RS_CAMERA, cameraCBV);
	cl->setCompute32BitConstants(LIGHT_CULLING_RS_CB, light_culling_cb{ lightCullingBuffers.numTilesX, numLights, 0 }); // TODO: Number of lights.
	cl->setDescriptorHeapSRV(LIGHT_CULLING_RS_SRV_UAV, 0, depthBuffer);
	cl->setDescriptorHeapSRV(LIGHT_CULLING_RS_SRV_UAV, 1, pointLightBoundingVolumes[dxContext.bufferedFrameID]);
	cl->setDescriptorHeapSRV(LIGHT_CULLING_RS_SRV_UAV, 2, spotLightBoundingVolumes[dxContext.bufferedFrameID]);
	cl->setDescriptorHeapSRV(LIGHT_CULLING_RS_SRV_UAV, 3, lightCullingBuffers.tiledFrusta);
	cl->setDescriptorHeapUAV(LIGHT_CULLING_RS_SRV_UAV, 4, lightCullingBuffers.opaqueLightIndexCounter);
	cl->setDescriptorHeapUAV(LIGHT_CULLING_RS_SRV_UAV, 5, lightCullingBuffers.opaqueLightIndexList);
	cl->setDescriptorHeapUAV(LIGHT_CULLING_RS_SRV_UAV, 6, lightCullingBuffers.opaqueLightGrid);
	cl->dispatch(lightCullingBuffers.numTilesX, lightCullingBuffers.numTilesY);
#endif


	// ----------------------------------------
	// LIGHT PASS
	// ----------------------------------------


	// Models.
	cl->setPipelineState(*modelPipeline.pipeline);
	cl->setGraphicsRootSignature(*modelPipeline.rootSignature);
	cl->setDescriptorHeapSRV(MODEL_RS_PBR_TEXTURES, 0, albedoTex);
	cl->setDescriptorHeapSRV(MODEL_RS_PBR_TEXTURES, 1, normalTex);
	cl->setDescriptorHeapSRV(MODEL_RS_PBR_TEXTURES, 2, roughTex);
	cl->setDescriptorHeapSRV(MODEL_RS_PBR_TEXTURES, 3, metalTex);
	cl->setDescriptorHeapSRV(MODEL_RS_ENVIRONMENT_TEXTURES, 0, environment.irradiance);
	cl->setDescriptorHeapSRV(MODEL_RS_ENVIRONMENT_TEXTURES, 1, environment.prefiltered);
	cl->setDescriptorHeapSRV(MODEL_RS_BRDF, 0, brdfTex);
	cl->setDescriptorHeapSRV(MODEL_RS_LIGHTS, 0, lightCullingBuffers.opaqueLightGrid);
	cl->setDescriptorHeapSRV(MODEL_RS_LIGHTS, 1, lightCullingBuffers.opaqueLightIndexList);
	cl->setDescriptorHeapSRV(MODEL_RS_LIGHTS, 2, pointLightBoundingVolumes[dxContext.bufferedFrameID]);
	cl->setDescriptorHeapSRV(MODEL_RS_LIGHTS, 3, spotLightBoundingVolumes[dxContext.bufferedFrameID]);

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


	if (showLightVolumes)
	{
		// Light volumes.
		cl->setPipelineState(*flatUnlitPipeline.pipeline);
		cl->setGraphicsRootSignature(*flatUnlitPipeline.rootSignature);
		cl->setVertexBuffer(0, positionOnlyMesh.vertexBuffer);
		cl->setIndexBuffer(positionOnlyMesh.indexBuffer);

		for (uint32 i = 0; i < numLights; ++i)
		{
			float radius = lights[i].radius;
			vec3 position = lights[i].position;
			cl->setGraphics32BitConstants(0, camera.viewProj * createModelMatrix(position, quat::identity, vec3(radius, radius, radius)));
			cl->drawIndexed(sphereMesh.numTriangles * 3, 1, sphereMesh.firstTriangle * 3, sphereMesh.baseVertex, 0);
		}
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
	cl->setDescriptorHeapSRV(PRESENT_RS_TEX, 0, hdrColorTexture);
	cl->drawFullscreenTriangle();


	barrier_batcher(cl)
		.transition(hdrColorTexture.resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON)
		.transition(frameResult.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);

	dxContext.executeCommandList(cl);
}
