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


struct scene_object
{
	trs transform;
	uint32 meshIndex;
};

struct application
{
	void initialize(dx_renderer* renderer);
	void update(const user_input& input, float dt);

	void setEnvironment(const char* filename);

	void serializeToFile(const char* filename);
	bool deserializeFromFile(const char* filename);

private:
	std::vector<composite_mesh> meshes;
	std::vector<scene_object> gameObjects;
	std::vector<raytracing_blas> blas;

	composite_mesh stormtrooperMesh;


	pbr_raytracing_binding_table reflectionsRaytracingPipeline;
	raytracing_tlas raytracingTLAS;

	ref<pbr_environment> environment;


	static const uint32 numPointLights = 0;
	static const uint32 numSpotLights = 0;

	point_light_cb* pointLights;
	spot_light_cb* spotLights;
	vec3* lightVelocities;

	directional_light sun;

	dx_renderer* renderer;

	render_camera camera;
	camera_controller cameraController;

	scene scene;
};
