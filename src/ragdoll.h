#pragma once

#include "physics.h"

struct humanoid_ragdoll
{
	void initialize(struct scene& appScene);
	bool edit();

	cone_twist_constraint_handle neckConstraint;
	cone_twist_constraint_handle leftShoulderConstraint;
	hinge_joint_constraint_handle leftElbowConstraint;
	cone_twist_constraint_handle rightShoulderConstraint;
	hinge_joint_constraint_handle rightElbowConstraint;
	cone_twist_constraint_handle leftHipConstraint;
	hinge_joint_constraint_handle leftKneeConstraint;
	cone_twist_constraint_handle rightHipConstraint;
	hinge_joint_constraint_handle rightKneeConstraint;
};
