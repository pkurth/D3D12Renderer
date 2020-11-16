#pragma once

#include "math.h"
#include "mesh.h"
#include "dx_renderer.h"
#include "input.h"
#include "camera.h"
#include "light_source.hlsl" // TODO: For now. The game should only know about the C++ version of lights eventually.
#include "light_source.h"


struct game_object
{
	trs transform;
	uint32 meshIndex;
	uint32 materialIndex;
};

struct game_scene
{
	void initialize(dx_renderer* renderers, uint32 numRenderers);
	void update(const user_input& input, float dt);

	void serializeToFile(const char* filename);
	void unserializeFromFile(const char* filename);


	void setEnvironment(const char* filename);


	dx_renderer* renderers;
	uint32 numRenderers;

	std::vector<dx_texture> textures;
	std::vector<composite_mesh> meshes;
	std::vector<pbr_material> materials; // TODO: Eventually store indices to textures here, instead of pointers.
	std::vector<game_object> gameObjects;
	pbr_environment environment;

	render_camera* cameras;

	const uint32 numPointLights = 0;
	point_light_cb* pointLights;

	const uint32 numSpotLights = 0;
	spot_light_cb* spotLights;

	directional_light sun;

	vec3* lightVelocities;
};
