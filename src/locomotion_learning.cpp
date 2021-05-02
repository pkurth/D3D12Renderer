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

	uint32 numIterations;
};

static learning_environment* env = 0;

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

static void getLimits(hinge_joint_constraint_handle handle, float* minPtr, uint32& minPushIndex, float* maxPtr, uint32& maxPushIndex)
{
	hinge_joint_constraint& c = getConstraint(handle);
	minPtr[minPushIndex++] = c.minRotationLimit <= 0.f ? c.minRotationLimit : -M_PI;
	maxPtr[maxPushIndex++] = c.maxRotationLimit >= 0.f ? c.maxRotationLimit : M_PI;
}

static void getLimits(cone_twist_constraint_handle handle, float* minPtr, uint32& minPushIndex, float* maxPtr, uint32& maxPushIndex)
{
	cone_twist_constraint& c = getConstraint(handle);

	minPtr[minPushIndex++] = c.twistLimit >= 0.f ? -c.twistLimit : -M_PI;
	minPtr[minPushIndex++] = c.swingLimit >= 0.f ? -c.swingLimit : -M_PI;
	minPtr[minPushIndex++] = -M_PI;

	maxPtr[maxPushIndex++] = c.twistLimit >= 0.f ? c.twistLimit : M_PI;
	maxPtr[maxPushIndex++] = c.swingLimit >= 0.f ? c.swingLimit : M_PI;
	maxPtr[maxPushIndex++] = M_PI;
}

// Returns true, if simulation has ended.
static bool readState(const humanoid_ragdoll& ragdoll, learning_state& outState, const learning_action& lastSmoothedAction)
{
	// Create coordinate system centered at the torso's location (projected onto the ground), with axes constructed from the horizontal heading and global up vector.
	quat rotation = ragdoll.torso.getComponent<trs>().rotation;
	vec3 forward = rotation * vec3(0.f, 0.f, -1.f);
	forward.y = 0.f;
	forward = normalize(forward);

	vec3 cog = ragdoll.torso.getComponent<rigid_body_component>().getGlobalCOGPosition(ragdoll.torso.getComponent<trs>());
	cog.y = 0;

	trs transform(cog, rotateFromTo(vec3(0.f, 0.f, -1.f), forward));


	readBodyPartState(transform, ragdoll.head, outState.headPosition, outState.headVelocity);
	if (outState.headPosition.y < 1.f)
	{
		return true; // Consider the ragdoll fallen.
	}

	outState.cogVelocity = inverseTransformDirection(transform, ragdoll.torso.getComponent<rigid_body_component>().linearVelocity);
	
	readBodyPartState(transform, ragdoll.leftToes, outState.leftToesPosition, outState.leftToesVelocity);
	readBodyPartState(transform, ragdoll.rightToes, outState.rightToesPosition, outState.rightToesVelocity);
	readBodyPartState(transform, ragdoll.torso, outState.torsoPosition, outState.torsoVelocity);
	readBodyPartState(transform, ragdoll.leftLowerArm, outState.leftLowerArmPosition, outState.leftLowerArmVelocity);
	readBodyPartState(transform, ragdoll.rightLowerArm, outState.rightLowerArmPosition, outState.rightLowerArmVelocity);

	outState.lastSmoothedAction = lastSmoothedAction;

	return false;
}

static float readReward()
{
	return env->ragdoll.head.getComponent<trs>().position.y; // TODO
}

extern "C" __declspec(dllexport) int getPhysicsStateSize() { return sizeof(learning_state) / 4; }
extern "C" __declspec(dllexport) int getPhysicsActionSize() { return sizeof(learning_action) / 4; }

extern "C" __declspec(dllexport) void getPhysicsRanges(float* stateMin, float* stateMax, float* actionMin, float* actionMax)
{
	scene tmpScene;
	humanoid_ragdoll tmpRagdoll;
	tmpRagdoll.initialize(tmpScene, vec3(0.f));

	// No limits for state.
	for (uint32 i = 0; i < (uint32)getPhysicsStateSize(); ++i)
	{
		stateMin[i] = -FLT_MAX;
		stateMax[i] = FLT_MAX;
	}

	uint32 minPushIndex = 0;
	uint32 maxPushIndex = 0;

	getLimits(tmpRagdoll.neckConstraint, actionMin, minPushIndex, actionMax, maxPushIndex);
	getLimits(tmpRagdoll.leftShoulderConstraint, actionMin, minPushIndex, actionMax, maxPushIndex);
	getLimits(tmpRagdoll.leftElbowConstraint, actionMin, minPushIndex, actionMax, maxPushIndex);
	getLimits(tmpRagdoll.rightShoulderConstraint, actionMin, minPushIndex, actionMax, maxPushIndex);
	getLimits(tmpRagdoll.rightElbowConstraint, actionMin, minPushIndex, actionMax, maxPushIndex);
	getLimits(tmpRagdoll.leftHipConstraint, actionMin, minPushIndex, actionMax, maxPushIndex);
	getLimits(tmpRagdoll.leftKneeConstraint, actionMin, minPushIndex, actionMax, maxPushIndex);
	getLimits(tmpRagdoll.leftAnkleConstraint, actionMin, minPushIndex, actionMax, maxPushIndex);
	getLimits(tmpRagdoll.leftToesConstraint, actionMin, minPushIndex, actionMax, maxPushIndex);
	getLimits(tmpRagdoll.rightHipConstraint, actionMin, minPushIndex, actionMax, maxPushIndex);
	getLimits(tmpRagdoll.rightKneeConstraint, actionMin, minPushIndex, actionMax, maxPushIndex);
	getLimits(tmpRagdoll.rightAnkleConstraint, actionMin, minPushIndex, actionMax, maxPushIndex);
	getLimits(tmpRagdoll.rightToesConstraint, actionMin, minPushIndex, actionMax, maxPushIndex);

	assert(minPushIndex == getPhysicsActionSize());
	assert(maxPushIndex == getPhysicsActionSize());

	deleteAllConstraints();
	tmpScene.clearAll();
}

static void applyAction(humanoid_ragdoll& ragdoll, const learning_action& action)
{
	updateConstraint(ragdoll.neckConstraint, action.neckConstraint);
	updateConstraint(ragdoll.leftShoulderConstraint, action.leftShoulderConstraint);
	updateConstraint(ragdoll.leftElbowConstraint, action.leftElbowConstraint);
	updateConstraint(ragdoll.rightShoulderConstraint, action.rightShoulderConstraint);
	updateConstraint(ragdoll.rightElbowConstraint, action.rightElbowConstraint);
	updateConstraint(ragdoll.leftHipConstraint, action.leftHipConstraint);
	updateConstraint(ragdoll.leftKneeConstraint, action.leftKneeConstraint);
	updateConstraint(ragdoll.leftAnkleConstraint, action.leftAnkleConstraint);
	updateConstraint(ragdoll.leftToesConstraint, action.leftToesConstraint);
	updateConstraint(ragdoll.rightHipConstraint, action.rightHipConstraint);
	updateConstraint(ragdoll.rightKneeConstraint, action.rightKneeConstraint);
	updateConstraint(ragdoll.rightAnkleConstraint, action.rightAnkleConstraint);
	updateConstraint(ragdoll.rightToesConstraint, action.rightToesConstraint);
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

	applyAction(env->ragdoll, a);

	physicsStep(env->appScene, 1.f / 60.f);

	bool failure = readState(env->ragdoll, *(learning_state*)outState, env->lastSmoothedAction);
	*outReward = 0.f;
	if (!failure)
	{
		*outReward = readReward();
		++env->numIterations;
	}
	else
	{
		//std::cout << "Dead after " << env->numIterations << " iterations.\n";
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
	env->numIterations = 0;
	

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
	updateConstraint(env->ragdoll.rightToesConstraint);

	env->lastSmoothedAction = {};

	bool failure = readState(env->ragdoll, *(learning_state*)outState, env->lastSmoothedAction);
	assert(!failure);
}

#if 0
void testLearning()
{
	float* state = new float[getPhysicsStateSize()];
	float* action = new float[getPhysicsActionSize()];

	float* stateMin = new float[getPhysicsStateSize()];
	float* stateMax = new float[getPhysicsStateSize()];
	float* actionMin = new float[getPhysicsActionSize()];
	float* actionMax = new float[getPhysicsActionSize()];

	getPhysicsRanges(stateMin, stateMax, actionMin, actionMax);

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

	deleteAllConstraints();
	env->appScene.clearAll();
	env->numIterations = 0;

	delete env;
}
#endif




#ifndef PHYSICS_ONLY
#undef min
#undef max
#undef rad2deg
#undef deg2rad
#undef M_PI
#pragma warning( disable : 4067 4624 4275 4251 4244 4996 4267 )
#include <torch/torch.h>
#include <torch/script.h>

struct locomotion_nn
{
	torch::jit::script::Module network;
	learning_action lastSmoothedAction;

	void initialize()
	{
		try
		{
			network = torch::jit::load("tmp/testexport.pt");
		}
		catch (c10::Error e)
		{
			std::cout << e.what() << '\n';
		}
		lastSmoothedAction = {};
	}

	void update(humanoid_ragdoll& ragdoll)
	{
		learning_state state;
		readState(ragdoll, state, lastSmoothedAction);

		std::vector<torch::jit::IValue> inputs;
		auto options = torch::TensorOptions().dtype(torch::kFloat32);
		inputs.push_back(torch::from_blob(&state, { sizeof(state) / 4 }, options).unsqueeze(0));

		try
		{
			auto output = network.forward(inputs);
			auto outputTuple = output.toTuple();
			auto elem0 = outputTuple->elements()[0];
			auto outputTensor = elem0.toTensor();

			float* result = outputTensor.data_ptr<float>();

			learning_action& action = *(learning_action*)result;

			applyAction(ragdoll, action);
		}
		catch (c10::Error e)
		{
			std::cout << e.what() << '\n';
		}
	}
};

static locomotion_nn nn;

void initializeLocomotionEval()
{
	nn.initialize();
}

void stepLocomotionEval(humanoid_ragdoll& ragdoll)
{
	nn.update(ragdoll);
}

#endif
