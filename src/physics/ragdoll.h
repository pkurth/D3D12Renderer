#pragma once

#include "physics.h"
#include "scene/scene.h"

struct humanoid_ragdoll
{
	humanoid_ragdoll() {}

	void initialize(struct scene& appScene, vec3 initialHipPosition, float initialRotation = 0.f);
	bool edit();

	union
	{
		struct
		{
			scene_entity torso;
			scene_entity head;
			scene_entity leftUpperArm;
			scene_entity leftLowerArm;
			scene_entity rightUpperArm;
			scene_entity rightLowerArm;
			scene_entity leftUpperLeg;
			scene_entity leftLowerLeg;
			scene_entity leftFoot;
			scene_entity leftToes;
			scene_entity rightUpperLeg;
			scene_entity rightLowerLeg;
			scene_entity rightFoot;
			scene_entity rightToes;
		};
		scene_entity bodyParts[14];
	};

	union
	{
		struct
		{
			scene_entity torsoParent;
			scene_entity headParent;
			scene_entity leftUpperArmParent;
			scene_entity leftLowerArmParent;
			scene_entity rightUpperArmParent;
			scene_entity rightLowerArmParent;
			scene_entity leftUpperLegParent;
			scene_entity leftLowerLegParent;
			scene_entity leftFootParent;
			scene_entity leftToesParent;
			scene_entity rightUpperLegParent;
			scene_entity rightLowerLegParent;
			scene_entity rightFootParent;
			scene_entity rightToesParent;
		};
		scene_entity bodyPartParents[14];
	};

	union
	{
		struct
		{
			cone_twist_constraint_handle neckConstraint;
			cone_twist_constraint_handle leftShoulderConstraint;
			cone_twist_constraint_handle rightShoulderConstraint;
			cone_twist_constraint_handle leftHipConstraint;
			cone_twist_constraint_handle leftAnkleConstraint;
			cone_twist_constraint_handle rightHipConstraint;
			cone_twist_constraint_handle rightAnkleConstraint;

			hinge_joint_constraint_handle leftElbowConstraint;
			hinge_joint_constraint_handle rightElbowConstraint;
			hinge_joint_constraint_handle leftKneeConstraint;
			hinge_joint_constraint_handle leftToesConstraint;
			hinge_joint_constraint_handle rightKneeConstraint;
			hinge_joint_constraint_handle rightToesConstraint;
		};
		struct
		{
			cone_twist_constraint_handle coneTwistConstraints[7];
			hinge_joint_constraint_handle hingeJointConstraints[6];
		};
	};
};
