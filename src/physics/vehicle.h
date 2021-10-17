#pragma once

#include "physics.h"

struct vehicle
{
	vehicle() {}

	void initialize(game_scene& scene);
	static vehicle create(game_scene& scene);

	union
	{
		struct
		{
			scene_entity motor;
			scene_entity gears[4];
		};
		scene_entity parts[5];
	};
};
