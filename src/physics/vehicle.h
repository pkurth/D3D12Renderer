#pragma once

#include "physics.h"

struct vehicle
{
	vehicle() {}

	void initialize(game_scene& scene, vec3 initialMotorPosition, float initialRotation = 0.f);
	static vehicle create(game_scene& scene, vec3 initialMotorPosition, float initialRotation = 0.f);

	union
	{
		struct
		{
			scene_entity motor;
			scene_entity motorGear;
			scene_entity driveAxis;
			scene_entity frontAxis;
			scene_entity steeringWheel;
			scene_entity steeringAxis;

			scene_entity leftWheelSuspension;
			scene_entity rightWheelSuspension;

			scene_entity leftFrontWheel;
			scene_entity rightFrontWheel;

			scene_entity leftWheelArm;
			scene_entity rightWheelArm;

			scene_entity differentialSunGear;
			scene_entity differentialSpiderGear;

			scene_entity leftRearWheel;
			scene_entity rightRearWheel;
		};
		scene_entity parts[16];
	};
};
