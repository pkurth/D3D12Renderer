#pragma once

#include "dx/dx_command_list.h"
#include "dx/dx_texture.h"
#include "dx/dx_render_target.h"
#include "shadow_map_cache.h"
#include "render_pass.h"
#include "material.h"
#include "light_source.h"
#include "core/reflect.h"

#define MAX_NUM_TOTAL_DECALS 256   // Total per frame (not per tile). MUST MATCH light_culling_rs.hlsli

enum gaussian_blur_kernel_size
{
	gaussian_blur_5x5,
	gaussian_blur_9x9,
	gaussian_blur_13x13,

	gaussian_blur_kernel_size_count,
};

struct ssr_settings
{
	uint32 numSteps = 400;
	float maxDistance = 1000.f;
	float strideCutoff = 100.f;
	float minStride = 5.f;
	float maxStride = 30.f;
};
REFLECT_STRUCT(ssr_settings,
	(numSteps, "Num steps"),
	(maxDistance, "Max distance"),
	(strideCutoff, "Stride cutoff"),
	(minStride, "Min stride"),
	(maxStride, "Max stride")
);

struct taa_settings
{
	float cameraJitterStrength = 1.f;
};
REFLECT_STRUCT(taa_settings,
	(cameraJitterStrength, "Camera jitter strength")
);

struct bloom_settings
{
	float threshold = 100.f;
	float strength = 0.05f;
};
REFLECT_STRUCT(bloom_settings,
	(threshold, "Threshold"),
	(strength, "Strength")
);

struct hbao_settings
{
	float radius = 0.5f; // In meters.
	uint32 numRays = 4;
	uint32 maxNumStepsPerRay = 10;
	float strength = 1.f;
};
REFLECT_STRUCT(hbao_settings,
	(radius, "Threshold"),
	(numRays, "Num rays"),
	(maxNumStepsPerRay, "Max num steps per ray"),
	(strength, "Strength")
);

struct sharpen_settings
{
	float strength = 0.5f;
};
REFLECT_STRUCT(sharpen_settings,
	(strength, "Strength")
);

struct sss_settings
{
	uint32 numSteps = 16;
    float rayDistance = 0.5f; // In meters.
    float thickness = 0.05f; // In meters.
	float maxDistanceFromCamera = 15.f; // In meters.
	float distanceFadeoutRange = 2.f; // In meters.
	float borderFadeout = 0.1f; // In UV-space.
};
REFLECT_STRUCT(sss_settings,
	(numSteps, "Num steps"),
	(rayDistance, "Ray distance"),
	(thickness, "Thickness"),
	(maxDistanceFromCamera, "Max distance from camera"),
	(distanceFadeoutRange, "Distance fadeout range"),
	(borderFadeout, "Border fadeout")
);

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

	float tonemap(float color) const
	{
		color *= exp2(exposure);
		return evaluate(color) / evaluate(linearWhite);
	}

	float evaluate(float x) const
	{
		return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - (E / F);
	}
};
REFLECT_STRUCT(tonemap_settings,
	(A),
	(B),
	(C),
	(D),
	(E),
	(F),
	(linearWhite, "Linear white"),
	(exposure, "Exposure")
);

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



enum stencil_flags
{
	stencil_flag_selected_object = (1 << 0),
};



void depthPrePass(dx_command_list* cl,
	const dx_render_target& depthOnlyRenderTarget,
	const opaque_render_pass* opaqueRenderPass,
	const mat4& viewProj, const mat4& prevFrameViewProj,
	vec2 jitter, vec2 prevFrameJitter);

void texturedSky(dx_command_list* cl,
	const dx_render_target& skyRenderTarget,
	const mat4& proj, const mat4& view, const mat4& prevFrameView,
	ref<dx_texture> sky,
	float skyIntensity, 
	vec2 jitter, vec2 prevFrameJitter);

void proceduralSky(dx_command_list* cl,
	const dx_render_target& skyRenderTarget,
	const mat4& proj, const mat4& view, const mat4& prevFrameView,
	float skyIntensity, 
	vec2 jitter, vec2 prevFrameJitter);

void stylisticSky(dx_command_list* cl,
	const dx_render_target& skyRenderTarget,
	const mat4& proj, const mat4& view, const mat4& prevFrameView,
	vec3 sunDirection, float skyIntensity,
	vec2 jitter, vec2 prevFrameJitter);

void sphericalHarmonicsSky(dx_command_list* cl,
	const dx_render_target& skyRenderTarget,
	const mat4& proj, const mat4& view, const mat4& prevFrameView,
	const ref<dx_buffer>& sh, uint32 shIndex,
	float skyIntensity,
	vec2 jitter, vec2 prevFrameJitter);

void preethamSky(dx_command_list* cl,
	const dx_render_target& skyRenderTarget,
	const mat4& proj, const mat4& view, const mat4& prevFrameView,
	vec3 sunDirection, float skyIntensity,
	vec2 jitter, vec2 prevFrameJitter);

void shadowPasses(dx_command_list* cl,
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
	const mat4& viewProj);

void ldrPass(dx_command_list* cl,
	const dx_render_target& ldrRenderTarget,
	ref<dx_texture> depthStencilBuffer,			// DEPTH_WRITE. Must be same as DSV bound to render-target.
	const ldr_render_pass* ldrRenderPass,
	const common_material_info& materialInfo,
	const mat4& viewProj);

void copyShadowMapParts(dx_command_list* cl,
	ref<dx_texture> from,						// PIXEL_SHADER_RESOURCE
	ref<dx_texture> to,							// DEPTH_WRITE
	shadow_map_viewport* copies, uint32 numCopies);





void lightAndDecalCulling(dx_command_list* cl,
	ref<dx_texture> depthStencilBuffer,				// NON_PIXEL_SHADER_RESOURCE
	ref<dx_buffer> pointLights,
	ref<dx_buffer> spotLights,
	ref<dx_buffer> decals,
	light_culling culling,
	uint32 numPointLights, uint32 numSpotLights, uint32 numDecals,
	dx_dynamic_constant_buffer cameraCBV);

void linearDepthPyramid(dx_command_list* cl,
	ref<dx_texture> depthStencilBuffer,				// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> linearDepthBuffer,				// UNORDERED_ACCESS
	vec4 projectionParams);

void gaussianBlur(dx_command_list* cl,
	ref<dx_texture> inputOutput,					// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> temp,							// UNORDERED_ACCESS
	uint32 inputMip, uint32 outputMip, gaussian_blur_kernel_size kernel, uint32 numIterations = 1);

void dilate(dx_command_list* cl,
	ref<dx_texture> inputOutput,					// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> temp,							// UNORDERED_ACCESS
	uint32 radius, uint32 numIterations = 1);

void erode(dx_command_list* cl,
	ref<dx_texture> inputOutput,					// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> temp,							// UNORDERED_ACCESS
	uint32 radius, uint32 numIterations = 1);

void depthSobel(dx_command_list* cl,
	ref<dx_texture> input,							// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> output,							// UNORDERED_ACCESS
	vec4 projectionParams, float threshold);

void screenSpaceReflections(dx_command_list* cl,
	ref<dx_texture> prevFrameHDR,					// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> depthStencilBuffer,				// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> linearDepthBuffer,				// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> worldNormalsRoughnessTexture,	// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> screenVelocitiesTexture,		// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> raycastTexture,					// UNORDERED_ACCESS
	ref<dx_texture> resolveTexture,					// UNORDERED_ACCESS. After call D3D12_RESOURCE_STATE_GENERIC_READ. Also output of this algorithm.
	ref<dx_texture> ssrTemporalHistory,				// NON_PIXEL_SHADER_RESOURCE. After call UNORDERED_ACCESS.
	ref<dx_texture> ssrTemporalOutput,				// UNORDERED_ACCESS. After call NON_PIXEL_SHADER_RESOURCE.
	ssr_settings settings,
	dx_dynamic_constant_buffer cameraCBV);

void specularAmbient(dx_command_list* cl,
	ref<dx_texture> hdrInput,						// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> ssr,							// NON_PIXEL_SHADER_RESOURCE. Can be null.
	ref<dx_texture> worldNormalsTexture,			// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> reflectanceTexture,				// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> environment,					// NON_PIXEL_SHADER_RESOURCE. Can be null.
	ref<dx_texture> ao,								// NON_PIXEL_SHADER_RESOURCE. Can be null.
	ref<dx_texture> output,							// UNORDERED_ACCESS
	dx_dynamic_constant_buffer cameraCBV);

void temporalAntiAliasing(dx_command_list* cl,
	ref<dx_texture> hdrInput,						// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> screenVelocitiesTexture,		// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> depthStencilBuffer,				// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> history,						// NON_PIXEL_SHADER_RESOURCE. After call UNORDERED_ACCESS.
	ref<dx_texture> output,							// UNORDERED_ACCESS. After call NON_PIXEL_SHADER_RESOURCE.
	vec4 jitteredCameraProjectionParams);

void downsample(dx_command_list* cl,
	ref<dx_texture> input,							// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> output,							// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> temp);							// UNORDERED_ACCESS

void bloom(dx_command_list* cl,
	ref<dx_texture> hdrInput,						// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> output,							// Input as UNORDERED_ACCESS. After call NON_PIXEL_SHADER_RESOURCE.
	ref<dx_texture> bloomTexture,					// UNORDERED_ACCESS
	ref<dx_texture> bloomTempTexture,				// UNORDERED_ACCESS
	bloom_settings settings);

void ambientOcclusion(dx_command_list* cl,
	ref<dx_texture> linearDepth,					// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> screenVelocitiesTexture,		// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> aoCalculationTexture,			// UNORDERED_ACCESS
	ref<dx_texture> aoBlurTempTexture,				// UNORDERED_ACCESS
	ref<dx_texture> history,						// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> output,							// UNORDERED_ACCESS
	hbao_settings settings,
	dx_dynamic_constant_buffer cameraCBV);

void screenSpaceShadows(dx_command_list* cl,
	ref<dx_texture> linearDepth,					// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> screenVelocitiesTexture,		// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> sssCalculationTexture,			// UNORDERED_ACCESS
	ref<dx_texture> sssBlurTempTexture,				// UNORDERED_ACCESS
	ref<dx_texture> history,						// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> output,							// UNORDERED_ACCESS
	vec3 sunDirection,
	sss_settings settings,
	const mat4& view,
	dx_dynamic_constant_buffer cameraCBV);

void tonemap(dx_command_list* cl,
	ref<dx_texture> hdrInput,						// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> ldrOutput,						// UNORDERED_ACCESS
	const tonemap_settings& settings);

void blit(dx_command_list* cl,
	ref<dx_texture> input,							// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> output);						// UNORDERED_ACCESS

void present(dx_command_list* cl,
	ref<dx_texture> ldrInput,						// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> output,							// UNORDERED_ACCESS
	sharpen_settings sharpenSettings);

void visualizeSunShadowCascades(dx_command_list* cl,
	ref<dx_texture> depthBuffer,					// NON_PIXEL_SHADER_RESOURCE
	ref<dx_texture> output,							// UNORDERED_ACCESS
	dx_dynamic_constant_buffer sunCBV,
	const mat4& invViewProj, vec3 cameraPosition, vec3 cameraForward);
