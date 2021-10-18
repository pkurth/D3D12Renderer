#pragma once

#include "core/math.h"
#include "physics/ragdoll.h"



#define NUM_CONE_TWIST_CONSTRAINTS arraysize(humanoid_ragdoll::coneTwistConstraints)
#define NUM_HINGE_CONSTRAINTS arraysize(humanoid_ragdoll::hingeConstraints)
#define NUM_BODY_PARTS arraysize(humanoid_ragdoll::bodyParts)

struct hinge_action
{
	float targetAngle;
};

struct cone_twist_action
{
	float twistTargetAngle;
	float swingTargetAngle;
	float swingAxisAngle;
};

struct learning_action
{
	cone_twist_action coneTwistActions[NUM_CONE_TWIST_CONSTRAINTS];
	hinge_action hingeActions[NUM_HINGE_CONSTRAINTS];
};

struct learning_target
{
	vec3 targetPositions[6];
	vec3 targetVelocities[6];
	quat localTargetRotation;
};

struct learning_state
{
	vec3 cogVelocity;

	vec3 leftToesPosition;
	vec3 leftToesVelocity;

	vec3 rightToesPosition;
	vec3 rightToesVelocity;

	vec3 torsoPosition;
	vec3 torsoVelocity;

	vec3 headPosition;
	vec3 headVelocity;

	vec3 leftLowerArmPosition;
	vec3 leftLowerArmVelocity;

	vec3 rightLowerArmPosition;
	vec3 rightLowerArmVelocity;

	learning_action lastSmoothedAction;
};


struct locomotion_environment
{
	void initialize(const humanoid_ragdoll& ragdoll);
	virtual void reset(game_scene& scene);

	// Returns true, if simulation has ended.
	bool getState(learning_state& outState);
	void applyAction(game_scene& scene, const learning_action& action);

	trs getCoordinateSystem();

	humanoid_ragdoll ragdoll;

	learning_action lastSmoothedAction;
	float headTargetHeight;
	vec3 torsoVelocityTarget;
};
