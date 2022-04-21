#pragma once

#include "physics/ragdoll.h"

struct learned_locomotion
{
	void initialize(game_scene& scene, const humanoid_ragdoll& ragdoll);
	void reset(game_scene& scene);

	void update(game_scene& scene);


	static const uint32 NUM_CONE_TWIST_CONSTRAINTS =  arraysize(humanoid_ragdoll::coneTwistConstraints);
	static const uint32 NUM_HINGE_CONSTRAINTS = arraysize(humanoid_ragdoll::hingeConstraints);
	static const uint32 NUM_BODY_PARTS = arraysize(humanoid_ragdoll::bodyParts);

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

	humanoid_ragdoll ragdoll;

protected:
	void updateConstraint(game_scene& scene, hinge_constraint_handle handle, hinge_action action = {}) const;
	void updateConstraint(game_scene& scene, cone_twist_constraint_handle handle, cone_twist_action action = {}) const;
	void applyAction(game_scene& scene, const learning_action& action);

	trs getCoordinateSystem() const;
	void readBodyPartState(const trs& transform, scene_entity entity, vec3& position, vec3& velocity) const;
	void getState(learning_state& outState) const;
	bool hasFallen(const learning_state& state) const;


	learning_action lastSmoothedAction;
	float headTargetHeight;
	vec3 torsoVelocityTarget;

};

