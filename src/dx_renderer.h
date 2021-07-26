#pragma once

#include "dx_command_list.h"
#include "math.h"
#include "camera.h"
#include "render_pass.h"
#include "light_source.h"
#include "pbr.h"
#include "input.h"
#include "raytracer.h"
#include "render_algorithms.h"
#include "render_resources.h"

#include "light_source.hlsli"
#include "camera.hlsli"



#define MAX_NUM_SPOT_LIGHT_SHADOW_PASSES 16
#define MAX_NUM_POINT_LIGHT_SHADOW_PASSES 16


enum aspect_ratio_mode
{
	aspect_ratio_free,
	aspect_ratio_fix_16_9,
	aspect_ratio_fix_16_10,

	aspect_ratio_mode_count,
};

static const char* aspectRatioNames[] =
{
	"Free",
	"16:9",
	"16:10",
};

enum renderer_mode
{
	renderer_mode_rasterized,
	renderer_mode_pathtraced,

	renderer_mode_count,
};

static const char* rendererModeNames[] =
{
	"Rasterized",
	"Path-traced",
};

struct renderer_settings
{
	aspect_ratio_mode aspectRatioMode = aspect_ratio_free;

	tonemap_settings tonemap;
	float environmentIntensity = 1.f;
	float skyIntensity = 1.f;

	bool enableSSR = true;
	ssr_settings ssr;

	bool enableTemporalAntialiasing = true;
	float cameraJitterStrength = 1.f;

	bool enableBloom = true;
	float bloomThreshold = 100.f;
	float bloomStrength = 0.1f;

	bool enableSharpen = true;
	float sharpenStrength = 0.5f;
};

struct dx_renderer
{
	static void initializeCommon(DXGI_FORMAT outputFormat);
	
	void initialize(uint32 windowWidth, uint32 windowHeight, bool renderObjectIDs);

	static void beginFrameCommon();
	static void endFrameCommon();

	void beginFrame(uint32 windowWidth, uint32 windowHeight);	
	void endFrame(const user_input& input);


	// Set these with your application.
	void setCamera(const render_camera& camera);
	void setEnvironment(const ref<pbr_environment>& environment);
	void setSun(const directional_light& light);

	void setPointLights(const ref<dx_buffer>& lights, uint32 numLights, const ref<dx_buffer>& shadowInfoBuffer);
	void setSpotLights(const ref<dx_buffer>& lights, uint32 numLights, const ref<dx_buffer>& shadowInfoBuffer);
	void setDecals(const ref<dx_buffer>& decals, uint32 numDecals, const ref<dx_texture>& textureAtlas);

	void submitRenderPass(opaque_render_pass* renderPass)
	{
		assert(!opaqueRenderPass);
		opaqueRenderPass = renderPass;
	}
	void submitRenderPass(transparent_render_pass* renderPass)
	{
		assert(!transparentRenderPass);
		transparentRenderPass = renderPass;
	}
	void submitRenderPass(overlay_render_pass* renderPass)
	{
		assert(!overlayRenderPass);
		overlayRenderPass = renderPass;
	}
	void submitRenderPass(sun_shadow_render_pass* renderPass)
	{
		assert(!sunShadowRenderPass);
		sunShadowRenderPass = renderPass;
	}
	void submitRenderPass(spot_shadow_render_pass* renderPass)
	{
		assert(numSpotLightShadowRenderPasses < MAX_NUM_SPOT_LIGHT_SHADOW_PASSES);
		spotLightShadowRenderPasses[numSpotLightShadowRenderPasses++] = renderPass;
	}
	void submitRenderPass(point_shadow_render_pass* renderPass)
	{
		assert(numPointLightShadowRenderPasses < MAX_NUM_POINT_LIGHT_SHADOW_PASSES);
		pointLightShadowRenderPasses[numPointLightShadowRenderPasses++] = renderPass;
	}
	void setRaytracer(dx_raytracer* raytracer, raytracing_tlas* tlas)
	{
		this->raytracer = raytracer;
		this->tlas = tlas;
	}

	renderer_mode mode = renderer_mode_rasterized;
	
	renderer_settings settings;

	uint32 renderWidth;
	uint32 renderHeight;
	ref<dx_texture> frameResult;

	uint32 hoveredObjectID = -1;

	static DXGI_FORMAT outputFormat;


	static constexpr DXGI_FORMAT hdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	static constexpr DXGI_FORMAT worldNormalsFormat = DXGI_FORMAT_R16G16_FLOAT;
	static constexpr DXGI_FORMAT hdrDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	static constexpr DXGI_FORMAT linearDepthFormat = DXGI_FORMAT_R32_FLOAT;
	static constexpr DXGI_FORMAT screenVelocitiesFormat = DXGI_FORMAT_R16G16_FLOAT;
	static constexpr DXGI_FORMAT objectIDsFormat = DXGI_FORMAT_R32_UINT;
	static constexpr DXGI_FORMAT reflectanceFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // Fresnel (xyz), roughness (w).
	static constexpr DXGI_FORMAT reflectionFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	static constexpr DXGI_FORMAT hdrPostProcessFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	static constexpr DXGI_FORMAT ldrPostProcessFormat = DXGI_FORMAT_R11G11B10_FLOAT; // Not really LDR. But I don't like the idea of converting to 8 bit and then to sRGB in separate passes.

	static constexpr DXGI_FORMAT overlayFormat = ldrPostProcessFormat;
	static constexpr DXGI_FORMAT overlayDepthFormat = hdrDepthStencilFormat;

	static constexpr DXGI_FORMAT opaqueLightPassFormats[] = { hdrFormat, worldNormalsFormat, reflectanceFormat };
	static constexpr DXGI_FORMAT transparentLightPassFormats[] = { hdrPostProcessFormat };
	static constexpr DXGI_FORMAT skyPassFormats[] = { hdrFormat, screenVelocitiesFormat, objectIDsFormat };

private:

	dx_raytracer* raytracer;
	raytracing_tlas* tlas;

	opaque_render_pass* opaqueRenderPass;
	transparent_render_pass* transparentRenderPass;
	sun_shadow_render_pass* sunShadowRenderPass;
	spot_shadow_render_pass* spotLightShadowRenderPasses[MAX_NUM_SPOT_LIGHT_SHADOW_PASSES];
	point_shadow_render_pass* pointLightShadowRenderPasses[MAX_NUM_POINT_LIGHT_SHADOW_PASSES];
	uint32 numSpotLightShadowRenderPasses;
	uint32 numPointLightShadowRenderPasses;
	overlay_render_pass* overlayRenderPass;


	uint32 windowWidth;
	uint32 windowHeight;
	uint32 windowXOffset;
	uint32 windowYOffset;

	ref<dx_texture> hdrColorTexture;
	ref<dx_texture> worldNormalsTexture;
	ref<dx_texture> screenVelocitiesTexture;
	ref<dx_texture> objectIDsTexture;
	ref<dx_texture> reflectanceTexture;
	ref<dx_texture> depthStencilBuffer;
	ref<dx_texture> linearDepthBuffer;
	ref<dx_texture> opaqueDepthBuffer; // The depth-stencil buffer gets copied to this texture after the opaque pass.

	ref<dx_texture> ssrRaycastTexture;
	ref<dx_texture> ssrResolveTexture;
	ref<dx_texture> ssrTemporalTextures[2];
	uint32 ssrHistoryIndex = 0;

	ref<dx_texture> prevFrameHDRColorTexture; // This is downsampled by a factor of 2 and contains up to 8 mip levels.
	ref<dx_texture> prevFrameHDRColorTempTexture;

	ref<dx_texture> hdrPostProcessingTexture;
	ref<dx_texture> ldrPostProcessingTexture;
	ref<dx_texture> taaTextures[2]; // These get flip-flopped from frame to frame.
	uint32 taaHistoryIndex = 0;

	ref<dx_texture> bloomTexture;
	ref<dx_texture> bloomTempTexture;

	ref<dx_buffer> hoveredObjectIDReadbackBuffer;

	ref<pbr_environment> environment;
	ref<dx_buffer> pointLights;
	ref<dx_buffer> spotLights;
	ref<dx_buffer> pointLightShadowInfoBuffer;
	ref<dx_buffer> spotLightShadowInfoBuffer;
	ref<dx_buffer> decals;
	uint32 numPointLights;
	uint32 numSpotLights;
	uint32 numDecals;

	ref<dx_texture> decalTextureAtlas;

	camera_cb jitteredCamera;
	camera_cb unjitteredCamera;
	directional_light_cb sun;


	light_culling culling;


	renderer_settings oldSettings;
	renderer_mode oldMode = renderer_mode_rasterized;

	void recalculateViewport(bool resizeTextures);
};

