#pragma once

#include "physics.h"

struct humanoid_ragdoll
{
	void initialize(struct scene& appScene);
	bool edit();

	constraint_handle neckConstraint;
	constraint_handle leftShoulderConstraint;
	constraint_handle leftElbowConstraint;
	constraint_handle rightShoulderConstraint;
	constraint_handle rightElbowConstraint;
	constraint_handle leftHipConstraint;
	constraint_handle leftKneeConstraint;
	constraint_handle rightHipConstraint;
	constraint_handle rightKneeConstraint;
};
