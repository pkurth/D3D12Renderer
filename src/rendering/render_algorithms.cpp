#include "pch.h"
#include "render_algorithms.h"
#include "dx/dx_profiling.h"
#include "dx/dx_barrier_batcher.h"
#include "core/cpu_profiling.h"

#include "render_resources.h"
#include "render_utils.h"

#include "post_processing_rs.hlsli"
#include "ssr_rs.hlsli"
#include "light_culling_rs.hlsli"
#include "depth_only_rs.hlsli"
#include "sky_rs.hlsli"
#include "outline_rs.hlsli"
#include "transform.hlsli"
#include "visualization_rs.hlsli"

static dx_pipeline depthPrePassPipeline;
static dx_pipeline animatedDepthPrePassPipeline;

static dx_pipeline doubleSidedDepthPrePassPipeline;
static dx_pipeline doubleSidedAnimatedDepthPrePassPipeline;

static dx_pipeline shadowPipeline;
static dx_pipeline pointLightShadowPipeline;

static dx_pipeline doubleSidedShadowPipeline;
static dx_pipeline doubleSidedPointLightShadowPipeline;

static dx_pipeline textureSkyPipeline;
static dx_pipeline proceduralSkyPipeline;
static dx_pipeline preethamSkyPipeline;
static dx_pipeline stylisticSkyPipeline;
static dx_pipeline sphericalHarmonicsSkyPipeline;

static dx_pipeline outlineMarkerPipeline;
static dx_pipeline outlineDrawerPipeline;

static dx_pipeline shadowMapCopyPipeline;

static dx_pipeline worldSpaceFrustaPipeline;
static dx_pipeline lightCullingPipeline;

static dx_pipeline ssrRaycastPipeline;
static dx_pipeline ssrResolvePipeline;
static dx_pipeline ssrTemporalPipeline;
static dx_pipeline ssrMedianBlurPipeline;

static dx_pipeline specularAmbientPipeline;

static dx_pipeline hierarchicalLinearDepthPipeline;

static dx_pipeline gaussianBlurFloatPipelines[gaussian_blur_kernel_size_count];
static dx_pipeline gaussianBlurFloat4Pipelines[gaussian_blur_kernel_size_count];

static dx_pipeline dilationPipeline;
static dx_pipeline erosionPipeline;

static dx_pipeline taaPipeline;

static dx_pipeline blitPipeline;

static dx_pipeline bloomThresholdPipeline;
static dx_pipeline bloomCombinePipeline;

static dx_pipeline hbaoPipeline;
static dx_pipeline sssPipeline;

static dx_pipeline shadowBlurXPipeline;
static dx_pipeline shadowBlurYPipeline;

static dx_pipeline tonemapPipeline;
static dx_pipeline presentPipeline;

static dx_pipeline depthSobelPipeline;

static dx_pipeline visualizeSunShadowCascadesPipeline;

static dx_command_signature rigidDepthPrePassCommandSignature;
static dx_command_signature animatedDepthPrePassCommandSignature;


#pragma pack(push, 1)
struct indirect_rigid_depth_prepass_command
{
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;		// 16 bytes.
	D3D12_INDEX_BUFFER_VIEW indexBufferView;		// 16 bytes -> 32 bytes total.
	depth_only_transform_cb transform;				// 128 bytes -> 160 bytes total.
	uint32 objectID;								// 4 bytes -> 164 bytes total.
	D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;		// 20 bytes -> 184 bytes total.
	uint32 padding[2];								// 8 bytes -> 192 bytes total (divisible by 16).
};

struct indirect_animated_depth_prepass_command
{
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;		// 16 bytes.
	D3D12_INDEX_BUFFER_VIEW indexBufferView;		// 16 bytes -> 32 bytes total.
	depth_only_transform_cb transform;				// 128 bytes -> 160 bytes total.
	uint32 objectID;								// 4 bytes -> 164 bytes total.
	D3D12_GPU_VIRTUAL_ADDRESS prevFramePositions;	// 8 bytes -> 172 bytes total.
	D3D12_DRAW_INDEXED_ARGUMENTS drawArguments;		// 20 bytes -> 192 bytes total (divisible by 16).
};
#pragma pack(pop)


void loadCommonShaders()
{
	// Sky.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(skyPassFormats, arraysize(skyPassFormats), depthStencilFormat)
			.depthSettings(true, false)
			.cullFrontFaces();

		textureSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_texture_ps" });
		proceduralSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_procedural_ps" });
		preethamSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_preetham_ps" });
		stylisticSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_stylistic_ps" });
		sphericalHarmonicsSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_sh_ps" });
	}

	// Depth prepass.
	{
		DXGI_FORMAT depthOnlyFormat[] = { screenVelocitiesFormat, objectIDsFormat };

		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(depthOnlyFormat, arraysize(depthOnlyFormat), depthStencilFormat)
			.inputLayout(inputLayout_position);

		depthPrePassPipeline = createReloadablePipeline(desc, { "depth_only_vs", "depth_only_ps" }, rs_in_vertex_shader);
		animatedDepthPrePassPipeline = createReloadablePipeline(desc, { "depth_only_animated_vs", "depth_only_ps" }, rs_in_vertex_shader);

		desc.cullingOff();
		doubleSidedDepthPrePassPipeline = createReloadablePipeline(desc, { "depth_only_vs", "depth_only_ps" }, rs_in_vertex_shader);
		doubleSidedAnimatedDepthPrePassPipeline = createReloadablePipeline(desc, { "depth_only_animated_vs", "depth_only_ps" }, rs_in_vertex_shader);
	}

	// Shadow.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(0, 0, shadowDepthFormat)
			.inputLayout(inputLayout_position)
			//.cullFrontFaces()
			;

		shadowPipeline = createReloadablePipeline(desc, { "shadow_vs" }, rs_in_vertex_shader);
		pointLightShadowPipeline = createReloadablePipeline(desc, { "shadow_point_light_vs", "shadow_point_light_ps" }, rs_in_vertex_shader);

		desc.cullingOff();
		doubleSidedShadowPipeline = createReloadablePipeline(desc, { "shadow_vs" }, rs_in_vertex_shader);
		doubleSidedPointLightShadowPipeline = createReloadablePipeline(desc, { "shadow_point_light_vs", "shadow_point_light_ps" }, rs_in_vertex_shader);
	}

	// Outline.
	{
		auto markerDesc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position)
			.renderTargets(0, 0, depthStencilFormat)
			.stencilSettings(D3D12_COMPARISON_FUNC_ALWAYS,
				D3D12_STENCIL_OP_REPLACE,
				D3D12_STENCIL_OP_REPLACE,
				D3D12_STENCIL_OP_KEEP,
				D3D12_DEFAULT_STENCIL_READ_MASK,
				stencil_flag_selected_object) // Mark selected object.
			.depthSettings(false, false)
			.cullingOff(); // Since this is fairly light-weight, we only render double sided.

		outlineMarkerPipeline = createReloadablePipeline(markerDesc, { "outline_vs" }, rs_in_vertex_shader);


		auto drawerDesc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(ldrFormat, depthStencilFormat)
			.stencilSettings(D3D12_COMPARISON_FUNC_EQUAL,
				D3D12_STENCIL_OP_KEEP,
				D3D12_STENCIL_OP_KEEP,
				D3D12_STENCIL_OP_KEEP,
				stencil_flag_selected_object, // Read only selected object bit.
				0)
			.depthSettings(false, false);

		outlineDrawerPipeline = createReloadablePipeline(drawerDesc, { "fullscreen_triangle_vs", "outline_ps" });
	}

	// Shadow map copy.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(0, 0, shadowDepthFormat)
			.depthSettings(true, true, D3D12_COMPARISON_FUNC_ALWAYS)
			.cullingOff();

		shadowMapCopyPipeline = createReloadablePipeline(desc, { "fullscreen_triangle_vs", "shadow_map_copy_ps" });
	}

	worldSpaceFrustaPipeline = createReloadablePipeline("world_space_tiled_frusta_cs");
	lightCullingPipeline = createReloadablePipeline("light_culling_cs");

	ssrRaycastPipeline = createReloadablePipeline("ssr_raycast_cs");
	ssrResolvePipeline = createReloadablePipeline("ssr_resolve_cs");
	ssrTemporalPipeline = createReloadablePipeline("ssr_temporal_cs");
	ssrMedianBlurPipeline = createReloadablePipeline("ssr_median_blur_cs");

	specularAmbientPipeline = createReloadablePipeline("specular_ambient_cs");

	hierarchicalLinearDepthPipeline = createReloadablePipeline("hierarchical_linear_depth_cs");

	gaussianBlurFloatPipelines[gaussian_blur_5x5] = createReloadablePipeline("gaussian_blur_5x5_float_cs");
	gaussianBlurFloatPipelines[gaussian_blur_9x9] = createReloadablePipeline("gaussian_blur_9x9_float_cs");
	gaussianBlurFloatPipelines[gaussian_blur_13x13] = createReloadablePipeline("gaussian_blur_13x13_float_cs");

	gaussianBlurFloat4Pipelines[gaussian_blur_5x5] = createReloadablePipeline("gaussian_blur_5x5_float4_cs");
	gaussianBlurFloat4Pipelines[gaussian_blur_9x9] = createReloadablePipeline("gaussian_blur_9x9_float4_cs");
	gaussianBlurFloat4Pipelines[gaussian_blur_13x13] = createReloadablePipeline("gaussian_blur_13x13_float4_cs");

	dilationPipeline = createReloadablePipeline("dilation_cs");
	erosionPipeline = createReloadablePipeline("erosion_cs");

	taaPipeline = createReloadablePipeline("taa_cs");

	blitPipeline = createReloadablePipeline("blit_cs");

	bloomThresholdPipeline = createReloadablePipeline("bloom_threshold_cs");
	bloomCombinePipeline = createReloadablePipeline("bloom_combine_cs");

	hbaoPipeline = createReloadablePipeline("hbao_cs");
	sssPipeline = createReloadablePipeline("sss_cs");

	shadowBlurXPipeline = createReloadablePipeline("shadow_blur_x_cs");
	shadowBlurYPipeline = createReloadablePipeline("shadow_blur_y_cs");

	tonemapPipeline = createReloadablePipeline("tonemap_cs");
	presentPipeline = createReloadablePipeline("present_cs");

	depthSobelPipeline = createReloadablePipeline("depth_sobel_cs");


	visualizeSunShadowCascadesPipeline = createReloadablePipeline("sun_shadow_cascades_cs");
}

void loadRemainingRenderResources()
{
	D3D12_INDIRECT_ARGUMENT_DESC rigidDepthPrepassDescs[] =
	{
		indirect_vertex_buffer(0),
		indirect_index_buffer(),
		indirect_root_constants<depth_only_transform_cb>(DEPTH_ONLY_RS_MVP),
		indirect_root_constants<uint32>(DEPTH_ONLY_RS_OBJECT_ID),
		indirect_draw_indexed(),
	};

	rigidDepthPrePassCommandSignature = createCommandSignature(*depthPrePassPipeline.rootSignature,
		rigidDepthPrepassDescs, arraysize(rigidDepthPrepassDescs),
		sizeof(indirect_rigid_depth_prepass_command));

	D3D12_INDIRECT_ARGUMENT_DESC animatedDepthPrepassDescs[] =
	{
		indirect_vertex_buffer(0),
		indirect_index_buffer(),
		indirect_root_constants<depth_only_transform_cb>(DEPTH_ONLY_RS_MVP),
		indirect_root_constants<uint32>(DEPTH_ONLY_RS_OBJECT_ID),
		indirect_srv(DEPTH_ONLY_RS_PREV_FRAME_POSITIONS),
		indirect_draw_indexed(),
	};

	animatedDepthPrePassCommandSignature = createCommandSignature(*animatedDepthPrePassPipeline.rootSignature,
		animatedDepthPrepassDescs, arraysize(animatedDepthPrepassDescs),
		sizeof(indirect_animated_depth_prepass_command));
}

#if 0
static void batchRenderRigidDepthPrepass(dx_command_list* cl,
	const sort_key_vector<float, static_depth_only_render_command>& commands,
	const mat4& viewProj, const mat4& prevFrameViewProj)
{
	PROFILE_ALL(cl, "Batch depth pre-pass");

	dx_allocation allocation = dxContext.allocateDynamicBuffer(
		(uint32)(commands.size() * sizeof(indirect_rigid_depth_prepass_command)));
	indirect_rigid_depth_prepass_command* ptr = (indirect_rigid_depth_prepass_command*)allocation.cpuPtr;

	for (const auto& dc : commands)
	{
		ptr->vertexBufferView = dc.vertexBuffer.view;
		ptr->indexBufferView = dc.indexBuffer.view;
		ptr->transform = depth_only_transform_cb{ viewProj * dc.transform, prevFrameViewProj * dc.transform };
		ptr->objectID = dc.objectID;
		ptr->drawArguments.StartInstanceLocation = 0;
		ptr->drawArguments.InstanceCount = 1;
		ptr->drawArguments.BaseVertexLocation = dc.submesh.baseVertex;
		ptr->drawArguments.IndexCountPerInstance = dc.submesh.numTriangles * 3;
		ptr->drawArguments.StartIndexLocation = dc.submesh.firstTriangle * 3;

		++ptr;
	}

	cl->drawIndirect(rigidDepthPrePassCommandSignature, (uint32)commands.size(),
		allocation.resource,
		allocation.offsetInResource);
}
#endif

static void depthPrePassInternal(dx_command_list* cl,
	dx_pipeline& rigidPipeline,
	dx_pipeline& animatedPipeline,
	const sort_key_vector<float, static_depth_only_render_command>& staticCommands,
	const sort_key_vector<float, dynamic_depth_only_render_command>& dynamicCommands,
	const sort_key_vector<float, animated_depth_only_render_command>& animatedCommands,
	const mat4& viewProj, const mat4& prevFrameViewProj,
	depth_only_camera_jitter_cb jitterCB)
{
	if (staticCommands.size() > 0 || dynamicCommands.size() > 0)
	{
		cl->setPipelineState(*rigidPipeline.pipeline);
		cl->setGraphicsRootSignature(*rigidPipeline.rootSignature);

		cl->setGraphics32BitConstants(DEPTH_ONLY_RS_CAMERA_JITTER, jitterCB);
	}

	// Static.
	if (staticCommands.size() > 0)
	{
		PROFILE_ALL(cl, "Static");

		for (const auto& dc : staticCommands)
		{
			cl->setGraphics32BitConstants(DEPTH_ONLY_RS_OBJECT_ID, dc.objectID);
			cl->setGraphics32BitConstants(DEPTH_ONLY_RS_MVP, depth_only_transform_cb{ viewProj * dc.transform, prevFrameViewProj * dc.transform });

			cl->setVertexBuffer(0, dc.vertexBuffer);
			cl->setIndexBuffer(dc.indexBuffer);
			cl->drawIndexed(dc.submesh.numIndices, 1, dc.submesh.firstIndex, dc.submesh.baseVertex, 0);
		}
	}

	// Dynamic.
	if (dynamicCommands.size() > 0)
	{
		PROFILE_ALL(cl, "Dynamic");

		for (const auto& dc : dynamicCommands)
		{
			cl->setGraphics32BitConstants(DEPTH_ONLY_RS_OBJECT_ID, dc.objectID);
			cl->setGraphics32BitConstants(DEPTH_ONLY_RS_MVP, depth_only_transform_cb{ viewProj * dc.transform, prevFrameViewProj * dc.prevFrameTransform });

			cl->setVertexBuffer(0, dc.vertexBuffer);
			cl->setIndexBuffer(dc.indexBuffer);
			cl->drawIndexed(dc.submesh.numIndices, 1, dc.submesh.firstIndex, dc.submesh.baseVertex, 0);
		}
	}

	// Animated.
	if (animatedCommands.size() > 0)
	{
		PROFILE_ALL(cl, "Animated");

		cl->setPipelineState(*animatedPipeline.pipeline);
		cl->setGraphicsRootSignature(*animatedPipeline.rootSignature);

		cl->setGraphics32BitConstants(DEPTH_ONLY_RS_CAMERA_JITTER, jitterCB);

		for (const auto& dc : animatedCommands)
		{
			cl->setGraphics32BitConstants(DEPTH_ONLY_RS_OBJECT_ID, dc.objectID);
			cl->setGraphics32BitConstants(DEPTH_ONLY_RS_MVP, depth_only_transform_cb{ viewProj * dc.transform, prevFrameViewProj * dc.prevFrameTransform });
			cl->setRootGraphicsSRV(DEPTH_ONLY_RS_PREV_FRAME_POSITIONS, dc.prevFrameVertexBufferAddress + dc.submesh.baseVertex * dc.vertexBuffer.view.StrideInBytes);

			cl->setVertexBuffer(0, dc.vertexBuffer);
			cl->setIndexBuffer(dc.indexBuffer);
			cl->drawIndexed(dc.submesh.numIndices, 1, dc.submesh.firstIndex, dc.submesh.baseVertex, 0);
		}
	}
}

void depthPrePass(dx_command_list* cl,
	const dx_render_target& depthOnlyRenderTarget,
	const opaque_render_pass* opaqueRenderPass,
	const mat4& viewProj, const mat4& prevFrameViewProj,
	vec2 jitter, vec2 prevFrameJitter)
{
	if (opaqueRenderPass)
	{
		PROFILE_ALL(cl, "Depth pre-pass");

		cl->setRenderTarget(depthOnlyRenderTarget);
		cl->setViewport(depthOnlyRenderTarget.viewport);
		cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		depth_only_camera_jitter_cb jitterCB = { jitter, prevFrameJitter };

		depthPrePassInternal(cl, depthPrePassPipeline, animatedDepthPrePassPipeline, 
			opaqueRenderPass->staticDepthPrepass, opaqueRenderPass->dynamicDepthPrepass, opaqueRenderPass->animatedDepthPrepass, 
			viewProj, prevFrameViewProj, jitterCB);
		depthPrePassInternal(cl, doubleSidedDepthPrePassPipeline, doubleSidedAnimatedDepthPrePassPipeline, 
			opaqueRenderPass->staticDoublesidedDepthPrepass, opaqueRenderPass->dynamicDoublesidedDepthPrepass, opaqueRenderPass->animatedDoublesidedDepthPrepass, 
			viewProj, prevFrameViewProj, jitterCB);
	}
}

void texturedSky(dx_command_list* cl,
	const dx_render_target& skyRenderTarget,
	const mat4& proj, const mat4& view, const mat4& prevFrameView,
	ref<dx_texture> sky,
	float skyIntensity,
	vec2 jitter, vec2 prevFrameJitter)
{
	PROFILE_ALL(cl, "Sky");

	cl->setRenderTarget(skyRenderTarget);
	cl->setViewport(skyRenderTarget.viewport);

	cl->setPipelineState(*textureSkyPipeline.pipeline);
	cl->setGraphicsRootSignature(*textureSkyPipeline.rootSignature);

	cl->setGraphics32BitConstants(SKY_RS_VP, sky_transform_cb{ proj * createSkyViewMatrix(view), proj * createSkyViewMatrix(prevFrameView) });
	cl->setGraphics32BitConstants(SKY_RS_INTENSITY, sky_cb{ jitter, prevFrameJitter, skyIntensity });
	cl->setDescriptorHeapSRV(SKY_RS_TEX, 0, sky);

	cl->drawCubeTriangleStrip();
}

void proceduralSky(dx_command_list* cl,
	const dx_render_target& skyRenderTarget,
	const mat4& proj, const mat4& view, const mat4& prevFrameView,
	float skyIntensity,
	vec2 jitter, vec2 prevFrameJitter)
{
	PROFILE_ALL(cl, "Sky");

	cl->setRenderTarget(skyRenderTarget);
	cl->setViewport(skyRenderTarget.viewport);

	cl->setPipelineState(*proceduralSkyPipeline.pipeline);
	cl->setGraphicsRootSignature(*proceduralSkyPipeline.rootSignature);

	cl->setGraphics32BitConstants(SKY_RS_VP, sky_transform_cb{ proj * createSkyViewMatrix(view), proj * createSkyViewMatrix(prevFrameView) });
	cl->setGraphics32BitConstants(SKY_RS_INTENSITY, sky_cb{ jitter, prevFrameJitter, skyIntensity });

	cl->drawCubeTriangleStrip();
}

void stylisticSky(dx_command_list* cl,
	const dx_render_target& skyRenderTarget,
	const mat4& proj, const mat4& view, const mat4& prevFrameView,
	vec3 sunDirection, float skyIntensity,
	vec2 jitter, vec2 prevFrameJitter)
{
	PROFILE_ALL(cl, "Sky");

	cl->setRenderTarget(skyRenderTarget);
	cl->setViewport(skyRenderTarget.viewport);

	cl->setPipelineState(*stylisticSkyPipeline.pipeline);
	cl->setGraphicsRootSignature(*stylisticSkyPipeline.rootSignature);

	cl->setGraphics32BitConstants(SKY_RS_VP, sky_transform_cb{ proj * createSkyViewMatrix(view), proj * createSkyViewMatrix(prevFrameView) });
	cl->setGraphics32BitConstants(SKY_RS_INTENSITY, sky_cb{ jitter, prevFrameJitter, skyIntensity, sunDirection });

	cl->drawCubeTriangleStrip();
}

void sphericalHarmonicsSky(dx_command_list* cl,
	const dx_render_target& skyRenderTarget,
	const mat4& proj, const mat4& view, const mat4& prevFrameView,
	const ref<dx_buffer>& sh, uint32 shIndex,
	float skyIntensity,
	vec2 jitter, vec2 prevFrameJitter)
{
	PROFILE_ALL(cl, "Sky");

	cl->setRenderTarget(skyRenderTarget);
	cl->setViewport(skyRenderTarget.viewport);

	cl->setPipelineState(*sphericalHarmonicsSkyPipeline.pipeline);
	cl->setGraphicsRootSignature(*sphericalHarmonicsSkyPipeline.rootSignature);

	cl->setGraphics32BitConstants(SKY_RS_VP, sky_transform_cb{ proj * createSkyViewMatrix(view), proj * createSkyViewMatrix(prevFrameView) });
	cl->setGraphics32BitConstants(SKY_RS_INTENSITY, sky_cb{ jitter, prevFrameJitter, skyIntensity });
	cl->setRootGraphicsSRV(SKY_RS_SH, sh->gpuVirtualAddress + sizeof(spherical_harmonics) * shIndex);

	cl->drawCubeTriangleStrip();
}

void preethamSky(dx_command_list* cl,
	const dx_render_target& skyRenderTarget,
	const mat4& proj, const mat4& view, const mat4& prevFrameView,
	vec3 sunDirection, float skyIntensity,
	vec2 jitter, vec2 prevFrameJitter)
{
	PROFILE_ALL(cl, "Sky");

	cl->setRenderTarget(skyRenderTarget);
	cl->setViewport(skyRenderTarget.viewport);

	cl->setPipelineState(*preethamSkyPipeline.pipeline);
	cl->setGraphicsRootSignature(*preethamSkyPipeline.rootSignature);

	cl->setGraphics32BitConstants(SKY_RS_VP, sky_transform_cb{ proj * createSkyViewMatrix(view), proj * createSkyViewMatrix(prevFrameView) });
	cl->setGraphics32BitConstants(SKY_RS_INTENSITY, sky_cb{ jitter, prevFrameJitter, skyIntensity, sunDirection });

	cl->drawCubeTriangleStrip();
}

static void renderSunCascadeShadow(dx_command_list* cl, const std::vector<shadow_render_command>& drawCalls, const mat4& viewProj)
{
	for (const auto& dc : drawCalls)
	{
		const mat4& m = dc.transform;
		const submesh_info& submesh = dc.submesh;
		cl->setGraphics32BitConstants(SHADOW_RS_MVP, viewProj * m);

		cl->setVertexBuffer(0, dc.vertexBuffer);
		cl->setIndexBuffer(dc.indexBuffer);

		cl->drawIndexed(submesh.numIndices, 1, submesh.firstIndex, submesh.baseVertex, 0);
	}
}

static void renderSpotShadow(dx_command_list* cl, const std::vector<shadow_render_command>& drawCalls, const mat4& viewProj)
{
	for (const auto& dc : drawCalls)
	{
		const mat4& m = dc.transform;
		const submesh_info& submesh = dc.submesh;
		cl->setGraphics32BitConstants(SHADOW_RS_MVP, viewProj * m);

		cl->setVertexBuffer(0, dc.vertexBuffer);
		cl->setIndexBuffer(dc.indexBuffer);

		cl->drawIndexed(submesh.numIndices, 1, submesh.firstIndex, submesh.baseVertex, 0);
	}
}

static void renderPointShadow(dx_command_list* cl, const std::vector<shadow_render_command>& drawCalls, vec3 lightPosition, float maxDistance, float flip)
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

		cl->drawIndexed(submesh.numIndices, 1, submesh.firstIndex, submesh.baseVertex, 0);
	}
}

static void sunShadowPassInternal(dx_command_list* cl,
	const sun_shadow_render_pass** sunShadowRenderPasses, uint32 numSunLightShadowRenderPasses,
	bool staticGeom, bool doubleSided)
{
	if (!numSunLightShadowRenderPasses)
	{
		return;
	}

	PROFILE_ALL(cl, "Sun");

	for (uint32 passIndex = 0; passIndex < numSunLightShadowRenderPasses; ++passIndex)
	{
		auto pass = sunShadowRenderPasses[passIndex];

		for (uint32 renderCascadeIndex = 0; renderCascadeIndex < pass->numCascades; ++renderCascadeIndex)
		{
			const sun_cascade_render_pass& cascade = pass->cascades[renderCascadeIndex];

			PROFILE_ALL(cl, (renderCascadeIndex == 0) ? "First cascade" : (renderCascadeIndex == 1) ? "Second cascade" : (renderCascadeIndex == 2) ? "Third cascade" : "Fourth cascade");

			shadow_map_viewport vp = cascade.viewport;
			cl->setViewport(vp.x, vp.y, vp.size, vp.size);

			for (uint32 i = 0; i <= renderCascadeIndex; ++i)
			{
				auto& dcs =
					staticGeom
					? (doubleSided ? pass->cascades[i].doubleSidedStaticDrawCalls : pass->cascades[i].staticDrawCalls)
					: (doubleSided ? pass->cascades[i].doubleSidedDynamicDrawCalls : pass->cascades[i].dynamicDrawCalls);

				renderSunCascadeShadow(cl, dcs, cascade.viewProj);
			}
		}
	}
}

static void spotShadowPassInternal(dx_command_list* cl,
	const spot_shadow_render_pass** spotLightShadowRenderPasses, uint32 numSpotLightShadowRenderPasses,
	bool staticGeom, bool doubleSided)
{
	if (!numSpotLightShadowRenderPasses)
	{
		return;
	}

	PROFILE_ALL(cl, "Spot lights");

	for (uint32 i = 0; i < numSpotLightShadowRenderPasses; ++i)
	{
		PROFILE_ALL(cl, "Single light");

		shadow_map_viewport vp = spotLightShadowRenderPasses[i]->viewport;
		cl->setViewport(vp.x, vp.y, vp.size, vp.size);

		auto& dcs =
			staticGeom
			? (doubleSided ? spotLightShadowRenderPasses[i]->doubleSidedStaticDrawCalls : spotLightShadowRenderPasses[i]->staticDrawCalls)
			: (doubleSided ? spotLightShadowRenderPasses[i]->doubleSidedDynamicDrawCalls : spotLightShadowRenderPasses[i]->dynamicDrawCalls);

		renderSpotShadow(cl, dcs, spotLightShadowRenderPasses[i]->viewProjMatrix);
	}
}

static void pointShadowPassInternal(dx_command_list* cl,
	const point_shadow_render_pass** pointLightShadowRenderPasses, uint32 numPointLightShadowRenderPasses,
	bool staticGeom, bool doubleSided)
{
	if (!numPointLightShadowRenderPasses)
	{
		return;
	}

	PROFILE_ALL(cl, "Point lights");

	for (uint32 i = 0; i < numPointLightShadowRenderPasses; ++i)
	{
		PROFILE_ALL(cl, "Single light");

		for (uint32 v = 0; v < 2; ++v)
		{
			PROFILE_ALL(cl, (v == 0) ? "First hemisphere" : "Second hemisphere");

			shadow_map_viewport vp = (v == 0) ? pointLightShadowRenderPasses[i]->viewport0 : pointLightShadowRenderPasses[i]->viewport1;
			cl->setViewport(vp.x, vp.y, vp.size, vp.size);

			auto& dcs =
				staticGeom
				? (doubleSided ? pointLightShadowRenderPasses[i]->doubleSidedStaticDrawCalls : pointLightShadowRenderPasses[i]->staticDrawCalls)
				: (doubleSided ? pointLightShadowRenderPasses[i]->doubleSidedDynamicDrawCalls : pointLightShadowRenderPasses[i]->dynamicDrawCalls);

			renderPointShadow(cl, dcs, pointLightShadowRenderPasses[i]->lightPosition, pointLightShadowRenderPasses[i]->maxDistance, (v == 0) ? 1.f : -1.f);
		}
	}
}

void shadowPasses(dx_command_list* cl,
	const sun_shadow_render_pass** sunShadowRenderPasses, uint32 numSunLightShadowRenderPasses,
	const spot_shadow_render_pass** spotLightShadowRenderPasses, uint32 numSpotLightShadowRenderPasses,
	const point_shadow_render_pass** pointLightShadowRenderPasses, uint32 numPointLightShadowRenderPasses)
{
	if (numSunLightShadowRenderPasses || numSpotLightShadowRenderPasses || numPointLightShadowRenderPasses)
	{
		PROFILE_ALL(cl, "Shadow map pass");

		clear_rect clearRects[128];
		uint32 numClearRects = 0;

		shadow_map_viewport copiesFromStaticCache[128];
		uint32 numCopiesFromStaticCache = 0;

		shadow_map_viewport copiesToStaticCache[128];
		uint32 numCopiesToStaticCache = 0;

		{
			for (uint32 passIndex = 0; passIndex < numSunLightShadowRenderPasses; ++passIndex)
			{
				auto pass = sunShadowRenderPasses[passIndex];

				for (uint32 cascadeIndex = 0; cascadeIndex < pass->numCascades; ++cascadeIndex)
				{
					const sun_cascade_render_pass& cascade = pass->cascades[cascadeIndex];
					shadow_map_viewport vp = cascade.viewport;

					if (pass->copyFromStaticCache)
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

		if (!enableStaticShadowMapCaching)
		{
			numCopiesToStaticCache = 0;
		}

		if (numCopiesFromStaticCache)
		{
			PROFILE_ALL(cl, "Copy from static shadow map cache");
			copyShadowMapParts(cl, render_resources::staticShadowMapCache, render_resources::shadowMap, copiesFromStaticCache, numCopiesFromStaticCache);
		}

		if (numClearRects)
		{
			cl->clearDepth(render_resources::shadowMap->defaultDSV, 1.f, clearRects, numClearRects);
		}

		if (numCopiesToStaticCache)
		{
			barrier_batcher(cl)
				.transitionBegin(render_resources::staticShadowMapCache, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}



		auto shadowRenderTarget = dx_render_target(SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT)
			.depthAttachment(render_resources::shadowMap);

		cl->setRenderTarget(shadowRenderTarget);

		{
			PROFILE_ALL(cl, "Static geometry");

			if (numSunLightShadowRenderPasses || numSpotLightShadowRenderPasses)
			{
				cl->setPipelineState(*shadowPipeline.pipeline);
				cl->setGraphicsRootSignature(*shadowPipeline.rootSignature);

				sunShadowPassInternal(cl, sunShadowRenderPasses, numSunLightShadowRenderPasses, true, false);
				spotShadowPassInternal(cl, spotLightShadowRenderPasses, numSpotLightShadowRenderPasses, true, false);


				cl->setPipelineState(*doubleSidedShadowPipeline.pipeline);
				cl->setGraphicsRootSignature(*doubleSidedShadowPipeline.rootSignature);

				sunShadowPassInternal(cl, sunShadowRenderPasses, numSunLightShadowRenderPasses, true, true);
				spotShadowPassInternal(cl, spotLightShadowRenderPasses, numSpotLightShadowRenderPasses, true, true);
			}

			if (numPointLightShadowRenderPasses)
			{
				cl->setPipelineState(*pointLightShadowPipeline.pipeline);
				cl->setGraphicsRootSignature(*pointLightShadowPipeline.rootSignature);

				pointShadowPassInternal(cl, pointLightShadowRenderPasses, numPointLightShadowRenderPasses, true, false);


				cl->setPipelineState(*doubleSidedPointLightShadowPipeline.pipeline);
				cl->setGraphicsRootSignature(*doubleSidedPointLightShadowPipeline.rootSignature);

				pointShadowPassInternal(cl, pointLightShadowRenderPasses, numPointLightShadowRenderPasses, true, true);
			}
		}

		if (numCopiesToStaticCache)
		{
			PROFILE_ALL(cl, "Copy to static shadow map cache");

			barrier_batcher(cl)
				.transition(render_resources::shadowMap, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
				.transitionEnd(render_resources::staticShadowMapCache, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);

			copyShadowMapParts(cl, render_resources::shadowMap, render_resources::staticShadowMapCache, copiesToStaticCache, numCopiesToStaticCache);

			barrier_batcher(cl)
				.transition(render_resources::shadowMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE)
				.transition(render_resources::staticShadowMapCache, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}


		cl->setRenderTarget(shadowRenderTarget);

		{
			PROFILE_ALL(cl, "Dynamic geometry");

			if (numSunLightShadowRenderPasses || numSpotLightShadowRenderPasses)
			{
				cl->setPipelineState(*shadowPipeline.pipeline);
				cl->setGraphicsRootSignature(*shadowPipeline.rootSignature);

				sunShadowPassInternal(cl, sunShadowRenderPasses, numSunLightShadowRenderPasses, false, false);
				spotShadowPassInternal(cl, spotLightShadowRenderPasses, numSpotLightShadowRenderPasses, false, false);


				cl->setPipelineState(*doubleSidedShadowPipeline.pipeline);
				cl->setGraphicsRootSignature(*doubleSidedShadowPipeline.rootSignature);

				sunShadowPassInternal(cl, sunShadowRenderPasses, numSunLightShadowRenderPasses, false, true);
				spotShadowPassInternal(cl, spotLightShadowRenderPasses, numSpotLightShadowRenderPasses, false, true);
			}

			if (numPointLightShadowRenderPasses)
			{
				cl->setPipelineState(*pointLightShadowPipeline.pipeline);
				cl->setGraphicsRootSignature(*pointLightShadowPipeline.rootSignature);

				pointShadowPassInternal(cl, pointLightShadowRenderPasses, numPointLightShadowRenderPasses, false, false);


				cl->setPipelineState(*doubleSidedPointLightShadowPipeline.pipeline);
				cl->setGraphicsRootSignature(*doubleSidedPointLightShadowPipeline.rootSignature);

				pointShadowPassInternal(cl, pointLightShadowRenderPasses, numPointLightShadowRenderPasses, false, true);
			}
		}
	}
}

void opaqueLightPass(dx_command_list* cl,
	const dx_render_target& renderTarget,
	const opaque_render_pass* opaqueRenderPass,
	const common_material_info& materialInfo,
	const mat4& viewProj)
{
	if (opaqueRenderPass && opaqueRenderPass->pass.size() > 0)
	{
		PROFILE_ALL(cl, "Main opaque light pass");

		cl->setRenderTarget(renderTarget);
		cl->setViewport(renderTarget.viewport);

		pipeline_setup_func lastSetupFunc = 0;

		for (const auto& dc : opaqueRenderPass->pass)
		{
			if (dc.setup != lastSetupFunc)
			{
				dc.setup(cl, materialInfo);
				lastSetupFunc = dc.setup;
			}
			dc.render(cl, viewProj, dc.data);
		}
	}
}

void transparentLightPass(dx_command_list* cl,
	const dx_render_target& renderTarget,
	const transparent_render_pass* transparentRenderPass,
	const common_material_info& materialInfo,
	const mat4& viewProj)
{
	if (transparentRenderPass && transparentRenderPass->pass.size() > 0)
	{
		PROFILE_ALL(cl, "Transparent light pass");

		cl->setRenderTarget(renderTarget);
		cl->setViewport(renderTarget.viewport);

		pipeline_setup_func lastSetupFunc = 0;

		for (const auto& dc : transparentRenderPass->pass)
		{
			if (dc.setup != lastSetupFunc)
			{
				dc.setup(cl, materialInfo);
				lastSetupFunc = dc.setup;
			}
			dc.render(cl, viewProj, dc.data);
		}
	}
}

void ldrPass(dx_command_list* cl,
	const dx_render_target& ldrRenderTarget,
	ref<dx_texture> depthStencilBuffer,
	const ldr_render_pass* ldrRenderPass,
	const common_material_info& materialInfo,
	const mat4& viewProj)
{
	if (ldrRenderPass)
	{
		PROFILE_ALL(cl, "LDR pass");

		cl->setRenderTarget(ldrRenderTarget);
		cl->setViewport(ldrRenderTarget.viewport);


		if (ldrRenderPass->ldrPass.size())
		{
			PROFILE_ALL(cl, "LDR Objects");

			pipeline_setup_func lastSetupFunc = 0;

			for (const auto& dc : ldrRenderPass->ldrPass)
			{
				if (dc.setup != lastSetupFunc)
				{
					dc.setup(cl, materialInfo);
					lastSetupFunc = dc.setup;
				}
				dc.render(cl, viewProj, dc.data);
			}
		}

		if (ldrRenderPass->overlays.size())
		{
			PROFILE_ALL(cl, "3D Overlays");

			cl->clearDepth(ldrRenderTarget.dsv);

			pipeline_setup_func lastSetupFunc = 0;

			for (const auto& dc : ldrRenderPass->overlays)
			{
				if (dc.setup != lastSetupFunc)
				{
					dc.setup(cl, materialInfo);
					lastSetupFunc = dc.setup;
				}
				dc.render(cl, viewProj, dc.data);
			}
		}

		if (ldrRenderPass->outlines.size())
		{
			PROFILE_ALL(cl, "Outlines");

			cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cl->setStencilReference(stencil_flag_selected_object);

			cl->setPipelineState(*outlineMarkerPipeline.pipeline);
			cl->setGraphicsRootSignature(*outlineMarkerPipeline.rootSignature);


			// Mark objects in stencil.
			for (const auto& dc : ldrRenderPass->outlines)
			{
				cl->setGraphics32BitConstants(OUTLINE_RS_MVP, outline_marker_cb{ viewProj * dc.transform });

				cl->setVertexBuffer(0, dc.vertexBuffer);
				cl->setIndexBuffer(dc.indexBuffer);
				cl->drawIndexed(dc.submesh.numIndices, 1, dc.submesh.firstIndex, dc.submesh.baseVertex, 0);
			}

			// Draw outline.
			cl->transitionBarrier(depthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ);

			cl->setPipelineState(*outlineDrawerPipeline.pipeline);
			cl->setGraphicsRootSignature(*outlineDrawerPipeline.rootSignature);

			cl->setGraphics32BitConstants(OUTLINE_RS_CB, outline_drawer_cb{ (int)depthStencilBuffer->width, (int)depthStencilBuffer->height });
			cl->setDescriptorHeapResource(OUTLINE_RS_STENCIL, 0, 1, depthStencilBuffer->stencilSRV);

			cl->drawFullscreenTriangle();

			cl->transitionBarrier(depthStencilBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}
	}
}

void copyShadowMapParts(dx_command_list* cl,
	ref<dx_texture> from,
	ref<dx_texture> to,
	shadow_map_viewport* copies, uint32 numCopies)
{
	// Since copies from or to parts of a depth-stencil texture are not allowed (even though they work on at least some hardware),
	// we copy to and from the static shadow map cache via a shader, and not via CopyTextureRegion.

	auto shadowRenderTarget = dx_render_target(to->width, to->height)
		.depthAttachment(to);

	cl->setRenderTarget(shadowRenderTarget);

	cl->setPipelineState(*shadowMapCopyPipeline.pipeline);
	cl->setGraphicsRootSignature(*shadowMapCopyPipeline.rootSignature);

	cl->setDescriptorHeapSRV(1, 0, from);

	for (uint32 i = 0; i < numCopies; ++i)
	{
		shadow_map_viewport vp = copies[i];
		cl->setGraphics32BitConstants(0, vec4((float)vp.x / SHADOW_MAP_WIDTH, (float)vp.y / SHADOW_MAP_HEIGHT, (float)vp.size / SHADOW_MAP_WIDTH, (float)vp.size / SHADOW_MAP_HEIGHT));
		cl->setViewport(vp.x, vp.y, vp.size, vp.size);
		cl->drawFullscreenTriangle();
	}
}


void lightAndDecalCulling(dx_command_list* cl,
	ref<dx_texture> depthStencilBuffer,
	ref<dx_buffer> pointLights,
	ref<dx_buffer> spotLights,
	ref<dx_buffer> decals,
	light_culling culling,
	uint32 numPointLights, uint32 numSpotLights, uint32 numDecals,
	dx_dynamic_constant_buffer cameraCBV)
{
	if (numPointLights || numSpotLights || numDecals)
	{
		PROFILE_ALL(cl, "Cull lights & decals");

		// Tiled frusta.
		{
			PROFILE_ALL(cl, "Create world space frusta");

			cl->setPipelineState(*worldSpaceFrustaPipeline.pipeline);
			cl->setComputeRootSignature(*worldSpaceFrustaPipeline.rootSignature);
			cl->setComputeDynamicConstantBuffer(WORLD_SPACE_TILED_FRUSTA_RS_CAMERA, cameraCBV);
			cl->setCompute32BitConstants(WORLD_SPACE_TILED_FRUSTA_RS_CB, frusta_cb{ culling.numCullingTilesX, culling.numCullingTilesY });
			cl->setRootComputeUAV(WORLD_SPACE_TILED_FRUSTA_RS_FRUSTA_UAV, culling.tiledWorldSpaceFrustaBuffer);
			cl->dispatch(bucketize(culling.numCullingTilesX, 16), bucketize(culling.numCullingTilesY, 16));
		}

		barrier_batcher(cl)
			.uav(culling.tiledWorldSpaceFrustaBuffer);

		// Culling.
		{
			PROFILE_ALL(cl, "Sort objects into tiles");

			cl->clearUAV(culling.tiledCullingIndexCounter, 0.f);
			//cl->uavBarrier(tiledCullingIndexCounter);
			cl->setPipelineState(*lightCullingPipeline.pipeline);
			cl->setComputeRootSignature(*lightCullingPipeline.rootSignature);
			cl->setComputeDynamicConstantBuffer(LIGHT_CULLING_RS_CAMERA, cameraCBV);
			cl->setCompute32BitConstants(LIGHT_CULLING_RS_CB, light_culling_cb{ culling.numCullingTilesX, numPointLights, numSpotLights, numDecals });
			cl->setDescriptorHeapSRV(LIGHT_CULLING_RS_SRV_UAV, 0, depthStencilBuffer);
			cl->setDescriptorHeapSRV(LIGHT_CULLING_RS_SRV_UAV, 1, culling.tiledWorldSpaceFrustaBuffer);
			cl->setDescriptorHeapSRV(LIGHT_CULLING_RS_SRV_UAV, 2, pointLights ? pointLights->defaultSRV : render_resources::nullBufferSRV);
			cl->setDescriptorHeapSRV(LIGHT_CULLING_RS_SRV_UAV, 3, spotLights ? spotLights->defaultSRV : render_resources::nullBufferSRV);
			cl->setDescriptorHeapSRV(LIGHT_CULLING_RS_SRV_UAV, 4, decals ? decals->defaultSRV : render_resources::nullBufferSRV);
			cl->setDescriptorHeapUAV(LIGHT_CULLING_RS_SRV_UAV, 5, culling.tiledCullingGrid);
			cl->setDescriptorHeapUAV(LIGHT_CULLING_RS_SRV_UAV, 6, culling.tiledCullingIndexCounter);
			cl->setDescriptorHeapUAV(LIGHT_CULLING_RS_SRV_UAV, 7, culling.tiledObjectsIndexList);
			cl->dispatch(culling.numCullingTilesX, culling.numCullingTilesY);
		}

		barrier_batcher(cl)
			.uav(culling.tiledCullingGrid)
			.uav(culling.tiledObjectsIndexList);
	}
}

void linearDepthPyramid(dx_command_list* cl,
	ref<dx_texture> depthStencilBuffer,
	ref<dx_texture> linearDepthBuffer,
	vec4 projectionParams)
{
	PROFILE_ALL(cl, "Linear depth pyramid");

	cl->setPipelineState(*hierarchicalLinearDepthPipeline.pipeline);
	cl->setComputeRootSignature(*hierarchicalLinearDepthPipeline.rootSignature);

	float width = ceilf(depthStencilBuffer->width * 0.5f);
	float height = ceilf(depthStencilBuffer->height * 0.5f);

	cl->setCompute32BitConstants(HIERARCHICAL_LINEAR_DEPTH_RS_CB, hierarchical_linear_depth_cb{ projectionParams, vec2(1.f / width, 1.f / height) });
	cl->setDescriptorHeapUAV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 0, linearDepthBuffer->uavAt(0));
	cl->setDescriptorHeapUAV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 1, linearDepthBuffer->uavAt(1));
	cl->setDescriptorHeapUAV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 2, linearDepthBuffer->uavAt(2));
	cl->setDescriptorHeapUAV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 3, linearDepthBuffer->uavAt(3));
	cl->setDescriptorHeapUAV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 4, linearDepthBuffer->uavAt(4));
	cl->setDescriptorHeapUAV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 5, linearDepthBuffer->uavAt(5));
	cl->setDescriptorHeapSRV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 6, depthStencilBuffer);

	cl->dispatch(bucketize((uint32)width, POST_PROCESSING_BLOCK_SIZE), bucketize((uint32)height, POST_PROCESSING_BLOCK_SIZE));
}

void gaussianBlur(dx_command_list* cl,
	ref<dx_texture> inputOutput,
	ref<dx_texture> temp,
	uint32 inputMip, uint32 outputMip, gaussian_blur_kernel_size kernel, uint32 numIterations)
{
	PROFILE_ALL(cl, "Gaussian Blur");

	assert(inputOutput->format == temp->format);

	uint32 numChannels = getNumberOfChannels(inputOutput->format);

	// Maybe we could add more specializations for 2 and 3 channels.
	dx_pipeline& pipeline = ((numChannels == 1) ? gaussianBlurFloatPipelines : gaussianBlurFloat4Pipelines)[kernel];

	cl->setPipelineState(*pipeline.pipeline);
	cl->setComputeRootSignature(*pipeline.rootSignature);

	uint32 outputWidth = max(1u, inputOutput->width >> outputMip);
	uint32 outputHeight = max(1u, inputOutput->height >> outputMip);

	uint32 widthBuckets = bucketize(outputWidth, POST_PROCESSING_BLOCK_SIZE);
	uint32 heightBuckets = bucketize(outputHeight, POST_PROCESSING_BLOCK_SIZE);

	assert(inputMip <= outputMip); // Currently only downsampling supported.

	float scale = 1.f / (1 << (outputMip - inputMip));

	uint32 sourceMip = inputMip;
	gaussian_blur_cb cb = { vec2(1.f / outputWidth, 1.f / outputHeight), scale };

	for (uint32 i = 0; i < numIterations; ++i)
	{
		PROFILE_ALL(cl, "Iteration");

		{
			PROFILE_ALL(cl, "Vertical");

			dx_cpu_descriptor_handle tempUAV = temp->uavAt(outputMip);

			// Vertical pass.
			cb.directionAndSourceMipLevel = (1 << 16) | sourceMip;
			cl->setCompute32BitConstants(GAUSSIAN_BLUR_RS_CB, cb);
			cl->setDescriptorHeapUAV(GAUSSIAN_BLUR_RS_TEXTURES, 0, tempUAV);
			cl->setDescriptorHeapSRV(GAUSSIAN_BLUR_RS_TEXTURES, 1, inputOutput);

			cl->dispatch(widthBuckets, heightBuckets);

			barrier_batcher(cl)
				//.uav(temp)
				.transition(temp, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.transition(inputOutput, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		cb.stepScale = 1.f;
		sourceMip = outputMip; // From here on we sample from the output mip.

		{
			PROFILE_ALL(cl, "Horizontal");

			dx_cpu_descriptor_handle outputUAV = inputOutput->uavAt(outputMip);

			// Horizontal pass.
			cb.directionAndSourceMipLevel = (0 << 16) | sourceMip;
			cl->setCompute32BitConstants(GAUSSIAN_BLUR_RS_CB, cb);
			cl->setDescriptorHeapUAV(GAUSSIAN_BLUR_RS_TEXTURES, 0, outputUAV);
			cl->setDescriptorHeapSRV(GAUSSIAN_BLUR_RS_TEXTURES, 1, temp);

			cl->dispatch(widthBuckets, heightBuckets);

			barrier_batcher(cl)
				//.uav(inputOutput)
				.transition(temp, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				.transition(inputOutput, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}
	}
}

static void morphologyCommon(dx_command_list* cl, dx_pipeline& pipeline, ref<dx_texture> inputOutput, ref<dx_texture> temp, uint32 radius, uint32 numIterations)
{
	if (radius == 0 || numIterations == 0)
	{
		return;
	}

	assert(inputOutput->width == temp->width);
	assert(inputOutput->height == temp->height);

	radius = min(radius, (uint32)MORPHOLOGY_MAX_RADIUS);

	cl->setPipelineState(*pipeline.pipeline);
	cl->setComputeRootSignature(*pipeline.rootSignature);

	for (uint32 i = 0; i < numIterations; ++i)
	{
		DX_PROFILE_BLOCK(cl, "Iteration");

		{
			DX_PROFILE_BLOCK(cl, "Vertical");

			cl->setCompute32BitConstants(MORPHOLOGY_RS_CB, morphology_cb{ radius, 1, inputOutput->height });
			cl->setDescriptorHeapUAV(MORPHOLOGY_RS_TEXTURES, 0, temp);
			cl->setDescriptorHeapSRV(MORPHOLOGY_RS_TEXTURES, 1, inputOutput);

			cl->dispatch(bucketize(inputOutput->height, MORPHOLOGY_BLOCK_SIZE), inputOutput->width);

			barrier_batcher(cl)
				//.uav(temp)
				.transition(temp, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
				.transition(inputOutput, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		{
			DX_PROFILE_BLOCK(cl, "Horizontal");

			cl->setCompute32BitConstants(MORPHOLOGY_RS_CB, morphology_cb{ radius, 0, inputOutput->width });
			cl->setDescriptorHeapUAV(MORPHOLOGY_RS_TEXTURES, 0, inputOutput);
			cl->setDescriptorHeapSRV(MORPHOLOGY_RS_TEXTURES, 1, temp);

			cl->dispatch(bucketize(inputOutput->width, MORPHOLOGY_BLOCK_SIZE), inputOutput->height);

			barrier_batcher(cl)
				//.uav(inputOutput)
				.transition(temp, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				.transition(inputOutput, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}
	}
}

void dilate(dx_command_list* cl, ref<dx_texture> inputOutput, ref<dx_texture> temp, uint32 radius, uint32 numIterations)
{
	DX_PROFILE_BLOCK(cl, "Dilate");
	morphologyCommon(cl, dilationPipeline, inputOutput, temp, radius, numIterations);
}

void erode(dx_command_list* cl, ref<dx_texture> inputOutput, ref<dx_texture> temp, uint32 radius, uint32 numIterations)
{
	DX_PROFILE_BLOCK(cl, "Erode");
	morphologyCommon(cl, erosionPipeline, inputOutput, temp, radius, numIterations);
}

void depthSobel(dx_command_list* cl, ref<dx_texture> input, ref<dx_texture> output, vec4 projectionParams, float threshold)
{
	DX_PROFILE_BLOCK(cl, "Depth sobel");

	cl->setPipelineState(*depthSobelPipeline.pipeline);
	cl->setComputeRootSignature(*depthSobelPipeline.rootSignature);

	cl->setCompute32BitConstants(DEPTH_SOBEL_RS_CB, depth_sobel_cb{ projectionParams, threshold });
	cl->setDescriptorHeapUAV(DEPTH_SOBEL_RS_TEXTURES, 0, output);
	cl->setDescriptorHeapSRV(DEPTH_SOBEL_RS_TEXTURES, 1, input);

	cl->dispatch(bucketize(input->width, POST_PROCESSING_BLOCK_SIZE), bucketize(input->height, POST_PROCESSING_BLOCK_SIZE));
}

void screenSpaceReflections(dx_command_list* cl,
	ref<dx_texture> prevFrameHDR,
	ref<dx_texture> depthStencilBuffer,
	ref<dx_texture> linearDepthBuffer,
	ref<dx_texture> worldNormalsRoughnessTexture,
	ref<dx_texture> screenVelocitiesTexture,
	ref<dx_texture> raycastTexture,
	ref<dx_texture> resolveTexture,
	ref<dx_texture> ssrTemporalHistory,
	ref<dx_texture> ssrTemporalOutput,
	ssr_settings settings,
	dx_dynamic_constant_buffer cameraCBV)
{
	PROFILE_ALL(cl, "Screen space reflections");

	uint32 raycastWidth = raycastTexture->width;
	uint32 raycastHeight = raycastTexture->height;

	uint32 resolveWidth = resolveTexture->width;
	uint32 resolveHeight = resolveTexture->height;

	{
		PROFILE_ALL(cl, "Raycast");

		cl->setPipelineState(*ssrRaycastPipeline.pipeline);
		cl->setComputeRootSignature(*ssrRaycastPipeline.rootSignature);

		ssr_raycast_cb raycastSettings;
		raycastSettings.numSteps = settings.numSteps;
		raycastSettings.maxDistance = settings.maxDistance;
		raycastSettings.strideCutoff = settings.strideCutoff;
		raycastSettings.minStride = settings.minStride;
		raycastSettings.maxStride = settings.maxStride;
		raycastSettings.dimensions = vec2((float)raycastWidth, (float)raycastHeight);
		raycastSettings.invDimensions = vec2(1.f / raycastWidth, 1.f / raycastHeight);
		raycastSettings.frameIndex = (uint32)dxContext.frameID;

		cl->setCompute32BitConstants(SSR_RAYCAST_RS_CB, raycastSettings);
		cl->setComputeDynamicConstantBuffer(SSR_RAYCAST_RS_CAMERA, cameraCBV);
		cl->setDescriptorHeapUAV(SSR_RAYCAST_RS_TEXTURES, 0, raycastTexture);
		cl->setDescriptorHeapSRV(SSR_RAYCAST_RS_TEXTURES, 1, depthStencilBuffer);
		cl->setDescriptorHeapSRV(SSR_RAYCAST_RS_TEXTURES, 2, linearDepthBuffer);
		cl->setDescriptorHeapSRV(SSR_RAYCAST_RS_TEXTURES, 3, worldNormalsRoughnessTexture);
		cl->setDescriptorHeapSRV(SSR_RAYCAST_RS_TEXTURES, 4, render_resources::noiseTexture);
		cl->setDescriptorHeapSRV(SSR_RAYCAST_RS_TEXTURES, 5, screenVelocitiesTexture);

		cl->dispatch(bucketize(raycastWidth, SSR_BLOCK_SIZE), bucketize(raycastHeight, SSR_BLOCK_SIZE));

		barrier_batcher(cl)
			//.uav(raycastTexture)
			.transition(raycastTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	{
		PROFILE_ALL(cl, "Resolve");

		cl->setPipelineState(*ssrResolvePipeline.pipeline);
		cl->setComputeRootSignature(*ssrResolvePipeline.rootSignature);

		cl->setCompute32BitConstants(SSR_RESOLVE_RS_CB, ssr_resolve_cb{ vec2((float)resolveWidth, (float)resolveHeight), vec2(1.f / resolveWidth, 1.f / resolveHeight) });
		cl->setComputeDynamicConstantBuffer(SSR_RESOLVE_RS_CAMERA, cameraCBV);

		cl->setDescriptorHeapUAV(SSR_RESOLVE_RS_TEXTURES, 0, resolveTexture);
		cl->setDescriptorHeapSRV(SSR_RESOLVE_RS_TEXTURES, 1, depthStencilBuffer);
		cl->setDescriptorHeapSRV(SSR_RESOLVE_RS_TEXTURES, 2, worldNormalsRoughnessTexture);
		cl->setDescriptorHeapSRV(SSR_RESOLVE_RS_TEXTURES, 3, raycastTexture);
		cl->setDescriptorHeapSRV(SSR_RESOLVE_RS_TEXTURES, 4, prevFrameHDR);
		cl->setDescriptorHeapSRV(SSR_RESOLVE_RS_TEXTURES, 5, screenVelocitiesTexture);

		cl->dispatch(bucketize(resolveWidth, SSR_BLOCK_SIZE), bucketize(resolveHeight, SSR_BLOCK_SIZE));

		barrier_batcher(cl)
			//.uav(resolveTexture)
			.transition(resolveTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			.transition(raycastTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}


	{
		PROFILE_ALL(cl, "Temporal");

		cl->setPipelineState(*ssrTemporalPipeline.pipeline);
		cl->setComputeRootSignature(*ssrTemporalPipeline.rootSignature);

		cl->setCompute32BitConstants(SSR_TEMPORAL_RS_CB, ssr_temporal_cb{ vec2(1.f / resolveWidth, 1.f / resolveHeight) });

		cl->setDescriptorHeapUAV(SSR_TEMPORAL_RS_TEXTURES, 0, ssrTemporalOutput);
		cl->setDescriptorHeapSRV(SSR_TEMPORAL_RS_TEXTURES, 1, resolveTexture);
		cl->setDescriptorHeapSRV(SSR_TEMPORAL_RS_TEXTURES, 2, ssrTemporalHistory);
		cl->setDescriptorHeapSRV(SSR_TEMPORAL_RS_TEXTURES, 3, screenVelocitiesTexture);

		cl->dispatch(bucketize(resolveWidth, SSR_BLOCK_SIZE), bucketize(resolveHeight, SSR_BLOCK_SIZE));

		barrier_batcher(cl)
			//.uav(ssrOutput)
			.transition(ssrTemporalOutput, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			.transition(ssrTemporalHistory, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.transition(resolveTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	{
		PROFILE_ALL(cl, "Median Blur");

		cl->setPipelineState(*ssrMedianBlurPipeline.pipeline);
		cl->setComputeRootSignature(*ssrMedianBlurPipeline.rootSignature);

		cl->setCompute32BitConstants(SSR_MEDIAN_BLUR_RS_CB, ssr_median_blur_cb{ vec2(1.f / resolveWidth, 1.f / resolveHeight) });

		cl->setDescriptorHeapUAV(SSR_MEDIAN_BLUR_RS_TEXTURES, 0, resolveTexture); // We reuse the resolve texture here.
		cl->setDescriptorHeapSRV(SSR_MEDIAN_BLUR_RS_TEXTURES, 1, ssrTemporalOutput);

		cl->dispatch(bucketize(resolveWidth, SSR_BLOCK_SIZE), bucketize(resolveHeight, SSR_BLOCK_SIZE));

		barrier_batcher(cl)
			//.uav(resolveTexture)
			.transition(resolveTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
	}
}

void specularAmbient(dx_command_list* cl,
	ref<dx_texture> hdrInput,
	ref<dx_texture> ssr,
	ref<dx_texture> worldNormalsTexture,
	ref<dx_texture> reflectanceTexture,
	ref<dx_texture> environment,
	ref<dx_texture> ao,
	ref<dx_texture> output,
	dx_dynamic_constant_buffer cameraCBV)
{
	PROFILE_ALL(cl, "Specular ambient");

	cl->setPipelineState(*specularAmbientPipeline.pipeline);
	cl->setComputeRootSignature(*specularAmbientPipeline.rootSignature);

	cl->setCompute32BitConstants(SPECULAR_AMBIENT_RS_CB, specular_ambient_cb{ vec2(1.f / output->width, 1.f / output->height) });
	cl->setComputeDynamicConstantBuffer(SPECULAR_AMBIENT_RS_CAMERA, cameraCBV);

	cl->setDescriptorHeapUAV(SPECULAR_AMBIENT_RS_TEXTURES, 0, output);
	cl->setDescriptorHeapSRV(SPECULAR_AMBIENT_RS_TEXTURES, 1, hdrInput);
	cl->setDescriptorHeapSRV(SPECULAR_AMBIENT_RS_TEXTURES, 2, worldNormalsTexture);
	cl->setDescriptorHeapSRV(SPECULAR_AMBIENT_RS_TEXTURES, 3, reflectanceTexture);
	cl->setDescriptorHeapSRV(SPECULAR_AMBIENT_RS_TEXTURES, 4, ssr ? ssr->defaultSRV : render_resources::nullTextureSRV);
	cl->setDescriptorHeapSRV(SPECULAR_AMBIENT_RS_TEXTURES, 5, environment ? environment : render_resources::blackCubeTexture);
	cl->setDescriptorHeapSRV(SPECULAR_AMBIENT_RS_TEXTURES, 6, render_resources::brdfTex);
	cl->setDescriptorHeapSRV(SPECULAR_AMBIENT_RS_TEXTURES, 7, ao ? ao : render_resources::whiteTexture);

	cl->dispatch(bucketize(output->width, POST_PROCESSING_BLOCK_SIZE), bucketize(output->height, POST_PROCESSING_BLOCK_SIZE));
}


void temporalAntiAliasing(dx_command_list* cl,
	ref<dx_texture> hdrInput,
	ref<dx_texture> screenVelocitiesTexture,
	ref<dx_texture> depthStencilBuffer,
	ref<dx_texture> history,
	ref<dx_texture> output,
	vec4 jitteredCameraProjectionParams)
{
	PROFILE_ALL(cl, "Temporal anti-aliasing");

	cl->setPipelineState(*taaPipeline.pipeline);
	cl->setComputeRootSignature(*taaPipeline.rootSignature);

	cl->setDescriptorHeapUAV(TAA_RS_TEXTURES, 0, output);
	cl->setDescriptorHeapSRV(TAA_RS_TEXTURES, 1, hdrInput);
	cl->setDescriptorHeapSRV(TAA_RS_TEXTURES, 2, history);
	cl->setDescriptorHeapSRV(TAA_RS_TEXTURES, 3, screenVelocitiesTexture);
	cl->setDescriptorHeapSRV(TAA_RS_TEXTURES, 4, depthStencilBuffer);

	uint32 renderWidth = depthStencilBuffer->width;
	uint32 renderHeight = depthStencilBuffer->height;
	cl->setCompute32BitConstants(TAA_RS_CB, taa_cb{ jitteredCameraProjectionParams, vec2((float)renderWidth, (float)renderHeight) });

	cl->dispatch(bucketize(renderWidth, POST_PROCESSING_BLOCK_SIZE), bucketize(renderHeight, POST_PROCESSING_BLOCK_SIZE));

	barrier_batcher(cl)
		//.uav(taaTextures[taaOutputIndex])
		.transition(output, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) // Will be read by rest of post processing stack. Can stay in read state, since it is read as history next frame.
		.transition(history, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS); // Will be used as UAV next frame.
}

void downsample(dx_command_list* cl,
	ref<dx_texture> input,
	ref<dx_texture> output,
	ref<dx_texture> temp)
{
	PROFILE_ALL(cl, "Downsample");

	barrier_batcher(cl)
		.transition(output, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	blit(cl, input, output);

	barrier_batcher(cl)
		//.uav(prevFrameHDRColorTexture)
		.transition(output, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	for (uint32 i = 0; i < output->numMipLevels - 1; ++i)
	{
		gaussianBlur(cl, output, temp, i, i + 1, gaussian_blur_5x5);
	}
}

void bloom(dx_command_list* cl,
	ref<dx_texture> hdrInput,
	ref<dx_texture> output,
	ref<dx_texture> bloomTexture,
	ref<dx_texture> bloomTempTexture,
	bloom_settings settings)
{
	PROFILE_ALL(cl, "Bloom");

	{
		PROFILE_ALL(cl, "Threshold");

		cl->setPipelineState(*bloomThresholdPipeline.pipeline);
		cl->setComputeRootSignature(*bloomThresholdPipeline.rootSignature);

		cl->setDescriptorHeapUAV(BLOOM_THRESHOLD_RS_TEXTURES, 0, bloomTexture);
		cl->setDescriptorHeapSRV(BLOOM_THRESHOLD_RS_TEXTURES, 1, hdrInput);

		cl->setCompute32BitConstants(BLOOM_THRESHOLD_RS_CB, bloom_threshold_cb{ vec2(1.f / bloomTexture->width, 1.f / bloomTexture->height), settings.threshold });

		cl->dispatch(bucketize(bloomTexture->width, POST_PROCESSING_BLOCK_SIZE), bucketize(bloomTexture->height, POST_PROCESSING_BLOCK_SIZE));
	}

	barrier_batcher(cl)
		.transition(bloomTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	for (uint32 i = 0; i < bloomTexture->numMipLevels - 1; ++i)
	{
		gaussianBlur(cl, bloomTexture, bloomTempTexture, i, i + 1, gaussian_blur_9x9);
	}

	{
		PROFILE_ALL(cl, "Combine");

		cl->setPipelineState(*bloomCombinePipeline.pipeline);
		cl->setComputeRootSignature(*bloomCombinePipeline.rootSignature);

		cl->setDescriptorHeapUAV(BLOOM_COMBINE_RS_TEXTURES, 0, output);
		cl->setDescriptorHeapSRV(BLOOM_COMBINE_RS_TEXTURES, 1, hdrInput);
		cl->setDescriptorHeapSRV(BLOOM_COMBINE_RS_TEXTURES, 2, bloomTexture);

		cl->setCompute32BitConstants(BLOOM_COMBINE_RS_CB, bloom_combine_cb{ vec2(1.f / hdrInput->width, 1.f / hdrInput->height), settings.strength });

		cl->dispatch(bucketize(hdrInput->width, POST_PROCESSING_BLOCK_SIZE), bucketize(hdrInput->height, POST_PROCESSING_BLOCK_SIZE));
	}

	barrier_batcher(cl)
		//.uav(bloomResult)
		.transition(output, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) // Will be read by rest of post processing stack. 
		.transition(bloomTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS); // For next frame.
}

static void bilateralBlurShadows(dx_command_list* cl,
	ref<dx_texture> input,
	ref<dx_texture> tempTexture,
	ref<dx_texture> linearDepth,
	ref<dx_texture> screenVelocitiesTexture,
	ref<dx_texture> history,
	ref<dx_texture> output)
{
	PROFILE_ALL(cl, "Bilateral blur");

	{
		PROFILE_ALL(cl, "Horizontal");

		shadow_blur_cb cb;
		cb.dimensions.x = (float)tempTexture->width;
		cb.dimensions.y = (float)tempTexture->height;
		cb.invDimensions.x = 1.f / cb.dimensions.x;
		cb.invDimensions.y = 1.f / cb.dimensions.y;

		cl->setPipelineState(*shadowBlurXPipeline.pipeline);
		cl->setComputeRootSignature(*shadowBlurXPipeline.rootSignature);

		cl->setDescriptorHeapUAV(SHADOW_BLUR_RS_TEXTURES, 0, tempTexture);
		cl->setDescriptorHeapSRV(SHADOW_BLUR_RS_TEXTURES, 1, input);
		cl->setDescriptorHeapSRV(SHADOW_BLUR_RS_TEXTURES, 2, linearDepth);

		cl->setCompute32BitConstants(SHADOW_BLUR_RS_CB, cb);

		cl->dispatch(bucketize(tempTexture->width, POST_PROCESSING_BLOCK_SIZE), bucketize(tempTexture->height, POST_PROCESSING_BLOCK_SIZE));

		barrier_batcher(cl)
			//.uav(tempTexture)
			.transition(tempTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			.transitionBegin(input, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	{
		PROFILE_ALL(cl, "Vertical");

		shadow_blur_cb cb;
		cb.dimensions.x = (float)output->width;
		cb.dimensions.y = (float)output->height;
		cb.invDimensions.x = 1.f / cb.dimensions.x;
		cb.invDimensions.y = 1.f / cb.dimensions.y;

		cl->setPipelineState(*shadowBlurYPipeline.pipeline);
		cl->setComputeRootSignature(*shadowBlurYPipeline.rootSignature);

		cl->setDescriptorHeapUAV(SHADOW_BLUR_RS_TEXTURES, 0, output);
		cl->setDescriptorHeapSRV(SHADOW_BLUR_RS_TEXTURES, 1, tempTexture);
		cl->setDescriptorHeapSRV(SHADOW_BLUR_RS_TEXTURES, 2, linearDepth);
		cl->setDescriptorHeapSRV(SHADOW_BLUR_RS_TEXTURES, 3, screenVelocitiesTexture);
		cl->setDescriptorHeapSRV(SHADOW_BLUR_RS_TEXTURES, 4, history);

		cl->setCompute32BitConstants(SHADOW_BLUR_RS_CB, cb);

		cl->dispatch(bucketize(output->width, POST_PROCESSING_BLOCK_SIZE), bucketize(output->height, POST_PROCESSING_BLOCK_SIZE));

		barrier_batcher(cl)
			.uav(output)
			.transition(tempTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
			.transitionEnd(input, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}
}

void ambientOcclusion(dx_command_list* cl,
	ref<dx_texture> linearDepth,
	ref<dx_texture> screenVelocitiesTexture,
	ref<dx_texture> aoCalculationTexture,
	ref<dx_texture> aoBlurTempTexture,
	ref<dx_texture> history,
	ref<dx_texture> output,
	hbao_settings settings,
	dx_dynamic_constant_buffer cameraCBV)
{
	PROFILE_ALL(cl, "HBAO");

	{
		PROFILE_ALL(cl, "Calculate AO");

		hbao_cb cb;
		cb.screenWidth = aoCalculationTexture->width;
		cb.screenHeight = aoCalculationTexture->height;
		cb.depthBufferMipLevel = log2((float)linearDepth->width / aoCalculationTexture->width);
		cb.numRays = settings.numRays;
		cb.maxNumStepsPerRay = settings.maxNumStepsPerRay;
		cb.strength = settings.strength;
		cb.radius = settings.radius;
		cb.seed = (float)(dxContext.frameID & 0xFFFFFFFF);

		float alpha = M_TAU / settings.numRays;
		cb.rayDeltaRotation = vec2(cos(alpha), sin(alpha));

		cl->setPipelineState(*hbaoPipeline.pipeline);
		cl->setComputeRootSignature(*hbaoPipeline.rootSignature);

		cl->setDescriptorHeapUAV(HBAO_RS_TEXTURES, 0, aoCalculationTexture);
		cl->setDescriptorHeapSRV(HBAO_RS_TEXTURES, 1, linearDepth);

		cl->setCompute32BitConstants(HBAO_RS_CB, cb);
		cl->setComputeDynamicConstantBuffer(HBAO_RS_CAMERA, cameraCBV);

		cl->dispatch(bucketize(aoCalculationTexture->width, POST_PROCESSING_BLOCK_SIZE), bucketize(aoCalculationTexture->height, POST_PROCESSING_BLOCK_SIZE));

		barrier_batcher(cl)
			//.uav(aoCalculationTexture)
			.transition(aoCalculationTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	bilateralBlurShadows(cl, aoCalculationTexture, aoBlurTempTexture, linearDepth, screenVelocitiesTexture, history, output);
}

void screenSpaceShadows(dx_command_list* cl, 
	ref<dx_texture> linearDepth, 
	ref<dx_texture> screenVelocitiesTexture, 
	ref<dx_texture> sssCalculationTexture, 
	ref<dx_texture> sssBlurTempTexture, 
	ref<dx_texture> history, 
	ref<dx_texture> output, 
	vec3 sunDirection, 
	sss_settings settings, 
	const mat4& view, 
	dx_dynamic_constant_buffer cameraCBV)
{
	PROFILE_ALL(cl, "Screen space shadows");

	{
		PROFILE_ALL(cl, "Calculate SSS");

		cl->setPipelineState(*sssPipeline.pipeline);
		cl->setComputeRootSignature(*sssPipeline.rootSignature);

		sss_cb cb;
		cb.invDimensions = vec2(1.f / (float)sssCalculationTexture->width, 1.f / (float)sssCalculationTexture->height);
		cb.lightDirection = -normalize(transformDirection(view, sunDirection));
		cb.rayDistance = settings.rayDistance;
		cb.numSteps = settings.numSteps;
		cb.thickness = settings.thickness;
		cb.maxDistanceFromCamera = settings.maxDistanceFromCamera;
		cb.distanceFadeoutRange = settings.distanceFadeoutRange;
		cb.invBorderFadeout = 1.f / settings.borderFadeout;
		cb.seed = (float)(dxContext.frameID & 0xFFFFFFFF);

		cl->setCompute32BitConstants(SSS_RS_CB, cb);
		cl->setComputeDynamicConstantBuffer(SSS_RS_CAMERA, cameraCBV);
		cl->setDescriptorHeapUAV(SSS_RS_TEXTURES, 0, sssCalculationTexture);
		cl->setDescriptorHeapSRV(SSS_RS_TEXTURES, 1, linearDepth);

		cl->dispatch(bucketize(sssCalculationTexture->width, POST_PROCESSING_BLOCK_SIZE), bucketize(sssCalculationTexture->height, POST_PROCESSING_BLOCK_SIZE));

		barrier_batcher(cl)
			//.uav(sssCalculationTexture)
			.transition(sssCalculationTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	bilateralBlurShadows(cl, sssCalculationTexture, sssBlurTempTexture, linearDepth, screenVelocitiesTexture, history, output);
}

void tonemap(dx_command_list* cl,
	ref<dx_texture> hdrInput,
	ref<dx_texture> ldrOutput,
	const tonemap_settings& settings)
{
	PROFILE_ALL(cl, "Tonemapping");

	tonemap_cb cb;
	cb.A = settings.A;
	cb.B = settings.B;
	cb.C = settings.C;
	cb.D = settings.D;
	cb.E = settings.E;
	cb.F = settings.F;
	cb.invEvaluatedLinearWhite = 1.f / settings.evaluate(settings.linearWhite);
	cb.expExposure = exp2(settings.exposure);

	cl->setPipelineState(*tonemapPipeline.pipeline);
	cl->setComputeRootSignature(*tonemapPipeline.rootSignature);

	cl->setDescriptorHeapUAV(TONEMAP_RS_TEXTURES, 0, ldrOutput);
	cl->setDescriptorHeapSRV(TONEMAP_RS_TEXTURES, 1, hdrInput);
	cl->setCompute32BitConstants(TONEMAP_RS_CB, cb);

	cl->dispatch(bucketize(ldrOutput->width, POST_PROCESSING_BLOCK_SIZE), bucketize(ldrOutput->height, POST_PROCESSING_BLOCK_SIZE));
}

void blit(dx_command_list* cl, ref<dx_texture> input, ref<dx_texture> output)
{
	DX_PROFILE_BLOCK(cl, "Blit");

	cl->setPipelineState(*blitPipeline.pipeline);
	cl->setComputeRootSignature(*blitPipeline.rootSignature);

	cl->setCompute32BitConstants(BLIT_RS_CB, blit_cb{ vec2(1.f / output->width, 1.f / output->height) });
	cl->setDescriptorHeapUAV(BLIT_RS_TEXTURES, 0, output);
	cl->setDescriptorHeapSRV(BLIT_RS_TEXTURES, 1, input);

	cl->dispatch(bucketize(output->width, POST_PROCESSING_BLOCK_SIZE), bucketize(output->height, POST_PROCESSING_BLOCK_SIZE));
}

void present(dx_command_list* cl,
	ref<dx_texture> ldrInput,
	ref<dx_texture> output,
	sharpen_settings sharpenSettings)
{
	PROFILE_ALL(cl, "Present");

	uint32 xOffset = (output->width - ldrInput->width) / 2;
	uint32 yOffset = (output->height - ldrInput->height) / 2;

	cl->setPipelineState(*presentPipeline.pipeline);
	cl->setComputeRootSignature(*presentPipeline.rootSignature);

	cl->setDescriptorHeapUAV(PRESENT_RS_TEXTURES, 0, output);
	cl->setDescriptorHeapSRV(PRESENT_RS_TEXTURES, 1, ldrInput);
	cl->setCompute32BitConstants(PRESENT_RS_CB, present_cb{ present_sdr, 0.f, sharpenSettings.strength, (xOffset << 16) | yOffset });

	cl->dispatch(bucketize(output->width, POST_PROCESSING_BLOCK_SIZE), bucketize(output->height, POST_PROCESSING_BLOCK_SIZE));
}

void visualizeSunShadowCascades(dx_command_list* cl, 
	ref<dx_texture> depthBuffer, 
	ref<dx_texture> output, 
	dx_dynamic_constant_buffer sunCBV, 
	const mat4& invViewProj, 
	vec3 cameraPosition, 
	vec3 cameraForward)
{
	PROFILE_ALL(cl, "Visualize sun shadow cascades");

	cl->setPipelineState(*visualizeSunShadowCascadesPipeline.pipeline);
	cl->setComputeRootSignature(*visualizeSunShadowCascadesPipeline.rootSignature);

	visualize_sun_shadow_cascades_cb cb;
	cb.invViewProj = invViewProj;
	cb.cameraPosition = vec4(cameraPosition, 1.f);
	cb.cameraForward = vec4(cameraForward, 0.f);

	cl->setDescriptorHeapUAV(VISUALIZE_SUN_SHADOW_CASCADES_RS_TEXTURES, 0, output);
	cl->setDescriptorHeapSRV(VISUALIZE_SUN_SHADOW_CASCADES_RS_TEXTURES, 1, depthBuffer);
	cl->setComputeDynamicConstantBuffer(VISUALIZE_SUN_SHADOW_CASCADES_RS_SUN, sunCBV);
	cl->setCompute32BitConstants(VISUALIZE_SUN_SHADOW_CASCADES_RS_CB, cb);

	cl->dispatch(bucketize(output->width, 16), bucketize(output->height, 16));
}

void light_culling::allocateIfNecessary(uint32 renderWidth, uint32 renderHeight)
{
	uint32 oldNumCullingTilesX = numCullingTilesX;
	uint32 oldNumCullingTilesY = numCullingTilesY;

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
	else if (numCullingTilesX != oldNumCullingTilesX || numCullingTilesY != oldNumCullingTilesY)
	{
		resizeTexture(tiledCullingGrid, numCullingTilesX, numCullingTilesY);
		resizeBuffer(tiledObjectsIndexList, numCullingTilesX * numCullingTilesY * MAX_NUM_INDICES_PER_TILE * 2);
		resizeBuffer(tiledWorldSpaceFrustaBuffer, numCullingTilesX * numCullingTilesY);
	}
}
