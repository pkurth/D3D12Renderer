#pragma once

#include "dx_command_list.h"
#include "math.h"
#include "camera.h"
#include "render_pass.h"
#include "light_source.h"
#include "pbr.h"
#include "input.h"
#include "raytracer.h"

#include "light_source.hlsli"
#include "camera.hlsli"
#include "volumetrics_rs.hlsli"
#include "post_processing_rs.hlsli"


#define SHADOW_MAP_WIDTH 8192
#define SHADOW_MAP_HEIGHT 8192

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
	tonemap_cb tonemap = defaultTonemapParameters();
	float environmentIntensity = 1.f;
	float skyIntensity = 1.f;

	aspect_ratio_mode aspectRatioMode = aspect_ratio_free;

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
	static void initializeCommon(DXGI_FORMAT screenFormat);
	
	void initialize(uint32 windowWidth, uint32 windowHeight, bool renderObjectIDs);

	static void beginFrameCommon();
	static void endFrameCommon();

	void beginFrame(uint32 windowWidth, uint32 windowHeight);	
	void endFrame(const user_input& input);
	void blitResultToScreen(dx_command_list* cl, dx_rtv_descriptor_handle rtv);


	// Set these with your application.
	void setCamera(const render_camera& camera);
	void setEnvironment(const ref<pbr_environment>& environment);
	void setSun(const directional_light& light);

	void setPointLights(const ref<dx_buffer>& lights, uint32 numLights);
	void setSpotLights(const ref<dx_buffer>& lights, uint32 numLights);
	void setDecals(const ref<dx_buffer>& decals, uint32 numDecals);

	void submitRenderPass(opaque_render_pass* renderPass)
	{
		assert(!opaqueRenderPass);
		opaqueRenderPass = renderPass;
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

	uint16 hoveredObjectID = 0xFFFF;

	static ref<dx_texture> getWhiteTexture();
	static ref<dx_texture> getBlackTexture();
	static ref<dx_texture> getShadowMap();

	static dx_cpu_descriptor_handle nullTextureSRV;
	static dx_cpu_descriptor_handle nullBufferSRV;


	static DXGI_FORMAT screenFormat;

	static constexpr DXGI_FORMAT hdrFormat = DXGI_FORMAT_R32G32B32A32_FLOAT; // TODO: This could be way less. However, for path tracing accumulation over time this was necessary.
	static constexpr DXGI_FORMAT worldNormalsFormat = DXGI_FORMAT_R16G16_FLOAT;
	static constexpr DXGI_FORMAT hdrDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	static constexpr DXGI_FORMAT screenVelocitiesFormat = DXGI_FORMAT_R16G16_FLOAT;
	static constexpr DXGI_FORMAT objectIDsFormat = DXGI_FORMAT_R16_UINT; // Do not change this. 16 bit is hardcoded in other places.
	static constexpr DXGI_FORMAT shadowDepthFormat = DXGI_FORMAT_D16_UNORM;
	static constexpr DXGI_FORMAT volumetricsFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	static constexpr DXGI_FORMAT hdrPostProcessFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	static constexpr DXGI_FORMAT ldrPostProcessFormat = DXGI_FORMAT_R11G11B10_FLOAT; // Not really LDR. But I don't like the idea of converting to 8 bit and then to sRGB in separate passes.

	static constexpr DXGI_FORMAT lightPassFormats[] = { hdrFormat, worldNormalsFormat };
	static constexpr DXGI_FORMAT skyPassFormats[] = { hdrFormat, screenVelocitiesFormat, objectIDsFormat };

private:

	dx_raytracer* raytracer;
	raytracing_tlas* tlas;

	opaque_render_pass* opaqueRenderPass;
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
	ref<dx_texture> depthStencilBuffer;

	ref<dx_texture> hdrPostProcessingTexture;
	ref<dx_texture> ldrPostProcessingTexture;
	ref<dx_texture> taaTextures[2]; // These get flip-flopped from frame to frame.
	uint32 taaHistoryIndex = 0;

	ref<dx_texture> bloomTexture;
	ref<dx_texture> bloomTempTexture;

	ref<dx_buffer> hoveredObjectIDReadbackBuffer;

	ref<dx_buffer> spotLightShadowInfoBuffer[NUM_BUFFERED_FRAMES];
	ref<dx_buffer> pointLightShadowInfoBuffer[NUM_BUFFERED_FRAMES];

	ref<pbr_environment> environment;
	ref<dx_buffer> pointLights;
	ref<dx_buffer> spotLights;
	ref<dx_buffer> decals;
	uint32 numPointLights;
	uint32 numSpotLights;
	uint32 numDecals;

	camera_cb camera;
	directional_light_cb sun;



	// Tiled light and decal culling.
	ref<dx_buffer> tiledWorldSpaceFrustaBuffer;

	ref<dx_buffer> tiledCullingIndexCounter;
	ref<dx_buffer> tiledObjectsIndexList;

	// DXGI_FORMAT_R32G32_UINT. The R channel contains the offset into tiledObjectsIndexList. The G channel contains the number of point lights and spot lights in 10 bit each, so there is space for more info.
	// For more info, see light_culling_cs.hlsl.
	ref<dx_texture> tiledCullingGrid;

	uint32 numCullingTilesX;
	uint32 numCullingTilesY;




	renderer_settings oldSettings;
	renderer_mode oldMode = renderer_mode_rasterized;

	void recalculateViewport(bool resizeTextures);
	void allocateLightCullingBuffers();

	void gaussianBlur(dx_command_list* cl, ref<dx_texture> inputOutput, ref<dx_texture> temp, uint32 inputMip, uint32 outputMip, uint32 numIterations = 1);
	void tonemapAndPresent(dx_command_list* cl, const ref<dx_texture>& hdrResult);
};

