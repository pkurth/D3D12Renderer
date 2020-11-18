#pragma once

#include "dx_render_primitives.h"
#include "math.h"
#include "camera.h"
#include "render_pass.h"
#include "light_source.h"

#include "model_rs.hlsl"
#include "light_source.hlsl"
#include "camera.hlsl"


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

enum gizmo_type
{
	gizmo_type_none,
	gizmo_type_translation,
	gizmo_type_rotation,
	gizmo_type_scale,

	gizmo_type_count,
};

static const char* gizmoTypeNames[] =
{
	"None",
	"Translation",
	"Rotation",
	"Scale",
};

struct pbr_material
{
	dx_texture* albedo;
	dx_texture* normal;
	dx_texture* roughness;
	dx_texture* metallic;

	vec4 albedoTint;
	float roughnessOverride;
	float metallicOverride;
};

struct pbr_environment
{
	dx_texture sky;
	dx_texture environment;
	dx_texture irradiance;
};


struct dx_renderer
{
	static void initializeCommon(DXGI_FORMAT screenFormat);
	static pbr_environment createEnvironment(const char* filename, uint32 skyResolution = 2048, uint32 environmentResolution = 128, uint32 irradianceResolution = 32, bool asyncCompute = false);
	
	void initialize(uint32 windowWidth, uint32 windowHeight);

	static void beginFrameCommon();
	void beginFrame(uint32 windowWidth, uint32 windowHeight);	
	void endFrame(float dt, bool mainWindow = false);
	void blitResultToScreen(dx_command_list* cl, dx_cpu_descriptor_handle rtv);


	void setCamera(const render_camera& camera);
	void setEnvironment(const pbr_environment& environment);
	void setSun(const directional_light& light);

	void setPointLights(const point_light_cb* lights, uint32 numLights);
	void setSpotLights(const spot_light_cb* lights, uint32 numLights);

	geometry_render_pass* beginGeometryPass() { return &geometryRenderPass; }
	sun_shadow_render_pass* beginSunShadowPass() { return &sunShadowRenderPass; }


	uint32 renderWidth;
	uint32 renderHeight;

	dx_render_target windowRenderTarget;
	dx_texture frameResult;

private:
	struct pbr_environment_handles
	{
		dx_cpu_descriptor_handle sky;
		dx_cpu_descriptor_handle irradiance;
		dx_cpu_descriptor_handle environment;
	};

	struct light_culling_buffers
	{
		dx_buffer tiledFrusta;

		dx_buffer lightIndexCounter;
		dx_buffer pointLightIndexList;
		dx_buffer spotLightIndexList;

		dx_texture lightGrid;

		uint32 numTilesX;
		uint32 numTilesY;
	};

	pbr_environment_handles environment;
	const point_light_cb* pointLights;
	const spot_light_cb* spotLights;
	uint32 numPointLights;
	uint32 numSpotLights;

	camera_cb camera;
	directional_light_cb sun;



	D3D12_VIEWPORT windowViewport;
	aspect_ratio_mode aspectRatioMode;

	dx_render_target hdrRenderTarget;
	dx_texture hdrColorTexture;
	dx_texture depthBuffer;

	uint32 windowWidth;
	uint32 windowHeight;

	geometry_render_pass geometryRenderPass;
	sun_shadow_render_pass sunShadowRenderPass;

	light_culling_buffers lightCullingBuffers;

	void recalculateViewport(bool resizeTextures);
	void allocateLightCullingBuffers();
};

