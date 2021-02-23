#pragma once

#include "input.h"
#include "camera.h"
#include "camera_controller.h"
#include "mesh.h"
#include "math.h"
#include "dx_renderer.h"
#include "light_source.hlsli" // TODO: For now. The game should only know about the C++ version of lights eventually.
#include "light_source.h"
#include "transformation_gizmo.h"
#include "raytracing.h"

#include "path_tracing.h"

#include "scene.h"


struct application
{
	void loadCustomShaders();
	void initialize(dx_renderer* renderer);
	void update(const user_input& input, float dt);

	void setEnvironment(const char* filename);

	void serializeToFile(const char* filename);
	bool deserializeFromFile(const char* filename);

private:
	void setSelectedEntityEulerRotation();
	void setSelectedEntity(scene_entity entity);
	void drawSceneHierarchy();
	void drawSettings(float dt);

	void resetRenderPasses();
	void submitRenderPasses();
	void handleUserInput(const user_input& input, float dt);

	void assignShadowMapViewports();


	raytracing_tlas raytracingTLAS;
	path_tracer pathTracer;

	ref<pbr_environment> environment;


	ref<dx_buffer> pointLightBuffer[NUM_BUFFERED_FRAMES];
	ref<dx_buffer> spotLightBuffer[NUM_BUFFERED_FRAMES];
	ref<dx_buffer> decalBuffer[NUM_BUFFERED_FRAMES];

	std::vector<point_light_cb> pointLights;
	std::vector<spot_light_cb> spotLights;
	std::vector<pbr_decal_cb> decals;

	ref<dx_texture> decalTexture;

	directional_light sun;

	dx_renderer* renderer;

	render_camera camera;
	camera_controller cameraController;

	scene appScene;
	scene_entity selectedEntity;
	vec3 selectedEntityEulerRotation;


	opaque_render_pass opaqueRenderPass;
	transparent_render_pass transparentRenderPass;
	sun_shadow_render_pass sunShadowRenderPass;
	spot_shadow_render_pass spotShadowRenderPasses[2];
	point_shadow_render_pass pointShadowRenderPasses[1];
	overlay_render_pass overlayRenderPass;
};
