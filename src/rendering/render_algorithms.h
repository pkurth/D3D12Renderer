#pragma once

#include "dx/dx_command_list.h"
#include "dx/dx_texture.h"
#include "dx/dx_render_target.h"
#include "shadow_map_cache.h"
#include "render_pass.h"
#include "material.h"
#include "light_source.h"

#define MAX_NUM_TOTAL_DECALS 256   // Total per frame (not per tile). MUST MATCH light_culling_rs.hlsli

enum gaussian_blur_kernel_size
{
	gaussian_blur_5x5,
	gaussian_blur_9x9,
	gaussian_blur_13x13,
};

struct ssr_settings
{
	uint32 numSteps = 400;
	float maxDistance = 1000.f;
	float strideCutoff = 100.f;
	float minStride = 5.f;
	float maxStride = 30.f;
};

struct taa_settings
{
	float cameraJitterStrength = 1.f;
};

struct bloom_settings
{
	float threshold = 100.f;
	float strength = 0.1f;
};

struct sharpen_settings
{
	float strength = 0.5f;
};

struct tonemap_settings
{
	float A = 0.22f; // Shoulder strength.
	float B = 0.3f; // Linear strength.
	float C = 0.1f; // Linear angle.
	float D = 0.2f; // Toe strength.
	float E = 0.01f; // Toe Numerator.
	float F = 0.3f; // Toe denominator.
	// Note E/F = Toe angle.
	float linearWhite = 11.2f;

	float exposure = 0.2f;

	float tonemap(float color)
	{
		color *= exp2(exposure);
		return evaluate(color) / evaluate(linearWhite);
	}

private:
	float evaluate(float x)
	{
		return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - (E / F);
	}
};

struct light_culling
{
	ref<dx_buffer> tiledWorldSpaceFrustaBuffer;

	ref<dx_buffer> tiledCullingIndexCounter;
	ref<dx_buffer> tiledObjectsIndexList;

	// DXGI_FORMAT_R32G32B32A32_UINT. 
	// The R&B channel contains the offset into tiledObjectsIndexList. 
	// The G&A channel contains the number of point lights and spot lights in 10 bit each, so there is space for more info.
	// Opaque is in R,G.
	// Transparent is in B,A.
	// For more info, see light_culling_cs.hlsl.
	ref<dx_texture> tiledCullingGrid;

	uint32 numCullingTilesX;
	uint32 numCullingTilesY;

	void allocateIfNecessary(uint32 renderWidth, uint32 renderHeight);
};


void loadCommonShaders();


// ----------------------------------------
// GRAPHICS SHADERS. Since the pipelines depend on the render-target format, we pass the pipelines in.
// ----------------------------------------


void depthPrePass(dx_command_list* cl,
	const dx_render_target& depthOnlyRenderTarget,
	const dx_pipeline& rigidPipeline,
	const dx_pipeline& animatedPipeline,
	const opaque_render_pass* opaqueRenderPass,
	const mat4& viewProj, const mat4& prevFrameViewProj,
	vec2 jitter, vec2 prevFrameJitter);

void texturedSky(dx_command_list* cl,
	const dx_render_target& skyRenderTarget,
	const dx_pipeline& skyPipeline,
	const mat4& proj, const mat4& view,
	ref<dx_texture> sky,
	float skyIntensity);

void proceduralSky(dx_command_list* cl,
	const dx_render_target& skyRenderTarget,
	const dx_pipeline& skyPipeline,
	const mat4& proj, const mat4& view,
	float skyIntensity);

void sphericalHarmonicsSky(dx_command_list* cl,
	const dx_render_target& skyRenderTarget,
	const dx_pipeline& skyPipeline,
	const mat4& proj, const mat4& view,
	const ref<dx_buffer>& sh, uint32 shIndex,
	float skyIntensity);

void preethamSky(dx_command_list* cl,
	const dx_render_target& skyRenderTarget,
	const dx_pipeline& skyPipeline,
	const mat4& proj, const mat4& view,
	vec3 sunDirection, float skyIntensity);

void shadowPasses(dx_command_list* cl,
	const dx_pipeline& shadowPipeline,
	const dx_pipeline& pointLightShadowPipeline,
	const sun_shadow_render_pass** sunShadowRenderPasses, uint32 numSunLightShadowRenderPasses,
	const spot_shadow_render_pass** spotLightShadowRenderPasses, uint32 numSpotLightShadowRenderPasses,
	const point_shadow_render_pass** pointLightShadowRenderPasses, uint32 numPointLightShadowRenderPasses);

void opaqueLightPass(dx_command_list* cl,
	const dx_render_target& renderTarget,
	const opaque_render_pass* opaqueRenderPass,
	const common_material_info& materialInfo,
	const mat4& viewProj);

void transparentLightPass(dx_command_list* cl,
	const dx_render_target& renderTarget,
	const transparent_render_pass* transparentRenderPass,
	const common_material_info& materialInfo,
	const dx_command_signature& particleCommandSignature,
	const mat4& viewProj);

void overlays(dx_command_list* cl,
	const dx_render_target& ldrRenderTarget,
	overlay_render_pass* overlayRenderPass,
	const common_material_info& materialInfo,
	const mat4& viewProj);

void outlines(dx_command_list* cl,
	const dx_render_target& ldrRenderTarget,
	ref<dx_texture> depthStencilBuffer,			// DEPTH_WRITE. Must be same as DSV bound to render-target.
	const dx_pipeline& outlineMarkerPipeline,
	const dx_pipeline& outlineDrawerPipeline,
	const opaque_render_pass* opaqueRenderPass,
	const mat4& viewProj,
	uint32 stencilBit);

void copyShadowMapParts(dx_command_list* cl,
	ref<dx_texture> from,						// PIXEL_SHADER_RESOURCE
	ref<dx_texture> to,							// DEPTH_WRITE
	shadow_map_viewport* copies, uint32 numCopies);




// ----------------------------------------
// COMPUTE SHADERS. These don't care about the texture formats.
// ----------------------------------------


void lightAndDecalCulling(dx_command_list* cl,
	ref<dx_texture> depthStencilBuffer,			// NON_PIXEL_SHADER_RESOURCE
	ref<dx_buffer> pointLights,
	ref<dx_buffer> spotLights,
	ref<dx_buffer> decals,
	light_culling culling,
	uint32 numPointLights, uint32 numSpotLights, uint32 numDecals,
	dx_dynamic_constant_buffer cameraCBV);

void linearDepthPyramid(dx_command_list* cl,
	ref<dx_texture> depthStencilBuffer,			// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> linearDepthBuffer,			// UNORDERED_ACCESS
	vec4 projectionParams);

void gaussianBlur(dx_command_list* cl,
	ref<dx_texture> inputOutput,				// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> temp,						// UNORDERED_ACCESS
	uint32 inputMip, uint32 outputMip, gaussian_blur_kernel_size kernel, uint32 numIterations = 1);

void screenSpaceReflections(dx_command_list* cl,
	ref<dx_texture> hdrInput,					// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> prevFrameHDR,				// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> depthStencilBuffer,			// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> linearDepthBuffer,			// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> worldNormalsTexture,		// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> reflectanceTexture,			// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> screenVelocitiesTexture,	// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> raycastTexture,				// UNORDERED_ACCESS
	ref<dx_texture> resolveTexture,				// UNORDERED_ACCESS. After call NON_PIXEL_SHADER_RESOURCE. Also output of this algorithm.
	ref<dx_texture> ssrTemporalHistory,			// NON_PIXEL_SHADER_RESOURCE. After call UNORDERED_ACCESS.
	ref<dx_texture> ssrTemporalOutput,			// UNORDERED_ACCESS. After call NON_PIXEL_SHADER_RESOURCE.
	ssr_settings settings,
	dx_dynamic_constant_buffer cameraCBV);

void specularAmbient(dx_command_list* cl,
	ref<dx_texture> hdrInput,					// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> ssr,						// NON_PIXEL_SHADER_RESOURCE. Can be null.
	ref<dx_texture> worldNormalsTexture,		// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> reflectanceTexture,			// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> environment,				// NON_PIXEL_SHADER_RESOURCE. Can be null.
	ref<dx_texture> output,						// UNORDERED_ACCESS
	dx_dynamic_constant_buffer cameraCBV);

void temporalAntiAliasing(dx_command_list* cl,
	ref<dx_texture> hdrInput,					// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> screenVelocitiesTexture,	// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> depthStencilBuffer,			// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> history,					// NON_PIXEL_SHADER_RESOURCE. After call UNORDERED_ACCESS.
	ref<dx_texture> output,						// UNORDERED_ACCESS. After call NON_PIXEL_SHADER_RESOURCE.
	vec4 jitteredCameraProjectionParams);

void downsample(dx_command_list* cl,
	ref<dx_texture> input,						// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> output,						// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> temp);						// UNORDERED_ACCESS

void bloom(dx_command_list* cl,
	ref<dx_texture> hdrInput,					// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> output,						// Input as UNORDERED_ACCESS. After call NON_PIXEL_SHADER_RESOURCE.
	ref<dx_texture> bloomTexture,				// UNORDERED_ACCESS
	ref<dx_texture> bloomTempTexture,			// UNORDERED_ACCESS
	bloom_settings settings);

void tonemap(dx_command_list* cl,
	ref<dx_texture> hdrInput,					// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> ldrOutput,					// UNORDERED_ACCESS
	const tonemap_settings& settings);

void present(dx_command_list* cl,
	ref<dx_texture> ldrInput,					// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> output,						// UNORDERED_ACCESS
	sharpen_settings sharpenSettings);
