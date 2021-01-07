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

#include "raytracing_tlas.h"
#include "pbr_raytracing_binding_table.h"

#include "scene.h"


struct application
{
	void initialize(dx_renderer* renderer);
	void update(const user_input& input, float dt);

	void setEnvironment(const char* filename);

	void serializeToFile(const char* filename);
	bool deserializeFromFile(const char* filename);

private:
	void setSelectedEntity(scene_entity entity);
	void drawSceneHierarchy();
	void drawSettings(float dt);

	void resetRenderPasses();
	void submitRenderPasses();
	void handleUserInput(const user_input& input, float dt);

	void assignShadowMapViewports();

	std::vector<raytracing_blas> blas;

	pbr_raytracing_binding_table raytracingBindingTable;
	raytracing_tlas raytracingTLAS;

	ref<pbr_environment> environment;


	std::vector<point_light_cb> pointLights;
	std::vector<spot_light_cb> spotLights;

	directional_light sun;

	dx_renderer* renderer;

	render_camera camera;
	camera_controller cameraController;

	scene appScene;
	scene_entity selectedEntity;
	vec3 selectedEntityEulerRotation;


	opaque_render_pass opaqueRenderPass;
	sun_shadow_render_pass sunShadowRenderPass;
	spot_shadow_render_pass spotShadowRenderPasses[2];
	point_shadow_render_pass pointShadowRenderPasses[1];
};
