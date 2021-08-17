#include "pch.h"
#include "render_algorithms.h"
#include "dx/dx_profiling.h"
#include "dx/dx_barrier_batcher.h"

#include "render_resources.h"

#include "post_processing_rs.hlsli"
#include "ssr_rs.hlsli"
#include "light_culling_rs.hlsli"
#include "depth_only_rs.hlsli"
#include "sky_rs.hlsli"
#include "outline_rs.hlsli"
#include "particles_rs.hlsli"
#include "transform.hlsli"

static dx_pipeline shadowMapCopyPipeline;

static dx_pipeline worldSpaceFrustaPipeline;
static dx_pipeline lightCullingPipeline;

static dx_pipeline ssrRaycastPipeline;
static dx_pipeline ssrResolvePipeline;
static dx_pipeline ssrTemporalPipeline;
static dx_pipeline ssrMedianBlurPipeline;

static dx_pipeline specularAmbientPipeline;

static dx_pipeline hierarchicalLinearDepthPipeline;

static dx_pipeline gaussianBlur5x5Pipeline;
static dx_pipeline gaussianBlur9x9Pipeline;
static dx_pipeline gaussianBlur13x13Pipeline;

static dx_pipeline taaPipeline;

static dx_pipeline blitPipeline;

static dx_pipeline bloomThresholdPipeline;
static dx_pipeline bloomCombinePipeline;

static dx_pipeline tonemapPipeline;
static dx_pipeline presentPipeline;


void loadCommonShaders()
{
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(0, 0, render_resources::shadowDepthFormat)
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

	gaussianBlur5x5Pipeline = createReloadablePipeline("gaussian_blur_5x5_cs");
	gaussianBlur9x9Pipeline = createReloadablePipeline("gaussian_blur_9x9_cs");
	gaussianBlur13x13Pipeline = createReloadablePipeline("gaussian_blur_13x13_cs");

	taaPipeline = createReloadablePipeline("taa_cs");

	blitPipeline = createReloadablePipeline("blit_cs");

	bloomThresholdPipeline = createReloadablePipeline("bloom_threshold_cs");
	bloomCombinePipeline = createReloadablePipeline("bloom_combine_cs");

	tonemapPipeline = createReloadablePipeline("tonemap_cs");
	presentPipeline = createReloadablePipeline("present_cs");
}


void depthPrePass(dx_command_list* cl,
	const dx_render_target& depthOnlyRenderTarget,
	const dx_pipeline& rigidPipeline,
	const dx_pipeline& animatedPipeline,
	const opaque_render_pass* opaqueRenderPass,
	const mat4& viewProj, const mat4& prevFrameViewProj,
	vec2 jitter, vec2 prevFrameJitter)
{
	DX_PROFILE_BLOCK(cl, "Depth pre-pass");

	cl->setRenderTarget(depthOnlyRenderTarget);
	cl->setViewport(depthOnlyRenderTarget.viewport);

	depth_only_camera_jitter_cb jitterCB = { jitter, prevFrameJitter };

	// Static.
	if (opaqueRenderPass && opaqueRenderPass->staticDepthOnlyDrawCalls.size() > 0)
	{
		DX_PROFILE_BLOCK(cl, "Static");

		cl->setPipelineState(*rigidPipeline.pipeline);
		cl->setGraphicsRootSignature(*rigidPipeline.rootSignature);

		cl->setGraphics32BitConstants(DEPTH_ONLY_RS_CAMERA_JITTER, jitterCB);

		for (const auto& dc : opaqueRenderPass->staticDepthOnlyDrawCalls)
		{
			const mat4& m = dc.transform;
			const submesh_info& submesh = dc.submesh;

			cl->setGraphics32BitConstants(DEPTH_ONLY_RS_OBJECT_ID, (uint32)dc.objectID);
			cl->setGraphics32BitConstants(DEPTH_ONLY_RS_MVP, depth_only_transform_cb{ viewProj * m, prevFrameViewProj * m });

			cl->setVertexBuffer(0, dc.vertexBuffer);
			cl->setIndexBuffer(dc.indexBuffer);
			cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
		}
	}

	// Dynamic.
	if (opaqueRenderPass && opaqueRenderPass->dynamicDepthOnlyDrawCalls.size() > 0)
	{
		DX_PROFILE_BLOCK(cl, "Dynamic");

		cl->setPipelineState(*rigidPipeline.pipeline);
		cl->setGraphicsRootSignature(*rigidPipeline.rootSignature);

		cl->setGraphics32BitConstants(DEPTH_ONLY_RS_CAMERA_JITTER, jitterCB);

		for (const auto& dc : opaqueRenderPass->dynamicDepthOnlyDrawCalls)
		{
			const mat4& m = dc.transform;
			const mat4& prevFrameM = dc.prevFrameTransform;
			const submesh_info& submesh = dc.submesh;

			cl->setGraphics32BitConstants(DEPTH_ONLY_RS_OBJECT_ID, (uint32)dc.objectID);
			cl->setGraphics32BitConstants(DEPTH_ONLY_RS_MVP, depth_only_transform_cb{ viewProj * m, prevFrameViewProj * prevFrameM });

			cl->setVertexBuffer(0, dc.vertexBuffer);
			cl->setIndexBuffer(dc.indexBuffer);
			cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
		}
	}

	// Animated.
	if (opaqueRenderPass && opaqueRenderPass->animatedDepthOnlyDrawCalls.size() > 0)
	{
		DX_PROFILE_BLOCK(cl, "Animated");

		cl->setPipelineState(*animatedPipeline.pipeline);
		cl->setGraphicsRootSignature(*animatedPipeline.rootSignature);

		cl->setGraphics32BitConstants(DEPTH_ONLY_RS_CAMERA_JITTER, jitterCB);

		for (const auto& dc : opaqueRenderPass->animatedDepthOnlyDrawCalls)
		{
			const mat4& m = dc.transform;
			const mat4& prevFrameM = dc.prevFrameTransform;
			const submesh_info& submesh = dc.submesh;
			const submesh_info& prevFrameSubmesh = dc.prevFrameSubmesh;
			const ref<dx_vertex_buffer>& prevFrameVertexBuffer = dc.prevFrameVertexBuffer;

			cl->setGraphics32BitConstants(DEPTH_ONLY_RS_OBJECT_ID, (uint32)dc.objectID);
			cl->setGraphics32BitConstants(DEPTH_ONLY_RS_MVP, depth_only_transform_cb{ viewProj * m, prevFrameViewProj * prevFrameM });
			cl->setRootGraphicsSRV(DEPTH_ONLY_RS_PREV_FRAME_POSITIONS, prevFrameVertexBuffer->gpuVirtualAddress + prevFrameSubmesh.baseVertex * prevFrameVertexBuffer->elementSize);

			cl->setVertexBuffer(0, dc.vertexBuffer);
			cl->setIndexBuffer(dc.indexBuffer);
			cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
		}
	}
}

void texturedSky(dx_command_list* cl,
	const dx_render_target& skyRenderTarget,
	const dx_pipeline& skyPipeline, 
	const mat4& proj, const mat4& view,
	ref<dx_texture> sky,
	float skyIntensity)
{
	DX_PROFILE_BLOCK(cl, "Sky");

	cl->setRenderTarget(skyRenderTarget);
	cl->setViewport(skyRenderTarget.viewport);

	cl->setPipelineState(*skyPipeline.pipeline);
	cl->setGraphicsRootSignature(*skyPipeline.rootSignature);

	cl->setGraphics32BitConstants(SKY_RS_VP, sky_transform_cb{ proj * createSkyViewMatrix(view) });
	cl->setGraphics32BitConstants(SKY_RS_INTENSITY, skyIntensity);
	cl->setDescriptorHeapSRV(SKY_RS_TEX, 0, sky);

	cl->drawCubeTriangleStrip();
}

void proceduralSky(dx_command_list* cl,
	const dx_render_target& skyRenderTarget,
	const dx_pipeline& skyPipeline,
	const mat4& proj, const mat4& view,
	float skyIntensity)
{
	DX_PROFILE_BLOCK(cl, "Sky");

	cl->setRenderTarget(skyRenderTarget);
	cl->setViewport(skyRenderTarget.viewport);

	cl->setPipelineState(*skyPipeline.pipeline);
	cl->setGraphicsRootSignature(*skyPipeline.rootSignature);

	cl->setGraphics32BitConstants(SKY_RS_VP, sky_transform_cb{ proj * createSkyViewMatrix(view) });
	cl->setGraphics32BitConstants(SKY_RS_INTENSITY, skyIntensity);

	cl->drawCubeTriangleStrip();
}

void sphericalHarmonicsSky(dx_command_list* cl, 
	const dx_render_target& skyRenderTarget, 
	const dx_pipeline& skyPipeline, 
	const mat4& proj, const mat4& view, 
	const ref<dx_buffer>& sh, uint32 shIndex, 
	float skyIntensity)
{
	DX_PROFILE_BLOCK(cl, "Sky");

	cl->setRenderTarget(skyRenderTarget);
	cl->setViewport(skyRenderTarget.viewport);

	cl->setPipelineState(*skyPipeline.pipeline);
	cl->setGraphicsRootSignature(*skyPipeline.rootSignature);

	cl->setGraphics32BitConstants(SKY_RS_VP, sky_transform_cb{ proj * createSkyViewMatrix(view) });
	cl->setGraphics32BitConstants(SKY_RS_INTENSITY, skyIntensity);
	cl->setRootGraphicsSRV(SKY_RS_SH, sh->gpuVirtualAddress + sizeof(spherical_harmonics) * shIndex);

	cl->drawCubeTriangleStrip();
}

void preethamSky(dx_command_list* cl, 
	const dx_render_target& skyRenderTarget, 
	const dx_pipeline& skyPipeline, 
	const mat4& proj, const mat4& view, 
	vec3 sunDirection, float skyIntensity)
{
	DX_PROFILE_BLOCK(cl, "Sky");

	cl->setRenderTarget(skyRenderTarget);
	cl->setViewport(skyRenderTarget.viewport);

	cl->setPipelineState(*skyPipeline.pipeline);
	cl->setGraphicsRootSignature(*skyPipeline.rootSignature);

	cl->setGraphics32BitConstants(SKY_RS_VP, sky_transform_cb{ proj * createSkyViewMatrix(view) });
	cl->setGraphics32BitConstants(SKY_RS_INTENSITY, sky_cb{ skyIntensity, sunDirection });

	cl->drawCubeTriangleStrip();
}

void shadowPasses(dx_command_list* cl,
	const dx_pipeline& shadowPipeline,
	const dx_pipeline& pointLightShadowPipeline,
	const sun_shadow_render_pass* sunShadowRenderPass, const directional_light_cb& sun,
	const spot_shadow_render_pass** spotLightShadowRenderPasses, uint32 numSpotLightShadowRenderPasses,
	const point_shadow_render_pass** pointLightShadowRenderPasses, uint32 numPointLightShadowRenderPasses)
{
	if (sunShadowRenderPass || numSpotLightShadowRenderPasses || numPointLightShadowRenderPasses)
	{
		DX_PROFILE_BLOCK(cl, "Shadow map pass");

		clear_rect clearRects[128];
		uint32 numClearRects = 0;

		shadow_map_viewport copiesFromStaticCache[128];
		uint32 numCopiesFromStaticCache = 0;

		shadow_map_viewport copiesToStaticCache[128];
		uint32 numCopiesToStaticCache = 0;

		{
			if (sunShadowRenderPass)
			{
				for (uint32 i = 0; i < sun.numShadowCascades; ++i)
				{
					shadow_map_viewport vp = sunShadowRenderPass->viewports[i];
					if (sunShadowRenderPass->copyFromStaticCache)
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

		if (numCopiesFromStaticCache)
		{
			DX_PROFILE_BLOCK(cl, "Copy from static shadow map cache");
			copyShadowMapParts(cl, render_resources::staticShadowMapCache, render_resources::shadowMap, copiesFromStaticCache, numCopiesFromStaticCache);
		}

		if (numClearRects)
		{
			cl->clearDepth(render_resources::shadowMap->dsvHandle, 1.f, clearRects, numClearRects);
		}

		if (numCopiesToStaticCache)
		{
			barrier_batcher(cl)
				.transitionBegin(render_resources::staticShadowMapCache, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}


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


		dx_render_target shadowRenderTarget({}, render_resources::shadowMap);
		cl->setRenderTarget(shadowRenderTarget);

		if (sunShadowRenderPass || numSpotLightShadowRenderPasses)
		{
			cl->setPipelineState(*shadowPipeline.pipeline);
			cl->setGraphicsRootSignature(*shadowPipeline.rootSignature);
		}

		if (sunShadowRenderPass)
		{
			DX_PROFILE_BLOCK(cl, "Sun static geometry");

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

		if (numSpotLightShadowRenderPasses)
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

		if (numPointLightShadowRenderPasses)
		{
			DX_PROFILE_BLOCK(cl, "Point lights static geometry");

			cl->setPipelineState(*pointLightShadowPipeline.pipeline);
			cl->setGraphicsRootSignature(*pointLightShadowPipeline.rootSignature);

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

		if (numCopiesToStaticCache)
		{
			DX_PROFILE_BLOCK(cl, "Copy to static shadow map cache");

			barrier_batcher(cl)
				.transition(render_resources::shadowMap, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
				.transitionEnd(render_resources::staticShadowMapCache, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);

			copyShadowMapParts(cl, render_resources::shadowMap, render_resources::staticShadowMapCache, copiesToStaticCache, numCopiesToStaticCache);

			barrier_batcher(cl)
				.transition(render_resources::shadowMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE)
				.transition(render_resources::staticShadowMapCache, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}


		cl->setRenderTarget(shadowRenderTarget);

		if (sunShadowRenderPass || numSpotLightShadowRenderPasses)
		{
			cl->setPipelineState(*shadowPipeline.pipeline);
			cl->setGraphicsRootSignature(*shadowPipeline.rootSignature);
		}

		if (sunShadowRenderPass)
		{
			DX_PROFILE_BLOCK(cl, "Sun dynamic geometry");

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

		if (numSpotLightShadowRenderPasses)
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

		if (numPointLightShadowRenderPasses)
		{
			DX_PROFILE_BLOCK(cl, "Point lights dynamic geometry");

			cl->setPipelineState(*pointLightShadowPipeline.pipeline);
			cl->setGraphicsRootSignature(*pointLightShadowPipeline.rootSignature);

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
}

void opaqueLightPass(dx_command_list* cl,
	const dx_render_target& renderTarget,
	const opaque_render_pass* opaqueRenderPass,
	const common_material_info& materialInfo, 
	const mat4& viewProj)
{
	if (opaqueRenderPass && opaqueRenderPass->drawCalls.size() > 0)
	{
		DX_PROFILE_BLOCK(cl, "Main opaque light pass");

		cl->setRenderTarget(renderTarget);
		cl->setViewport(renderTarget.viewport);

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

			cl->setGraphics32BitConstants(0, transform_cb{ viewProj * m, m });

			cl->setVertexBuffer(0, dc.vertexBuffer.positions);
			cl->setVertexBuffer(1, dc.vertexBuffer.others);
			cl->setIndexBuffer(dc.indexBuffer);
			cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
		}
	}
}

void transparentLightPass(dx_command_list* cl,
	const dx_render_target& renderTarget, 
	const transparent_render_pass* transparentRenderPass,
	const common_material_info& materialInfo, 
	const dx_command_signature& particleCommandSignature,
	const mat4& viewProj)
{
	if (transparentRenderPass && (transparentRenderPass->drawCalls.size() > 0 || transparentRenderPass->particleDrawCalls.size() > 0))
	{
		DX_PROFILE_BLOCK(cl, "Transparent light pass");

		cl->setRenderTarget(renderTarget);
		cl->setViewport(renderTarget.viewport);


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

				cl->setGraphics32BitConstants(0, transform_cb{ viewProj * m, m });

				cl->setVertexBuffer(0, dc.vertexBuffer.positions);
				cl->setVertexBuffer(1, dc.vertexBuffer.others);
				cl->setIndexBuffer(dc.indexBuffer);
				cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
			}
		}

		if (transparentRenderPass->particleDrawCalls.size() > 0)
		{
			DX_PROFILE_BLOCK(cl, "Particles");

			for (const auto& dc : transparentRenderPass->particleDrawCalls)
			{
				if (dc.materialSetupFunc != lastSetupFunc)
				{
					dc.materialSetupFunc(cl, materialInfo);
					lastSetupFunc = dc.materialSetupFunc;
				}

				dc.material->prepareForRendering(cl);

				const particle_draw_info& info = dc.drawInfo;

				cl->setRootGraphicsSRV(info.rootParameterOffset + PARTICLE_RENDERING_RS_PARTICLES, info.particleBuffer->gpuVirtualAddress);
				cl->setRootGraphicsSRV(info.rootParameterOffset + PARTICLE_RENDERING_RS_ALIVE_LIST, info.aliveList->gpuVirtualAddress + info.aliveListOffset);

				cl->setVertexBuffer(0, dc.vertexBuffer.positions);
				if (dc.vertexBuffer.others)
				{
					cl->setVertexBuffer(1, dc.vertexBuffer.others);
				}
				cl->setIndexBuffer(dc.indexBuffer);

				cl->drawIndirect(particleCommandSignature, 1, info.commandBuffer, info.commandBufferOffset);
			}
		}
	}
}

void overlays(dx_command_list* cl,
	const dx_render_target& ldrRenderTarget,
	overlay_render_pass* overlayRenderPass,
	const common_material_info& materialInfo,
	const mat4& viewProj)
{
	DX_PROFILE_BLOCK(cl, "3D Overlays");

	cl->setRenderTarget(ldrRenderTarget);
	cl->setViewport(ldrRenderTarget.viewport);

	cl->clearDepth(ldrRenderTarget.dsv);

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
			cl->setGraphics32BitConstants(0, transform_cb{ viewProj * m, m });
		}

		if (dc.drawType == geometry_render_pass::draw_type_default)
		{
			const submesh_info& submesh = dc.submesh;

			cl->setVertexBuffer(0, dc.vertexBuffer.positions);
			cl->setVertexBuffer(1, dc.vertexBuffer.others);
			cl->setIndexBuffer(dc.indexBuffer);
			cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
		}
		else
		{
			cl->dispatchMesh(dc.dispatchInfo.dispatchX, dc.dispatchInfo.dispatchY, dc.dispatchInfo.dispatchZ);
		}
	}
}

void outlines(dx_command_list* cl, 
	const dx_render_target& ldrRenderTarget, 
	ref<dx_texture> depthStencilBuffer,
	const dx_pipeline& outlineMarkerPipeline,
	const dx_pipeline& outlineDrawerPipeline,
	const opaque_render_pass* opaqueRenderPass,
	const mat4& viewProj,
	uint32 stencilBit)
{
	DX_PROFILE_BLOCK(cl, "Outlines");

	cl->setRenderTarget(ldrRenderTarget);
	cl->setViewport(ldrRenderTarget.viewport);

	cl->setStencilReference(stencilBit);

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

			cl->setVertexBuffer(0, vertexBuffer.positions);
			cl->setIndexBuffer(indexBuffer);
			cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);
		}
	};

	mark(*opaqueRenderPass, cl, viewProj);
	//mark(*transparentRenderPass, cl, unjitteredCamera.viewProj);

	// Draw outline.
	cl->transitionBarrier(depthStencilBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ);

	cl->setPipelineState(*outlineDrawerPipeline.pipeline);
	cl->setGraphicsRootSignature(*outlineDrawerPipeline.rootSignature);

	cl->setGraphics32BitConstants(OUTLINE_RS_CB, outline_drawer_cb{ (int)depthStencilBuffer->width, (int)depthStencilBuffer->height });
	cl->setDescriptorHeapResource(OUTLINE_RS_STENCIL, 0, 1, depthStencilBuffer->stencilSRV);

	cl->drawFullscreenTriangle();

	cl->transitionBarrier(depthStencilBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void copyShadowMapParts(dx_command_list* cl, 
	ref<dx_texture> from,
	ref<dx_texture> to,
	shadow_map_viewport* copies, uint32 numCopies)
{
	// Since copies from or to parts of a depth-stencil texture are not allowed (even though they work on at least some hardware),
	// we copy to and from the static shadow map cache via a shader, and not via CopyTextureRegion.

	dx_render_target shadowRenderTarget({}, to);
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
		DX_PROFILE_BLOCK(cl, "Cull lights & decals");

		// Tiled frusta.
		{
			DX_PROFILE_BLOCK(cl, "Create world space frusta");

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
			DX_PROFILE_BLOCK(cl, "Sort objects into tiles");

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
	DX_PROFILE_BLOCK(cl, "Linear depth pyramid");

	cl->setPipelineState(*hierarchicalLinearDepthPipeline.pipeline);
	cl->setComputeRootSignature(*hierarchicalLinearDepthPipeline.rootSignature);

	float width = ceilf(depthStencilBuffer->width * 0.5f);
	float height = ceilf(depthStencilBuffer->height * 0.5f);

	cl->setCompute32BitConstants(HIERARCHICAL_LINEAR_DEPTH_RS_CB, hierarchical_linear_depth_cb{ projectionParams, vec2(1.f / width, 1.f / height) });
	cl->setDescriptorHeapUAV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 0, linearDepthBuffer->defaultUAV);
	cl->setDescriptorHeapUAV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 1, linearDepthBuffer->mipUAVs[0]);
	cl->setDescriptorHeapUAV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 2, linearDepthBuffer->mipUAVs[1]);
	cl->setDescriptorHeapUAV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 3, linearDepthBuffer->mipUAVs[2]);
	cl->setDescriptorHeapUAV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 4, linearDepthBuffer->mipUAVs[3]);
	cl->setDescriptorHeapUAV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 5, linearDepthBuffer->mipUAVs[4]);
	cl->setDescriptorHeapSRV(HIERARCHICAL_LINEAR_DEPTH_RS_TEXTURES, 6, depthStencilBuffer);

	cl->dispatch(bucketize((uint32)width, POST_PROCESSING_BLOCK_SIZE), bucketize((uint32)height, POST_PROCESSING_BLOCK_SIZE));
}

void gaussianBlur(dx_command_list* cl,
	ref<dx_texture> inputOutput,
	ref<dx_texture> temp,
	uint32 inputMip, uint32 outputMip, gaussian_blur_kernel_size kernel, uint32 numIterations)
{
	DX_PROFILE_BLOCK(cl, "Gaussian Blur");

	auto& pipeline =
		(kernel == gaussian_blur_5x5) ? gaussianBlur5x5Pipeline :
		(kernel == gaussian_blur_9x9) ? gaussianBlur9x9Pipeline :
		(kernel == gaussian_blur_13x13) ? gaussianBlur13x13Pipeline :
		gaussianBlur5x5Pipeline; // TODO: Emit error!

	cl->setPipelineState(*pipeline.pipeline);
	cl->setComputeRootSignature(*pipeline.rootSignature);

	uint32 outputWidth = max(1, inputOutput->width >> outputMip);
	uint32 outputHeight = max(1, inputOutput->height >> outputMip);

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
				//.uav(temp)
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
				//.uav(inputOutput)
				.transition(temp, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
				.transition(inputOutput, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}
	}
}

void screenSpaceReflections(dx_command_list* cl,
	ref<dx_texture> hdrInput,
	ref<dx_texture> prevFrameHDR,
	ref<dx_texture> depthStencilBuffer,
	ref<dx_texture> linearDepthBuffer,
	ref<dx_texture> worldNormalsTexture,
	ref<dx_texture> reflectanceTexture,
	ref<dx_texture> screenVelocitiesTexture,
	ref<dx_texture> raycastTexture,
	ref<dx_texture> resolveTexture,
	ref<dx_texture> ssrTemporalHistory,
	ref<dx_texture> ssrTemporalOutput,
	ssr_settings settings,
	dx_dynamic_constant_buffer cameraCBV)
{
	DX_PROFILE_BLOCK(cl, "Screen space reflections");

	uint32 raycastWidth = raycastTexture->width;
	uint32 raycastHeight = raycastTexture->height;

	uint32 resolveWidth = resolveTexture->width;
	uint32 resolveHeight = resolveTexture->height;

	{
		DX_PROFILE_BLOCK(cl, "Raycast");

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
		cl->setDescriptorHeapSRV(SSR_RAYCAST_RS_TEXTURES, 3, worldNormalsTexture);
		cl->setDescriptorHeapSRV(SSR_RAYCAST_RS_TEXTURES, 4, reflectanceTexture);
		cl->setDescriptorHeapSRV(SSR_RAYCAST_RS_TEXTURES, 5, render_resources::noiseTexture);

		cl->dispatch(bucketize(raycastWidth, SSR_BLOCK_SIZE), bucketize(raycastHeight, SSR_BLOCK_SIZE));

		barrier_batcher(cl)
			//.uav(raycastTexture)
			.transition(raycastTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	{
		DX_PROFILE_BLOCK(cl, "Resolve");

		cl->setPipelineState(*ssrResolvePipeline.pipeline);
		cl->setComputeRootSignature(*ssrResolvePipeline.rootSignature);

		cl->setCompute32BitConstants(SSR_RESOLVE_RS_CB, ssr_resolve_cb{ vec2((float)resolveWidth, (float)resolveHeight), vec2(1.f / resolveWidth, 1.f / resolveHeight) });
		cl->setComputeDynamicConstantBuffer(SSR_RESOLVE_RS_CAMERA, cameraCBV);

		cl->setDescriptorHeapUAV(SSR_RESOLVE_RS_TEXTURES, 0, resolveTexture);
		cl->setDescriptorHeapSRV(SSR_RESOLVE_RS_TEXTURES, 1, depthStencilBuffer);
		cl->setDescriptorHeapSRV(SSR_RESOLVE_RS_TEXTURES, 2, worldNormalsTexture);
		cl->setDescriptorHeapSRV(SSR_RESOLVE_RS_TEXTURES, 3, reflectanceTexture);
		cl->setDescriptorHeapSRV(SSR_RESOLVE_RS_TEXTURES, 4, raycastTexture);
		cl->setDescriptorHeapSRV(SSR_RESOLVE_RS_TEXTURES, 5, prevFrameHDR);
		cl->setDescriptorHeapSRV(SSR_RESOLVE_RS_TEXTURES, 6, screenVelocitiesTexture);

		cl->dispatch(bucketize(resolveWidth, SSR_BLOCK_SIZE), bucketize(resolveHeight, SSR_BLOCK_SIZE));

		barrier_batcher(cl)
			//.uav(resolveTexture)
			.transition(resolveTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
			.transition(raycastTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}


	{
		DX_PROFILE_BLOCK(cl, "Temporal");

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
		DX_PROFILE_BLOCK(cl, "Median Blur");

		cl->setPipelineState(*ssrMedianBlurPipeline.pipeline);
		cl->setComputeRootSignature(*ssrMedianBlurPipeline.rootSignature);

		cl->setCompute32BitConstants(SSR_MEDIAN_BLUR_RS_CB, ssr_median_blur_cb{ vec2(1.f / resolveWidth, 1.f / resolveHeight) });

		cl->setDescriptorHeapUAV(SSR_MEDIAN_BLUR_RS_TEXTURES, 0, resolveTexture); // We reuse the resolve texture here.
		cl->setDescriptorHeapSRV(SSR_MEDIAN_BLUR_RS_TEXTURES, 1, ssrTemporalOutput);

		cl->dispatch(bucketize(resolveWidth, SSR_BLOCK_SIZE), bucketize(resolveHeight, SSR_BLOCK_SIZE));

		barrier_batcher(cl)
			//.uav(resolveTexture)
			.transition(resolveTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}
}

void specularAmbient(dx_command_list* cl,
	ref<dx_texture> hdrInput, 
	ref<dx_texture> ssr,
	ref<dx_texture> worldNormalsTexture,
	ref<dx_texture> reflectanceTexture,
	ref<dx_texture> environment,
	ref<dx_texture> output,
	dx_dynamic_constant_buffer cameraCBV)
{
	DX_PROFILE_BLOCK(cl, "Specular ambient");

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
	DX_PROFILE_BLOCK(cl, "Temporal anti-aliasing");

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
	DX_PROFILE_BLOCK(cl, "Downsample");

	barrier_batcher(cl)
		.transition(output, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	cl->setPipelineState(*blitPipeline.pipeline);
	cl->setComputeRootSignature(*blitPipeline.rootSignature);

	cl->setCompute32BitConstants(BLIT_RS_CB, blit_cb{ vec2(1.f / output->width, 1.f / output->height) });
	cl->setDescriptorHeapUAV(BLIT_RS_TEXTURES, 0, output);
	cl->setDescriptorHeapSRV(BLIT_RS_TEXTURES, 1, input);

	cl->dispatch(bucketize(output->width, POST_PROCESSING_BLOCK_SIZE), bucketize(output->height, POST_PROCESSING_BLOCK_SIZE));

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
	DX_PROFILE_BLOCK(cl, "Bloom");

	{
		DX_PROFILE_BLOCK(cl, "Threshold");

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
		DX_PROFILE_BLOCK(cl, "Combine");

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

void tonemap(dx_command_list* cl,
	ref<dx_texture> hdrInput,
	ref<dx_texture> ldrOutput,
	const tonemap_settings& settings)
{
	DX_PROFILE_BLOCK(cl, "Tonemapping");

	cl->setPipelineState(*tonemapPipeline.pipeline);
	cl->setComputeRootSignature(*tonemapPipeline.rootSignature);

	cl->setDescriptorHeapUAV(TONEMAP_RS_TEXTURES, 0, ldrOutput);
	cl->setDescriptorHeapSRV(TONEMAP_RS_TEXTURES, 1, hdrInput);
	cl->setCompute32BitConstants(TONEMAP_RS_CB, settings); // Settings struct is identical to tonemap_cb.

	cl->dispatch(bucketize(ldrOutput->width, POST_PROCESSING_BLOCK_SIZE), bucketize(ldrOutput->height, POST_PROCESSING_BLOCK_SIZE));
}

void present(dx_command_list* cl,
	ref<dx_texture> ldrInput,
	ref<dx_texture> output,
	sharpen_settings sharpenSettings)
{
	DX_PROFILE_BLOCK(cl, "Present");

	uint32 xOffset = (output->width - ldrInput->width) / 2;
	uint32 yOffset = (output->height - ldrInput->height) / 2;

	cl->setPipelineState(*presentPipeline.pipeline);
	cl->setComputeRootSignature(*presentPipeline.rootSignature);

	cl->setDescriptorHeapUAV(PRESENT_RS_TEXTURES, 0, output);
	cl->setDescriptorHeapSRV(PRESENT_RS_TEXTURES, 1, ldrInput);
	cl->setCompute32BitConstants(PRESENT_RS_CB, present_cb{ present_sdr, 0.f, sharpenSettings.strength, (xOffset << 16) | yOffset });

	cl->dispatch(bucketize(output->width, POST_PROCESSING_BLOCK_SIZE), bucketize(output->height, POST_PROCESSING_BLOCK_SIZE));
}

void light_culling::allocateIfNecessary(uint32 renderWidth, uint32 renderHeight)
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
