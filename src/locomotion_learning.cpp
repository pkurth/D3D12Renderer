#include "pch.h"
#include "scene.h"
#include "ragdoll.h"
#include "physics.h"


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
	cone_twist_action neckConstraint;
	cone_twist_action leftShoulderConstraint;
	hinge_action leftElbowConstraint;
	cone_twist_action rightShoulderConstraint;
	hinge_action rightElbowConstraint;
	cone_twist_action leftHipConstraint;
	hinge_action leftKneeConstraint;
	cone_twist_action leftAnkleConstraint;
	hinge_action leftToesConstraint;
	cone_twist_action rightHipConstraint;
	hinge_action rightKneeConstraint;
	cone_twist_action rightAnkleConstraint;
	hinge_action rightToesConstraint;
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

struct learning_environment
{
	scene appScene;
	humanoid_ragdoll ragdoll;

	learning_action lastSmoothedAction;
};

static learning_environment* env = 0;

extern "C" __declspec(dllexport) int getPhysicsStateSize() { return sizeof(learning_state) / 4; }
extern "C" __declspec(dllexport) int getPhysicsActionSize() { return sizeof(learning_action) / 4; }

static void readBodyPartState(const trs& transform, scene_entity entity, vec3& position, vec3& velocity)
{
	position = inverseTransformPosition(transform, entity.getComponent<trs>().position);
	velocity = inverseTransformDirection(transform, entity.getComponent<rigid_body_component>().linearVelocity);
}

static void updateConstraint(hinge_joint_constraint_handle handle, hinge_action action = {})
{
	hinge_joint_constraint& c = getConstraint(handle);
	c.maxMotorTorque = 200.f;
	c.motorType = constraint_position_motor;
	c.motorTargetAngle = action.targetAngle;
}

static void updateConstraint(cone_twist_constraint_handle handle, cone_twist_action action = {})
{
	cone_twist_constraint& c = getConstraint(handle);
	c.maxSwingMotorTorque = 200.f;
	c.maxTwistMotorTorque = 200.f;
	c.swingMotorType = constraint_position_motor;
	c.twistMotorType = constraint_position_motor;
	c.swingMotorTargetAngle = action.swingTargetAngle;
	c.twistMotorTargetAngle = action.twistTargetAngle;
	c.swingMotorAxis = action.swingAxisAngle;
}

// Returns true, if simulation has ended.
static bool readState(float* outState)
{
	learning_state* s = (learning_state*)outState;

	quat rotation = env->ragdoll.torso.getComponent<trs>().rotation;
	vec3 forward = rotation * vec3(0.f, 0.f, -1.f);
	forward.y = 0.f;
	forward = normalize(forward);

	vec3 cog = env->ragdoll.torso.getComponent<rigid_body_component>().getGlobalCOGPosition(env->ragdoll.torso.getComponent<trs>());
	cog.y = 0;

	trs transform(cog, rotateFromTo(vec3(0.f, 0.f, -1.f), forward));


	readBodyPartState(transform, env->ragdoll.head, s->headPosition, s->headVelocity);
	if (s->headPosition.y < 1.f)
	{
		return true; // Consider the ragdoll fallen.
	}

	s->cogVelocity = inverseTransformDirection(transform, env->ragdoll.torso.getComponent<rigid_body_component>().linearVelocity);
	
	readBodyPartState(transform, env->ragdoll.leftToes, s->leftToesPosition, s->leftToesVelocity);
	readBodyPartState(transform, env->ragdoll.rightToes, s->rightToesPosition, s->rightToesVelocity);
	readBodyPartState(transform, env->ragdoll.torso, s->torsoPosition, s->torsoVelocity);
	readBodyPartState(transform, env->ragdoll.leftLowerArm, s->leftLowerArmPosition, s->leftLowerArmVelocity);
	readBodyPartState(transform, env->ragdoll.rightLowerArm, s->rightLowerArmPosition, s->rightLowerArmVelocity);

	s->lastSmoothedAction = env->lastSmoothedAction;

	return false;
}

static float readReward()
{
	return 1.f;
}

extern "C" __declspec(dllexport) int updatePhysics(float* action, float* outState, float* outReward)
{
	// Smooth action.
	const float beta = 0.2f;

	learning_action& a = env->lastSmoothedAction;
	for (uint32 i = 0; i < (uint32)getPhysicsActionSize(); ++i)
	{
		float* p = (float*)&a;
		p[i] = lerp(p[i], action[i], beta);
	}

	// Apply action.
	updateConstraint(env->ragdoll.neckConstraint, a.neckConstraint);
	updateConstraint(env->ragdoll.leftShoulderConstraint, a.leftShoulderConstraint);
	updateConstraint(env->ragdoll.leftElbowConstraint, a.leftElbowConstraint);
	updateConstraint(env->ragdoll.rightShoulderConstraint, a.rightShoulderConstraint);
	updateConstraint(env->ragdoll.rightElbowConstraint, a.rightElbowConstraint);
	updateConstraint(env->ragdoll.leftHipConstraint, a.leftHipConstraint);
	updateConstraint(env->ragdoll.leftKneeConstraint, a.leftKneeConstraint);
	updateConstraint(env->ragdoll.leftAnkleConstraint, a.leftAnkleConstraint);
	updateConstraint(env->ragdoll.leftToesConstraint, a.leftToesConstraint);
	updateConstraint(env->ragdoll.rightHipConstraint, a.rightHipConstraint);
	updateConstraint(env->ragdoll.rightKneeConstraint, a.rightKneeConstraint);
	updateConstraint(env->ragdoll.rightAnkleConstraint, a.rightAnkleConstraint);


	physicsStep(env->appScene, 1.f / 60.f);
	bool failure = readState(outState);
	*outReward = 0.f;
	if (!failure)
	{
		*outReward = readReward();
	}
	return failure;
}

extern "C" __declspec(dllexport) void resetPhysics(float* outState)
{
	if (!env)
	{
		env = new learning_environment;
	}

	deleteAllConstraints();
	env->appScene.clearAll();
	

	env->appScene.createEntity("Test ground")
		.addComponent<trs>(vec3(0.f, -4.f, 0.f), quat(vec3(1.f, 0.f, 0.f), deg2rad(0.f)))
		.addComponent<collider_component>(bounding_box::fromCenterRadius(vec3(0.f, 0.f, 0.f), vec3(20.f, 4.f, 20.f)), 0.1f, 0.5f, 4.f)
		.addComponent<rigid_body_component>(true);

	env->ragdoll.initialize(env->appScene, vec3(0.f, 1.25f, 0.f));

	// Apply initial action.
	updateConstraint(env->ragdoll.neckConstraint);
	updateConstraint(env->ragdoll.leftShoulderConstraint);
	updateConstraint(env->ragdoll.leftElbowConstraint);
	updateConstraint(env->ragdoll.rightShoulderConstraint);
	updateConstraint(env->ragdoll.rightElbowConstraint);
	updateConstraint(env->ragdoll.leftHipConstraint);
	updateConstraint(env->ragdoll.leftKneeConstraint);
	updateConstraint(env->ragdoll.leftAnkleConstraint);
	updateConstraint(env->ragdoll.leftToesConstraint);
	updateConstraint(env->ragdoll.rightHipConstraint);
	updateConstraint(env->ragdoll.rightKneeConstraint);
	updateConstraint(env->ragdoll.rightAnkleConstraint);

	env->lastSmoothedAction = {};

	bool failure = readState(outState);
	assert(!failure);
}

#if 0
void testLearning()
{
	float* state = new float[getPhysicsStateSize()];
	float* action = new float[getPhysicsActionSize()];


	for (int i = 0; i < 10; ++i)
	{
		resetPhysics(state);
		memset(action, 0, sizeof(float) * getPhysicsActionSize());

		float totalReward = 0.f;
		float reward;
		while (!updatePhysics(action, state, &reward))
		{
			totalReward += reward;
			break;
		}

		std::cout << totalReward << '\n';
		int asdau7s = 0;
	}

	int aasd = 0;
}
#endif
