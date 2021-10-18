#include "pch.h"
#include "locomotion_environment.h"




static void readBodyPartState(const trs& transform, scene_entity entity, vec3& position, vec3& velocity)
{
	transform_component& bodyPartTransform = entity.getComponent<transform_component>();
	rigid_body_component& rb = entity.getComponent<rigid_body_component>();

	position = inverseTransformPosition(transform, rb.getGlobalCOGPosition(bodyPartTransform));
	velocity = inverseTransformDirection(transform, rb.linearVelocity);
}

static void updateConstraint(game_scene& scene, hinge_constraint_handle handle, hinge_action action = {})
{
	hinge_constraint& c = getConstraint(scene, handle);
	c.maxMotorTorque = 200.f;
	c.motorType = constraint_position_motor;
	c.motorTargetAngle = action.targetAngle;
}

static void updateConstraint(game_scene& scene, cone_twist_constraint_handle handle, cone_twist_action action = {})
{
	cone_twist_constraint& c = getConstraint(scene, handle);
	c.maxSwingMotorTorque = 200.f;
	c.maxTwistMotorTorque = 200.f;
	c.swingMotorType = constraint_position_motor;
	c.twistMotorType = constraint_position_motor;
	c.swingMotorTargetAngle = action.swingTargetAngle;
	c.twistMotorTargetAngle = action.twistTargetAngle;
	c.swingMotorAxis = action.swingAxisAngle;
}


void locomotion_environment::initialize(const humanoid_ragdoll& ragdoll)
{
	this->ragdoll = ragdoll;
}

void locomotion_environment::reset(game_scene& scene)
{
	lastSmoothedAction = {};
	applyAction(scene, {});

	headTargetHeight = ragdoll.head.getComponent<transform_component>().position.y;
	torsoVelocityTarget = vec3(0.f);
}

bool locomotion_environment::getState(learning_state& outState)
{
	trs transform = getCoordinateSystem();


	readBodyPartState(transform, ragdoll.head, outState.headPosition, outState.headVelocity);
	bool fallen = false;
	if (outState.headPosition.y < 1.f)
	{
		fallen = true;
	}

	outState.cogVelocity = inverseTransformDirection(transform, ragdoll.torso.getComponent<rigid_body_component>().linearVelocity);

	readBodyPartState(transform, ragdoll.leftToes, outState.leftToesPosition, outState.leftToesVelocity);
	readBodyPartState(transform, ragdoll.rightToes, outState.rightToesPosition, outState.rightToesVelocity);
	readBodyPartState(transform, ragdoll.torso, outState.torsoPosition, outState.torsoVelocity);
	readBodyPartState(transform, ragdoll.leftLowerArm, outState.leftLowerArmPosition, outState.leftLowerArmVelocity);
	readBodyPartState(transform, ragdoll.rightLowerArm, outState.rightLowerArmPosition, outState.rightLowerArmVelocity);

	outState.lastSmoothedAction = lastSmoothedAction;

	return fallen;
}

void locomotion_environment::applyAction(game_scene& scene, const learning_action& action)
{
	const float beta = 0.1f;

	const float* in = (float*)&action;
	float* out = (float*)&lastSmoothedAction;
	for (uint32 i = 0; i < (uint32)sizeof(learning_action) / 4; ++i)
	{
		out[i] = lerp(out[i], in[i], beta);
	}

	for (uint32 i = 0; i < NUM_CONE_TWIST_CONSTRAINTS; ++i)
	{
		updateConstraint(scene, ragdoll.coneTwistConstraints[i], lastSmoothedAction.coneTwistActions[i]);
	}
	for (uint32 i = 0; i < NUM_HINGE_CONSTRAINTS; ++i)
	{
		updateConstraint(scene, ragdoll.hingeConstraints[i], lastSmoothedAction.hingeActions[i]);
	}
}

trs locomotion_environment::getCoordinateSystem()
{
	// Create coordinate system centered at the torso's location (projected onto the ground), with axes constructed from the horizontal heading and global up vector.

	transform_component& torsoTransform = ragdoll.torso.getComponent<transform_component>();

	vec3 forward = torsoTransform.rotation * vec3(0.f, 0.f, -1.f);
	forward.y = 0.f;
	forward = normalize(forward);

	vec3 cog = ragdoll.torso.getComponent<rigid_body_component>().getGlobalCOGPosition(torsoTransform);
	cog.y = 0;

	trs transform(cog, rotateFromTo(vec3(0.f, 0.f, -1.f), forward));
	//trs transform(cog, quat::identity);
	transform.scale = vec3(1.f);

	return transform;
}
