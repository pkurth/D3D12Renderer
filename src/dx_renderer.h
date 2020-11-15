#pragma once

#include "dx_render_primitives.h"
#include "math.h"
#include "camera.h"

#include "model_rs.hlsl"
#include "light_source.hlsl"

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
	dx_texture irradiance;
	dx_texture prefiltered;
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


struct dx_renderer
{
	static void initialize(uint32 windowWidth, uint32 windowHeight);
	static pbr_environment createEnvironment(const char* filename);

	static void beginFrame(uint32 windowWidth, uint32 windowHeight);
	static void setCamera(const render_camera& camera);

	static void setEnvironment(const pbr_environment& environment);
	static void setSun(const vec3& direction, const vec3& radiance);
	static void setPointLights(const point_light_cb* lights, uint32 numLights);
	static void setSpotLights(const spot_light_cb* lights, uint32 numLights);

	static void renderObject(const dx_mesh* mesh, submesh_info submesh, const pbr_material* material, const trs& transform);

	static void render(float dt);



	static uint32 renderWidth;
	static uint32 renderHeight;

	static dx_render_target windowRenderTarget;
	static dx_texture frameResult;

private:
	static D3D12_VIEWPORT windowViewport;
	static aspect_ratio_mode aspectRatioMode;

	static dx_dynamic_constant_buffer cameraCBV;
	static dx_dynamic_constant_buffer sunCBV;

	static dx_texture whiteTexture;

	static dx_render_target hdrRenderTarget;
	static dx_texture hdrColorTexture;
	static dx_texture depthBuffer;

	static light_culling_buffers lightCullingBuffers;
	static dx_buffer pointLightBuffer[NUM_BUFFERED_FRAMES];
	static dx_buffer spotLightBuffer[NUM_BUFFERED_FRAMES];

	static uint32 windowWidth;
	static uint32 windowHeight;

	// Per frame stuff.
	static const render_camera* camera;
	static const pbr_environment* environment;
	static const point_light_cb* pointLights;
	static const spot_light_cb* spotLights;
	static uint32 numPointLights;
	static uint32 numSpotLights;

	struct draw_call
	{
		const mat4 transform;
		const dx_mesh* mesh;
		const pbr_material* material;
		submesh_info submesh;
	};

	static std::vector<draw_call> drawCalls;



	static void recalculateViewport(bool resizeTextures);
	static void allocateLightCullingBuffers();
};

