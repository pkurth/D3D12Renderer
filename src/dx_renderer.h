#pragma once

#include "dx_render_target.h"
#include "dx_command_list.h"
#include "math.h"
#include "camera.h"
#include "render_pass.h"
#include "light_source.h"
#include "pbr.h"
#include "input.h"

#include "light_source.hlsli"
#include "camera.hlsli"
#include "present_rs.hlsli"
#include "volumetrics_rs.hlsli"


/*
	The layout of our shadow map is:
	_________________________________
	|		|		|		|		|
	| Sun 0 | Sun 1 | Sun 2 | Sun 3 |
	|_______|_______|_______|_______|
	|								|
	|								|
	|								|
	|								|
	|								|
	|								|
	|_______________________________|

	Sun 0-3 are the sun cascades.
	The space at the bottom is free for the application to use for shadow casters other than the sun.
*/


#define SHADOW_MAP_WIDTH (SUN_SHADOW_DIMENSIONS * MAX_NUM_SUN_SHADOW_CASCADES)
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


struct renderer_settings
{
	tonemap_cb tonemap = defaultTonemapParameters();
	float environmentIntensity = 1.f;
	float skyIntensity = 1.f;

	uint32 raytracingDownsampleFactor = 1;
	uint32 numRaytracingBounces = 1;
	uint32 blurRaytracingResultIterations = 1;
	float raytracingMaxDistance = 100.f;
	float raytracingFadeoutDistance = 80.f;

	aspect_ratio_mode aspectRatioMode = aspect_ratio_free;
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

	void submitRenderPass(opaque_render_pass* renderPass)
	{
		assert(!opaqueRenderPass);
		opaqueRenderPass = renderPass;
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

	
	renderer_settings settings;

	uint32 renderWidth;
	uint32 renderHeight;
	ref<dx_texture> frameResult;

	uint16 hoveredObjectID = 0xFFFF;

	static struct pbr_raytracing_pipeline* getRaytracingPipeline();

	static ref<dx_texture> getWhiteTexture();
	static ref<dx_texture> getBlackTexture();
	static ref<dx_texture> getShadowMap();

	static dx_cpu_descriptor_handle nullTextureSRV;
	static dx_cpu_descriptor_handle nullBufferSRV;


	static DXGI_FORMAT screenFormat;

	static constexpr DXGI_FORMAT hdrFormat[] = { DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16_FLOAT }; // HDR color, world normals.
	static constexpr DXGI_FORMAT hdrDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	static constexpr DXGI_FORMAT depthOnlyFormat[] = { DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_R16_UINT }; // Screen velocities, object ID.
	static constexpr DXGI_FORMAT shadowDepthFormat = DXGI_FORMAT_D16_UNORM; // TODO: Evaluate whether this is enough.
	static constexpr DXGI_FORMAT volumetricsFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	static constexpr DXGI_FORMAT raytracedReflectionsFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

private:

	struct light_culling_buffers
	{
		ref<dx_buffer> tiledFrusta;

		ref<dx_buffer> lightIndexCounter;
		ref<dx_buffer> pointLightIndexList;
		ref<dx_buffer> spotLightIndexList;

		ref<dx_texture> lightGrid;

		uint32 numTilesX;
		uint32 numTilesY;
	};


	opaque_render_pass* opaqueRenderPass;
	sun_shadow_render_pass* sunShadowRenderPass;
	spot_shadow_render_pass* spotLightShadowRenderPasses[MAX_NUM_SPOT_LIGHT_SHADOW_PASSES];
	point_shadow_render_pass* pointLightShadowRenderPasses[MAX_NUM_POINT_LIGHT_SHADOW_PASSES];
	uint32 numSpotLightShadowRenderPasses;
	uint32 numPointLightShadowRenderPasses;


	uint32 windowWidth;
	uint32 windowHeight;
	D3D12_VIEWPORT windowViewport;

	dx_render_target windowRenderTarget;

	dx_render_target hdrRenderTarget;
	dx_render_target depthOnlyRenderTarget;
	ref<dx_texture> hdrColorTexture;
	ref<dx_texture> worldNormalsTexture;
	ref<dx_texture> screenVelocitiesTexture;
	ref<dx_texture> objectIDsTexture;
	ref<dx_texture> depthStencilBuffer;

	ref<dx_texture> volumetricsTexture;

	ref<dx_texture> raytracingTexture;
	ref<dx_texture> raytracingTextureTmpForBlur;

	ref<dx_buffer> hoveredObjectIDReadbackBuffer[NUM_BUFFERED_FRAMES];

	ref<dx_buffer> spotLightShadowInfoBuffer[NUM_BUFFERED_FRAMES];
	ref<dx_buffer> pointLightShadowInfoBuffer[NUM_BUFFERED_FRAMES];


	ref<pbr_environment> environment;
	ref<dx_buffer> pointLights;
	ref<dx_buffer> spotLights;
	uint32 numPointLights;
	uint32 numSpotLights;

	camera_cb camera;
	directional_light_cb sun;



	light_culling_buffers lightCullingBuffers;

	renderer_settings oldSettings;

	void recalculateViewport(bool resizeTextures);
	void allocateLightCullingBuffers();
};

