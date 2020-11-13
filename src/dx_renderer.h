#pragma once

#include "dx_render_primitives.h"
#include "math.h"

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

struct pbr_environment
{
	dx_texture sky;
	dx_texture irradiance;
	dx_texture prefiltered;
};

struct light_culling_buffers
{
	dx_buffer tiledFrusta;

	dx_buffer opaqueLightIndexCounter;
	dx_buffer opaqueLightIndexList;

	dx_texture opaqueLightGrid;

	uint32 numTilesX;
	uint32 numTilesY;
};


struct dx_renderer
{
	static void initialize(uint32 windowWidth, uint32 windowHeight);

	static void beginFrame(uint32 windowWidth, uint32 windowHeight, float dt);
	static void recalculateViewport(bool resizeTextures);
	static void fillCameraConstantBuffer(struct camera_cb& cb);
	static void allocateLightCullingBuffers();
	static void dummyRender(float dt);


	static dx_dynamic_constant_buffer cameraCBV;

	static dx_texture whiteTexture;

	static dx_render_target hdrRenderTarget;
	static dx_texture hdrColorTexture;
	static dx_texture depthBuffer;

	static light_culling_buffers lightCullingBuffers;
	static dx_buffer pointLightBoundingVolumes[NUM_BUFFERED_FRAMES];
	static dx_buffer spotLightBoundingVolumes[NUM_BUFFERED_FRAMES];

	static uint32 renderWidth;
	static uint32 renderHeight;
	static uint32 windowWidth;
	static uint32 windowHeight;

	static dx_render_target windowRenderTarget;
	static dx_texture frameResult;

	static D3D12_VIEWPORT windowViewport;

	static aspect_ratio_mode aspectRatioMode;
};

