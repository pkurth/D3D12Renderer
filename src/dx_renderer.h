#pragma once

#include "dx_render_target.h"
#include "dx_command_list.h"
#include "math.h"
#include "camera.h"
#include "render_pass.h"
#include "light_source.h"
#include "gizmo.h"
#include "pbr.h"

#include "light_source.hlsli"
#include "camera.hlsli"
#include "present_rs.hlsli"
#include "volumetrics_rs.hlsli"


#define MAX_NUM_POINT_LIGHTS_PER_FRAME 4096
#define MAX_NUM_SPOT_LIGHTS_PER_FRAME 4096

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
	bool showLightVolumes = false;
};


struct dx_renderer
{
	static void initializeCommon(DXGI_FORMAT screenFormat);
	
	void initialize(uint32 windowWidth, uint32 windowHeight);

	static void beginFrameCommon();
	void beginFrame(uint32 windowWidth, uint32 windowHeight);	
	void endFrame();
	void blitResultToScreen(dx_command_list* cl, dx_rtv_descriptor_handle rtv);


	// Set these with your application.
	void setCamera(const render_camera& camera);
	void setEnvironment(const ref<pbr_environment>& environment);
	void setSun(const directional_light& light);

	void setPointLights(const point_light_cb* lights, uint32 numLights);
	void setSpotLights(const spot_light_cb* lights, uint32 numLights);

	geometry_render_pass* beginGeometryPass() { return &geometryRenderPass; }
	sun_shadow_render_pass* beginSunShadowPass() { return &sunShadowRenderPass; }
	raytraced_reflections_render_pass* beginRaytracedReflectionsPass() { return &raytracedReflectionsRenderPass; }

	
	renderer_settings settings;

	uint32 renderWidth;
	uint32 renderHeight;
	ref<dx_texture> frameResult;

//private:

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


	uint32 windowWidth;
	uint32 windowHeight;
	D3D12_VIEWPORT windowViewport;

	dx_render_target windowRenderTarget;

	dx_render_target hdrRenderTarget;
	dx_render_target hdrRenderTargetWithVelocities;
	ref<dx_texture> hdrColorTexture;
	ref<dx_texture> worldNormalsTexture;
	ref<dx_texture> screenSpaceVelocityTexture;
	ref<dx_texture> depthBuffer;

	ref<dx_texture> volumetricsTexture;

	ref<dx_texture> raytracingTexture;
	ref<dx_texture> raytracingTextureTmpForBlur;



	ref<pbr_environment> environment;
	const point_light_cb* pointLights;
	const spot_light_cb* spotLights;
	uint32 numPointLights;
	uint32 numSpotLights;

	camera_cb camera;
	directional_light_cb sun;





	geometry_render_pass geometryRenderPass;
	sun_shadow_render_pass sunShadowRenderPass;
	raytraced_reflections_render_pass raytracedReflectionsRenderPass;

	light_culling_buffers lightCullingBuffers;

	renderer_settings oldSettings;

	void recalculateViewport(bool resizeTextures);
	void allocateLightCullingBuffers();
};

