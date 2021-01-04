#include "pch.h"
#include "dx_renderer.h"
#include "dx_command_list.h"
#include "dx_pipeline.h"
#include "geometry.h"
#include "dx_texture.h"
#include "dx_barrier_batcher.h"
#include "texture_preprocessing.h"
#include "skinning.h"
#include "dx_context.h"

#include "depth_only_rs.hlsli"
#include "outline_rs.hlsli"
#include "sky_rs.hlsli"
#include "light_culling_rs.hlsli"
#include "camera.hlsli"
#include "flat_simple_rs.hlsli"
#include "transform.hlsli"

#include "raytracing.h"
#include "pbr_raytracing_pipeline.h"


static ref<dx_texture> whiteTexture;
static ref<dx_texture> blackTexture;
static ref<dx_texture> blackCubeTexture;


// TODO: When using multiple renderers, these should not be here (I guess).
static ref<dx_buffer> pointLightBuffer[NUM_BUFFERED_FRAMES];
static ref<dx_buffer> spotLightBuffer[NUM_BUFFERED_FRAMES];


static dx_render_target sunShadowRenderTarget[MAX_NUM_SUN_SHADOW_CASCADES];
static ref<dx_texture> sunShadowCascadeTextures[MAX_NUM_SUN_SHADOW_CASCADES];

static dx_pipeline clearObjectIDsPipeline;

static dx_pipeline depthOnlyPipeline;
static dx_pipeline animatedDepthOnlyPipeline;
static dx_pipeline shadowPipeline;

static dx_pipeline textureSkyPipeline;
static dx_pipeline proceduralSkyPipeline;

static dx_pipeline blitPipeline;
static dx_pipeline presentPipeline;

static dx_pipeline flatUnlitPipeline;
static dx_pipeline flatSimplePipeline;


static dx_pipeline atmospherePipeline;

static dx_pipeline outlineMarkerPipeline;
static dx_pipeline outlineDrawerPipeline;

static dx_pipeline worldSpaceFrustaPipeline;
static dx_pipeline lightCullingPipeline;

static pbr_specular_reflections_raytracing_pipeline raytracingPipeline;



static dx_mesh positionOnlyMesh;

static submesh_info cubeMesh;
static submesh_info sphereMesh;




static ref<dx_texture> brdfTex;



DXGI_FORMAT dx_renderer::screenFormat;


static bool performedSkinning;


enum stencil_flags
{
	stencil_flag_selected_object = (1 << 0),
};


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

	initializeTexturePreprocessing();
	initializeSkinning();

	for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; ++i)
	{
		pointLightBuffer[i] = createUploadBuffer(sizeof(point_light_cb), MAX_NUM_POINT_LIGHTS_PER_FRAME, 0);
		spotLightBuffer[i] = createUploadBuffer(sizeof(spot_light_cb), MAX_NUM_SPOT_LIGHTS_PER_FRAME, 0);
	}

	for (uint32 i = 0; i < MAX_NUM_SUN_SHADOW_CASCADES; ++i)
	{
		sunShadowCascadeTextures[i] = createDepthTexture(SUN_SHADOW_DIMENSIONS, SUN_SHADOW_DIMENSIONS, shadowDepthFormat, 1, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		sunShadowRenderTarget[i].pushDepthStencilAttachment(sunShadowCascadeTextures[i]);
	}


	// Sky.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position)
			.renderTargets(hdrFormat[0])
			.depthSettings(false, false)
			.cullFrontFaces();

		proceduralSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_procedural_ps" });
		textureSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_texture_ps" });
	}

	// Depth prepass.
	{
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
			.cullFrontFaces();

		shadowPipeline = createReloadablePipeline(desc, { "shadow_vs" }, rs_in_vertex_shader);
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
			.renderTargets(hdrFormat[0], hdrDepthStencilFormat)
			.stencilSettings(D3D12_COMPARISON_FUNC_EQUAL,
				D3D12_STENCIL_OP_KEEP,
				D3D12_STENCIL_OP_KEEP,
				D3D12_STENCIL_OP_KEEP,
				stencil_flag_selected_object, // Read only selected object bit.
				0)
			.depthSettings(false, false);

		outlineDrawerPipeline = createReloadablePipeline(drawerDesc, { "fullscreen_triangle_vs", "outline_ps" });
	}

	// Flat unlit.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position)
			.renderTargets(hdrFormat[0], hdrDepthStencilFormat)
			.cullingOff()
			.wireframe();

		flatUnlitPipeline = createReloadablePipeline(desc, { "flat_unlit_vs", "flat_unlit_ps" }, rs_in_vertex_shader);
	}

	// Flat simple.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position_normal)
			.renderTargets(hdrFormat[0], hdrDepthStencilFormat)
			//.depthSettings(false)
			;

		flatSimplePipeline = createReloadablePipeline(desc, { "flat_simple_vs", "flat_simple_ps" });
	}

	// Present.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(&screenFormat, 1)
			.depthSettings(false, false);

		presentPipeline = createReloadablePipeline(desc, { "fullscreen_triangle_vs", "present_ps" });
	}

	// Blit.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(&screenFormat, 1)
			.depthSettings(false, false);

		blitPipeline = createReloadablePipeline(desc, { "fullscreen_triangle_vs", "blit_ps" });
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

	// Clear object IDs.
	{
		clearObjectIDsPipeline = createReloadablePipeline("clear_object_ids_cs");
	}

	// Raytracing.
	if (dxContext.raytracingSupported)
	{
		raytracingPipeline.initialize(L"shaders/raytracing/specular_reflections_rts.hlsl");
	}

	pbr_material::initializePipeline();

	createAllPendingReloadablePipelines();



	{
		cpu_mesh mesh(mesh_creation_flags_with_positions);
		cubeMesh = mesh.pushCube(1.f);
		sphereMesh = mesh.pushIcoSphere(1.f, 2);
		positionOnlyMesh = mesh.createDXMesh();
	}

	{
		dx_command_list* cl = dxContext.getFreeRenderCommandList();
		brdfTex = integrateBRDF(cl);
		dxContext.executeCommandList(cl);
	}
}

void dx_renderer::initialize(uint32 windowWidth, uint32 windowHeight, bool renderObjectIDs)
{
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;

	recalculateViewport(false);

	// HDR render target.
	{
		depthStencilBuffer = createDepthTexture(renderWidth, renderHeight, hdrDepthStencilFormat);
		hdrColorTexture = createTexture(0, renderWidth, renderHeight, hdrFormat[0], false, true);
		worldNormalsTexture = createTexture(0, renderWidth, renderHeight, hdrFormat[1], false, true);
		screenVelocitiesTexture = createTexture(0, renderWidth, renderHeight, depthOnlyFormat[0], false, true);

		SET_NAME(hdrColorTexture->resource, "HDR Color");
		SET_NAME(worldNormalsTexture->resource, "World normals");
		SET_NAME(screenVelocitiesTexture->resource, "Screen velocities");

		if (renderObjectIDs)
		{
			objectIDsTexture = createTexture(0, renderWidth, renderHeight, depthOnlyFormat[1], false, true, true);
			SET_NAME(objectIDsTexture->resource, "Object IDs");
		}

		hdrRenderTarget.pushColorAttachment(hdrColorTexture);
		hdrRenderTarget.pushColorAttachment(worldNormalsTexture);
		hdrRenderTarget.pushDepthStencilAttachment(depthStencilBuffer);

		depthOnlyRenderTarget.pushColorAttachment(screenVelocitiesTexture);

		if (renderObjectIDs)
		{
			depthOnlyRenderTarget.pushColorAttachment(objectIDsTexture);
		}
		depthOnlyRenderTarget.pushDepthStencilAttachment(depthStencilBuffer);
	}

	// Frame result.
	{
		frameResult = createTexture(0, windowWidth, windowHeight, screenFormat, false, true);
		SET_NAME(frameResult->resource, "Frame result");

		windowRenderTarget.pushColorAttachment(frameResult);
	}

	// Volumetrics.
	{
		volumetricsTexture = createTexture(0, renderWidth, renderHeight, volumetricsFormat, false, false, true);
		SET_NAME(volumetricsTexture->resource, "Volumetrics");
	}

	// Raytracing.
	if (dxContext.raytracingSupported)
	{
		raytracingTexture = createTexture(0, renderWidth / settings.raytracingDownsampleFactor, renderHeight / settings.raytracingDownsampleFactor, raytracedReflectionsFormat, false, false, true);
		raytracingTextureTmpForBlur = createTexture(0, renderWidth / settings.raytracingDownsampleFactor, renderHeight / settings.raytracingDownsampleFactor, raytracedReflectionsFormat, false, false, true);

		SET_NAME(raytracingTexture->resource, "Raytracing");
		SET_NAME(raytracingTextureTmpForBlur->resource, "Raytracing TMP");
	}

	if (renderObjectIDs)
	{
		for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; ++i)
		{
			hoveredObjectIDReadbackBuffer[i] = createReadbackBuffer(getFormatSize(depthOnlyFormat[1]), 1);
		}
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

	if (this->windowWidth != windowWidth || this->windowHeight != windowHeight)
	{
		this->windowWidth = windowWidth;
		this->windowHeight = windowHeight;

		// Frame result.
		{
			resizeTexture(frameResult, windowWidth, windowHeight);
			windowRenderTarget.notifyOnTextureResize(windowWidth, windowHeight);
		}

		recalculateViewport(true);
	}

	if (objectIDsTexture)
	{
		uint16* id = (uint16*)mapBuffer(hoveredObjectIDReadbackBuffer[dxContext.bufferedFrameID]);
		hoveredObjectID = *id;
		unmapBuffer(hoveredObjectIDReadbackBuffer[dxContext.bufferedFrameID]);
	}

	opaqueRenderPass.reset();
	transparentRenderPass.reset();
	sunShadowRenderPass.reset();
	visualizationRenderPass.reset();
	giRenderPass.reset();
}

void dx_renderer::recalculateViewport(bool resizeTextures)
{
	if (settings.aspectRatioMode == aspect_ratio_free)
	{
		windowViewport = { 0.f, 0.f, (float)windowWidth, (float)windowHeight, 0.f, 1.f };
	}
	else
	{
		const float targetAspect = settings.aspectRatioMode == aspect_ratio_fix_16_9 ? (16.f / 9.f) : (16.f / 10.f);

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
		resizeTexture(worldNormalsTexture, renderWidth, renderHeight);
		resizeTexture(depthStencilBuffer, renderWidth, renderHeight);
		hdrRenderTarget.notifyOnTextureResize(renderWidth, renderHeight);

		resizeTexture(screenVelocitiesTexture, renderWidth, renderHeight);
		if (objectIDsTexture)
		{
			resizeTexture(objectIDsTexture, renderWidth, renderHeight);
		}
		depthOnlyRenderTarget.notifyOnTextureResize(renderWidth, renderHeight);
		
		resizeTexture(volumetricsTexture, renderWidth, renderHeight);
		
		if (dxContext.raytracingSupported)
		{
			resizeTexture(raytracingTexture, renderWidth / settings.raytracingDownsampleFactor, renderHeight / settings.raytracingDownsampleFactor);
			resizeTexture(raytracingTextureTmpForBlur, renderWidth / settings.raytracingDownsampleFactor, renderHeight / settings.raytracingDownsampleFactor);
		}
	}

	allocateLightCullingBuffers();
}

void dx_renderer::allocateLightCullingBuffers()
{
	lightCullingBuffers.numTilesX = bucketize(renderWidth, LIGHT_CULLING_TILE_SIZE);
	lightCullingBuffers.numTilesY = bucketize(renderHeight, LIGHT_CULLING_TILE_SIZE);

	bool firstAllocation = lightCullingBuffers.lightGrid == nullptr;

	if (firstAllocation)
	{
		lightCullingBuffers.lightGrid = createTexture(0, lightCullingBuffers.numTilesX, lightCullingBuffers.numTilesY,
			DXGI_FORMAT_R32G32B32A32_UINT, false, false, true);
		SET_NAME(lightCullingBuffers.lightGrid->resource, "Light grid");

		lightCullingBuffers.lightIndexCounter = createBuffer(sizeof(uint32), 2, 0, true, true);
		lightCullingBuffers.pointLightIndexList = createBuffer(sizeof(uint32),
			lightCullingBuffers.numTilesX * lightCullingBuffers.numTilesY * MAX_NUM_LIGHTS_PER_TILE, 0, true);
		lightCullingBuffers.spotLightIndexList = createBuffer(sizeof(uint32),
			lightCullingBuffers.numTilesX * lightCullingBuffers.numTilesY * MAX_NUM_LIGHTS_PER_TILE, 0, true);
		lightCullingBuffers.tiledFrusta = createBuffer(sizeof(light_culling_view_frustum), lightCullingBuffers.numTilesX * lightCullingBuffers.numTilesY, 0, true);
	}
	else
	{
		resizeTexture(lightCullingBuffers.lightGrid, lightCullingBuffers.numTilesX, lightCullingBuffers.numTilesY);
		resizeBuffer(lightCullingBuffers.pointLightIndexList, lightCullingBuffers.numTilesX * lightCullingBuffers.numTilesY * MAX_NUM_LIGHTS_PER_TILE);
		resizeBuffer(lightCullingBuffers.spotLightIndexList, lightCullingBuffers.numTilesX * lightCullingBuffers.numTilesY * MAX_NUM_LIGHTS_PER_TILE);
		resizeBuffer(lightCullingBuffers.tiledFrusta, lightCullingBuffers.numTilesX * lightCullingBuffers.numTilesY);
	}
}

void dx_renderer::setCamera(const render_camera& camera)
{
	this->camera.prevFrameViewProj = this->camera.viewProj;
	this->camera.viewProj = camera.viewProj;
	this->camera.view = camera.view;
	this->camera.proj = camera.proj;
	this->camera.invViewProj = camera.invViewProj;
	this->camera.invView = camera.invView;
	this->camera.invProj = camera.invProj;
	this->camera.position = vec4(camera.position, 1.f);
	this->camera.forward = vec4(camera.rotation * vec3(0.f, 0.f, -1.f), 0.f);
	this->camera.projectionParams = vec4(camera.nearPlane, camera.farPlane, camera.farPlane / camera.nearPlane, 1.f - camera.farPlane / camera.nearPlane);
	this->camera.screenDims = vec2((float)renderWidth, (float)renderHeight);
	this->camera.invScreenDims = vec2(1.f / renderWidth, 1.f / renderHeight);
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
	sun.blendArea = light.blendArea;
	sun.radiance = light.color * light.intensity;
	sun.numShadowCascades = light.numShadowCascades;

	memcpy(sun.vp, light.vp, sizeof(mat4) * light.numShadowCascades);
}

void dx_renderer::setPointLights(const point_light_cb* lights, uint32 numLights)
{
	pointLights = lights;
	numPointLights = numLights;
}

void dx_renderer::setSpotLights(const spot_light_cb* lights, uint32 numLights)
{
	spotLights = lights;
	numSpotLights = numLights;
}

pbr_raytracing_pipeline* dx_renderer::getRaytracingPipeline()
{
	return &raytracingPipeline;
}

void dx_renderer::endFrame(const user_input& input)
{
	bool aspectRatioModeChanged = settings.aspectRatioMode != oldSettings.aspectRatioMode;

	if (aspectRatioModeChanged
		|| settings.raytracingDownsampleFactor != oldSettings.raytracingDownsampleFactor)
	{
		recalculateViewport(true);
	}


	auto cameraCBV = dxContext.uploadDynamicConstantBuffer(camera);
	auto sunCBV = dxContext.uploadDynamicConstantBuffer(sun);

	common_material_info materialInfo;
	if (environment)
	{
		materialInfo.environment = environment->environment;
		materialInfo.irradiance = environment->irradiance;
	}
	else
	{
		materialInfo.environment = blackCubeTexture;
		materialInfo.irradiance = blackCubeTexture;
	}
	materialInfo.environmentIntensity = settings.environmentIntensity;
	materialInfo.brdf = brdfTex;
	materialInfo.lightGrid = lightCullingBuffers.lightGrid;
	materialInfo.pointLightIndexList = lightCullingBuffers.pointLightIndexList;
	materialInfo.spotLightIndexList = lightCullingBuffers.spotLightIndexList;
	materialInfo.pointLightBuffer = pointLightBuffer[dxContext.bufferedFrameID];
	materialInfo.spotLightBuffer = spotLightBuffer[dxContext.bufferedFrameID];
	for (uint32 i = 0; i < MAX_NUM_SUN_SHADOW_CASCADES; ++i)
	{
		materialInfo.sunShadowCascades[i] = sunShadowCascadeTextures[i];
	}
	materialInfo.volumetricsTexture = volumetricsTexture;
	materialInfo.cameraCBV = cameraCBV;
	materialInfo.sunCBV = sunCBV;


	dx_command_list* cl = dxContext.getFreeRenderCommandList();

	barrier_batcher(cl)
		.transition(hdrColorTexture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.transition(worldNormalsTexture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.transition(screenVelocitiesTexture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.transition(objectIDsTexture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		.transition(frameResult, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET)
		.transition(sunShadowCascadeTextures, MAX_NUM_SUN_SHADOW_CASCADES, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);


	cl->clearDepthAndStencil(depthStencilBuffer->dsvHandle);

	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);



	// ----------------------------------------
	// CLEAR OBJECT IDS
	// ----------------------------------------

	if (objectIDsTexture)
	{
		cl->setPipelineState(*clearObjectIDsPipeline.pipeline);
		cl->setComputeRootSignature(*clearObjectIDsPipeline.rootSignature);

		struct clear_ids_cb { uint32 width, height; };
		cl->setCompute32BitConstants(0, clear_ids_cb{ objectIDsTexture->width, objectIDsTexture->height });
		cl->setDescriptorHeapUAV(1, 0, objectIDsTexture);
		cl->dispatch(bucketize(objectIDsTexture->width, 16), bucketize(objectIDsTexture->height, 16));

		barrier_batcher(cl)
			.transition(objectIDsTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}





	cl->setRenderTarget(hdrRenderTarget);
	cl->setViewport(hdrRenderTarget.viewport);

	// ----------------------------------------
	// SKY PASS
	// ----------------------------------------

	if (environment)
	{
		cl->setPipelineState(*textureSkyPipeline.pipeline);
		cl->setGraphicsRootSignature(*textureSkyPipeline.rootSignature);

		cl->setGraphics32BitConstants(SKY_RS_VP, sky_cb{ camera.proj * createSkyViewMatrix(camera.view) });
		cl->setGraphics32BitConstants(SKY_RS_INTENSITY, sky_intensity_cb{ settings.skyIntensity });
		cl->setDescriptorHeapSRV(SKY_RS_TEX, 0, environment->sky->defaultSRV);

		cl->setVertexBuffer(0, positionOnlyMesh.vertexBuffer);
		cl->setIndexBuffer(positionOnlyMesh.indexBuffer);
		cl->drawIndexed(cubeMesh.numTriangles * 3, 1, cubeMesh.firstTriangle * 3, cubeMesh.baseVertex, 0);
	}
	else
	{
		cl->clearRTV(hdrRenderTarget, 0, 0.f, 0.f, 0.f);
	}


	if (performedSkinning)
	{
		dxContext.renderQueue.waitForOtherQueue(dxContext.computeQueue); // Wait for GPU skinning.
	}


	// ----------------------------------------
	// DEPTH-ONLY PASS
	// ----------------------------------------

	cl->setRenderTarget(depthOnlyRenderTarget);
	cl->setViewport(depthOnlyRenderTarget.viewport);

	// Static.
	if (opaqueRenderPass.staticDepthOnlyDrawCalls.size() > 0)
	{
		cl->setPipelineState(*depthOnlyPipeline.pipeline);
		cl->setGraphicsRootSignature(*depthOnlyPipeline.rootSignature);

		for (const auto& dc : opaqueRenderPass.staticDepthOnlyDrawCalls)
		{
			const mat4& m = dc.transform;
			const submesh_info& submesh = dc.submesh;

			cl->setGraphics32BitConstants(DEPTH_ONLY_RS_OBJECT_ID, (uint32)dc.objectID);
			cl->setGraphics32BitConstants(DEPTH_ONLY_RS_MVP, depth_only_transform_cb{ camera.viewProj * m, camera.prevFrameViewProj * m });

			cl->setVertexBuffer(0, dc.vertexBuffer);
			cl->setIndexBuffer(dc.indexBuffer);
			cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
		}
	}

	// Dynamic.
	if (opaqueRenderPass.dynamicDepthOnlyDrawCalls.size() > 0)
	{
		cl->setPipelineState(*depthOnlyPipeline.pipeline);
		cl->setGraphicsRootSignature(*depthOnlyPipeline.rootSignature);

		for (const auto& dc : opaqueRenderPass.dynamicDepthOnlyDrawCalls)
		{
			const mat4& m = dc.transform;
			const mat4& prevFrameM = dc.prevFrameTransform;
			const submesh_info& submesh = dc.submesh;

			cl->setGraphics32BitConstants(DEPTH_ONLY_RS_OBJECT_ID, (uint32)dc.objectID);
			cl->setGraphics32BitConstants(DEPTH_ONLY_RS_MVP, depth_only_transform_cb{ camera.viewProj * m, camera.prevFrameViewProj * prevFrameM });

			cl->setVertexBuffer(0, dc.vertexBuffer);
			cl->setIndexBuffer(dc.indexBuffer);
			cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
		}
	}

	// Animated.
	if (opaqueRenderPass.animatedDepthOnlyDrawCalls.size() > 0)
	{
		cl->setPipelineState(*animatedDepthOnlyPipeline.pipeline);
		cl->setGraphicsRootSignature(*animatedDepthOnlyPipeline.rootSignature);

		for (const auto& dc : opaqueRenderPass.animatedDepthOnlyDrawCalls)
		{
			const mat4& m = dc.transform;
			const mat4& prevFrameM = dc.prevFrameTransform;
			const submesh_info& submesh = dc.submesh;
			const submesh_info& prevFrameSubmesh = dc.prevFrameSubmesh;
			const ref<dx_vertex_buffer>& prevFrameVertexBuffer = dc.prevFrameVertexBuffer;

			cl->setGraphics32BitConstants(DEPTH_ONLY_RS_OBJECT_ID, (uint32)dc.objectID);
			cl->setGraphics32BitConstants(DEPTH_ONLY_RS_MVP, depth_only_transform_cb{ camera.viewProj * m, camera.prevFrameViewProj * prevFrameM });
			cl->setRootGraphicsSRV(DEPTH_ONLY_RS_PREV_FRAME_POSITIONS, prevFrameVertexBuffer->gpuVirtualAddress + prevFrameSubmesh.baseVertex * prevFrameVertexBuffer->elementSize);

			cl->setVertexBuffer(0, dc.vertexBuffer);
			cl->setIndexBuffer(dc.indexBuffer);
			cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
		}
	}


	// Copy hovered object id to readback buffer.
	if (objectIDsTexture)
	{
		barrier_batcher(cl)
			.transition(objectIDsTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);

		if (input.overWindow)
		{
			cl->copyTextureRegionToBuffer(objectIDsTexture, hoveredObjectIDReadbackBuffer[dxContext.bufferedFrameID], (uint32)input.mouse.x, (uint32)input.mouse.y, 1, 1);
		}
	}


	// ----------------------------------------
	// LIGHT CULLING
	// ----------------------------------------

	if (numPointLights || numSpotLights)
	{
		if (numPointLights)
		{
			point_light_cb* pls = (point_light_cb*)mapBuffer(pointLightBuffer[dxContext.bufferedFrameID]);
			memcpy(pls, pointLights, sizeof(point_light_cb) * numPointLights);
			unmapBuffer(pointLightBuffer[dxContext.bufferedFrameID]);
		}
		if (numSpotLights)
		{
			spot_light_cb* sls = (spot_light_cb*)mapBuffer(spotLightBuffer[dxContext.bufferedFrameID]);
			memcpy(sls, spotLights, sizeof(spot_light_cb) * numSpotLights);
			unmapBuffer(spotLightBuffer[dxContext.bufferedFrameID]);
		}


		// Tiled frusta.
		cl->setPipelineState(*worldSpaceFrustaPipeline.pipeline);
		cl->setComputeRootSignature(*worldSpaceFrustaPipeline.rootSignature);
		cl->setComputeDynamicConstantBuffer(WORLD_SPACE_TILED_FRUSTA_RS_CAMERA, cameraCBV);
		cl->setCompute32BitConstants(WORLD_SPACE_TILED_FRUSTA_RS_CB, frusta_cb{ lightCullingBuffers.numTilesX, lightCullingBuffers.numTilesY });
		cl->setRootComputeUAV(WORLD_SPACE_TILED_FRUSTA_RS_FRUSTA_UAV, lightCullingBuffers.tiledFrusta);
		cl->dispatch(bucketize(lightCullingBuffers.numTilesX, 16), bucketize(lightCullingBuffers.numTilesY, 16));

		// Light culling.
		cl->clearUAV(lightCullingBuffers.lightIndexCounter, 0.f);
		//cl->uavBarrier(lightCullingBuffers.lightIndexCounter); // Is this necessary?
		cl->setPipelineState(*lightCullingPipeline.pipeline);
		cl->setComputeRootSignature(*lightCullingPipeline.rootSignature);
		cl->setComputeDynamicConstantBuffer(LIGHT_CULLING_RS_CAMERA, cameraCBV);
		cl->setCompute32BitConstants(LIGHT_CULLING_RS_CB, light_culling_cb{ lightCullingBuffers.numTilesX, numPointLights, numSpotLights });
		cl->setDescriptorHeapSRV(LIGHT_CULLING_RS_SRV_UAV, 0, depthStencilBuffer);
		cl->setDescriptorHeapSRV(LIGHT_CULLING_RS_SRV_UAV, 1, pointLightBuffer[dxContext.bufferedFrameID]);
		cl->setDescriptorHeapSRV(LIGHT_CULLING_RS_SRV_UAV, 2, spotLightBuffer[dxContext.bufferedFrameID]);
		cl->setDescriptorHeapSRV(LIGHT_CULLING_RS_SRV_UAV, 3, lightCullingBuffers.tiledFrusta);
		cl->setDescriptorHeapUAV(LIGHT_CULLING_RS_SRV_UAV, 4, lightCullingBuffers.lightIndexCounter);
		cl->setDescriptorHeapUAV(LIGHT_CULLING_RS_SRV_UAV, 5, lightCullingBuffers.pointLightIndexList);
		cl->setDescriptorHeapUAV(LIGHT_CULLING_RS_SRV_UAV, 6, lightCullingBuffers.spotLightIndexList);
		cl->setDescriptorHeapUAV(LIGHT_CULLING_RS_SRV_UAV, 7, lightCullingBuffers.lightGrid);
		cl->dispatch(lightCullingBuffers.numTilesX, lightCullingBuffers.numTilesY);
	}



	// ----------------------------------------
	// SHADOW MAP PASS
	// ----------------------------------------

	cl->setPipelineState(*shadowPipeline.pipeline);
	cl->setGraphicsRootSignature(*shadowPipeline.rootSignature);

	for (uint32 i = 0; i < sun.numShadowCascades; ++i)
	{
		cl->setRenderTarget(sunShadowRenderTarget[i]);
		cl->setViewport(sunShadowRenderTarget[i].viewport);
		cl->clearDepth(sunShadowRenderTarget[i]);

		for (uint32 cascade = 0; cascade <= i; ++cascade)
		{
			for (const auto& dc : sunShadowRenderPass.drawCalls[cascade])
			{
				const mat4& m = dc.transform;
				const submesh_info& submesh = dc.submesh;
				cl->setGraphics32BitConstants(SHADOW_RS_MVP, sun.vp[i] * m);

				cl->setVertexBuffer(0, dc.vertexBuffer);
				cl->setIndexBuffer(dc.indexBuffer);

				cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
			}
		}
	}

	barrier_batcher(cl)
		.transition(sunShadowCascadeTextures, MAX_NUM_SUN_SHADOW_CASCADES, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);




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
	// LIGHT PASS
	// ----------------------------------------

	cl->setRenderTarget(hdrRenderTarget);
	cl->setViewport(hdrRenderTarget.viewport);

	if (opaqueRenderPass.drawCalls.size() > 0)
	{
		material_setup_function lastSetupFunc = 0;

		for (const auto& dc : opaqueRenderPass.drawCalls)
		{
			const mat4& m = dc.transform;
			const submesh_info& submesh = dc.submesh;

			if (dc.materialSetupFunc != lastSetupFunc)
			{
				dc.materialSetupFunc(cl, materialInfo);
				lastSetupFunc = dc.materialSetupFunc;
			}

			dc.material->prepareForRendering(cl);

			cl->setGraphics32BitConstants(0, transform_cb{ camera.viewProj * m, m });

			cl->setVertexBuffer(0, dc.vertexBuffer);
			cl->setIndexBuffer(dc.indexBuffer);
			cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
		}
	}


	barrier_batcher(cl)
		.transition(worldNormalsTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
		.transition(screenVelocitiesTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);


	// ----------------------------------------
	// OUTLINES
	// ----------------------------------------

	if (opaqueRenderPass.outlinedObjects.size() > 0)
	{
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

		mark(opaqueRenderPass, cl, camera.viewProj);
		mark(transparentRenderPass, cl, camera.viewProj);

		// Draw outline.
		cl->transitionBarrier(depthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ);

		cl->setPipelineState(*outlineDrawerPipeline.pipeline);
		cl->setGraphicsRootSignature(*outlineDrawerPipeline.rootSignature);

		cl->setGraphics32BitConstants(OUTLINE_RS_CB, outline_drawer_cb{ (int)renderWidth, (int)renderHeight });
		cl->setDescriptorHeapResource(OUTLINE_RS_STENCIL, 0, 1, depthStencilBuffer->stencilSRV);

		cl->drawFullscreenTriangle();

		cl->transitionBarrier(depthStencilBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}



	// ----------------------------------------
	// RAYTRACING
	// ----------------------------------------

#if 1
	if (dxContext.raytracingSupported && giRenderPass.bindingTable)
	{
		raytracingPipeline.render(cl, *giRenderPass.bindingTable, *giRenderPass.tlas,
			raytracingTexture, settings.numRaytracingBounces, settings.raytracingFadeoutDistance, settings.raytracingMaxDistance, settings.environmentIntensity, settings.skyIntensity,
			cameraCBV, sunCBV, depthStencilBuffer, worldNormalsTexture, environment, brdfTex);
	}

	//generateMipMapsOnGPU(cl, raytracingTexture);
	cl->resetToDynamicDescriptorHeap();

	cl->uavBarrier(raytracingTexture);
	gaussianBlur(cl, raytracingTexture, raytracingTextureTmpForBlur, settings.blurRaytracingResultIterations);
#endif




	// ----------------------------------------
	// HELPER STUFF
	// ----------------------------------------


	if (visualizationRenderPass.drawCalls.size() > 0)
	{
		cl->setPipelineState(*flatSimplePipeline.pipeline);
		cl->setGraphicsRootSignature(*flatSimplePipeline.rootSignature);

		for (const auto& dc : visualizationRenderPass.drawCalls)
		{
			const mat4& m = dc.transform;
			const submesh_info& submesh = dc.submesh;

			cl->setGraphics32BitConstants(0, flat_simple_transform_cb{ camera.viewProj * m, camera.view * m });
			cl->setGraphics32BitConstants(1, dc.color);

			cl->setVertexBuffer(0, dc.vertexBuffer);
			cl->setIndexBuffer(dc.indexBuffer);
			cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
		}
	}


	if (settings.showLightVolumes)
	{
		// Light volumes.
		cl->setPipelineState(*flatUnlitPipeline.pipeline);
		cl->setGraphicsRootSignature(*flatUnlitPipeline.rootSignature);
		cl->setVertexBuffer(0, positionOnlyMesh.vertexBuffer);
		cl->setIndexBuffer(positionOnlyMesh.indexBuffer);

		for (uint32 i = 0; i < numPointLights; ++i)
		{
			float radius = pointLights[i].radius;
			vec3 position = pointLights[i].position;
			cl->setGraphics32BitConstants(0, camera.viewProj * createModelMatrix(position, quat::identity, vec3(radius, radius, radius)));
			cl->drawIndexed(sphereMesh.numTriangles * 3, 1, sphereMesh.firstTriangle * 3, sphereMesh.baseVertex, 0);
		}
	}



	// ----------------------------------------
	// PRESENT
	// ----------------------------------------

	barrier_batcher(cl)
		.transition(hdrColorTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	cl->setRenderTarget(windowRenderTarget);
	cl->setViewport(windowViewport);
	if (aspectRatioModeChanged)
	{
		cl->clearRTV(windowRenderTarget, 0, 0.f, 0.f, 0.f);
	}

	cl->setPipelineState(*presentPipeline.pipeline);
	cl->setGraphicsRootSignature(*presentPipeline.rootSignature);

	cl->setGraphics32BitConstants(PRESENT_RS_TONEMAP, settings.tonemap);
	cl->setGraphics32BitConstants(PRESENT_RS_PRESENT, present_cb{ 0, 0.f });
	cl->setDescriptorHeapSRV(PRESENT_RS_TEX, 0, hdrColorTexture);
	cl->drawFullscreenTriangle();


	barrier_batcher(cl)
		.transition(hdrColorTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON)
		.transition(objectIDsTexture, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON)
		.transition(worldNormalsTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON)
		.transition(screenVelocitiesTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON)
		.transition(frameResult, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);

	dxContext.executeCommandList(cl);

	oldSettings = settings;
}

void dx_renderer::blitResultToScreen(dx_command_list* cl, dx_rtv_descriptor_handle rtv)
{
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.f, 0.f, (float)windowWidth, (float)windowHeight);
	cl->setViewport(viewport);

	cl->setRenderTarget(&rtv, 1, 0);

	cl->setPipelineState(*blitPipeline.pipeline);
	cl->setGraphicsRootSignature(*blitPipeline.rootSignature);
	cl->setDescriptorHeapSRV(0, 0, frameResult);
	cl->drawFullscreenTriangle();
}
