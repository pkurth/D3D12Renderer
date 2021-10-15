#pragma once

#include "locomotion_environment.h"

struct locomotion_inference_environment : locomotion_environment
{
	void initialize(game_scene& scene, const humanoid_ragdoll& ragdoll);
	void update(game_scene& scene);
};
