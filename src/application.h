#pragma once

#include "input.h"
#include "camera.h"
#include "mesh.h"
#include "math.h"
#include "dx_renderer.h"
#include "light_source.hlsl" // TODO: For now. The game should only know about the C++ version of lights eventually.
#include "light_source.h"
#include "raytracing.h"
#include "raytracing_batch.h"


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
	void updateCamera(const user_input& input, float dt);

	std::vector<composite_mesh> meshes;
	std::vector<scene_object> gameObjects;
	std::vector<raytracing_blas> blas;

	specular_reflections_raytracing_batch reflectionsRaytracingBatch;

	ref<pbr_environment> environment;


	static const uint32 numPointLights = 0;
	static const uint32 numSpotLights = 0;

	point_light_cb* pointLights;
	spot_light_cb* spotLights;
	vec3* lightVelocities;

	directional_light sun;


	dx_renderer* renderer;
	render_camera camera;
};
