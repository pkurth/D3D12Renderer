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
			scene_entity motorGear;
			scene_entity driveAxis;
			scene_entity frontAxis;
			scene_entity rearAxis;
			scene_entity steeringWheel;
			scene_entity steeringAxis;
		};
		scene_entity parts[7];
	};
};
