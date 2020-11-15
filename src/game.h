#pragma once

#include "math.h"
#include "mesh.h"
#include "dx_renderer.h"
#include "input.h"
#include "camera.h"
#include "light_source.hlsl" // TODO: For now. The game should only know about the C++ version of lights eventually.

struct game_object
{
	trs transform;
	uint32 meshIndex;
	uint32 materialIndex;
};

struct game_scene
{
	std::vector<dx_texture> textures;
	std::vector<composite_mesh> meshes;
	std::vector<pbr_material> materials;
	std::vector<game_object> gameObjects;
	pbr_environment environment;

	render_camera camera;

	const uint32 numPointLights = 128;
	point_light_cb* pointLights;

	const uint32 numSpotLights = 0;
	spot_light_cb* spotLights;

	vec3* lightVelocities;



	void initialize();
	void update(const user_input& input, float dt);
};
