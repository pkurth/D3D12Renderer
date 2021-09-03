#include "pch.h"
#include "locomotion_learning.h"
#include "scene/scene.h"
#include "physics/ragdoll.h"
#include "physics/physics.h"
#include "core/random.h"

#define NUM_CONE_TWIST_CONSTRAINTS arraysize(humanoid_ragdoll::coneTwistConstraints)
#define NUM_HINGE_JOINT_CONSTRAINTS arraysize(humanoid_ragdoll::hingeJointConstraints)
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

struct learning_positions
{
	vec3 p[6];
};

struct learning_target
{
	vec3 targetPositions[6];
	vec3 targetVelocities[6];
	quat localTargetRotation;
};

struct body_part_error
{
	float positionError;
	float velocityError;
	float rotationError;
};

struct learning_action
{
	cone_twist_action coneTwistActions[NUM_CONE_TWIST_CONSTRAINTS];
	hinge_action hingeJointActions[NUM_HINGE_JOINT_CONSTRAINTS];
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
	void resetForLearning();
	void resetForEvaluation();

	void resetCommon();

	// Returns true, if simulation has ended.
	bool getState(learning_state& outState);
	void applyAction(const learning_action& action);
	float getLastActionReward();



	scene& appScene;
	humanoid_ragdoll ragdoll;

	uint32 episodeLength;
	float totalReward;

	learning_action lastSmoothedAction;

	learning_target targets[NUM_BODY_PARTS];
	learning_positions localPositions[NUM_BODY_PARTS];
	float headTargetHeight;
	vec3 torsoVelocityTarget;

	random_number_generator rng;
};

static locomotion_environment* learningEnv = 0;

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

bool locomotion_environment::getState(learning_state& outState)
{
	// Create coordinate system centered at the torso's location (projected onto the ground), with axes constructed from the horizontal heading and global up vector.
	//quat rotation = ragdoll.torso.getComponent<trs>().rotation;
	//vec3 forward = rotation * vec3(0.f, 0.f, -1.f);
	//forward.y = 0.f;
	//forward = normalize(forward);
	//
	vec3 cog = ragdoll.torso.getComponent<rigid_body_component>().getGlobalCOGPosition(ragdoll.torso.getComponent<trs>());
	cog.y = 0;
	//
	//trs transform(cog, rotateFromTo(vec3(0.f, 0.f, -1.f), forward));
	trs transform(cog, quat::identity);


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

static void getLocalPositions(scene_entity entity, learning_positions& outPositions)
{
	physics_reference_component& reference = entity.getComponent<physics_reference_component>();

	scene_entity colliderEntity = { reference.firstColliderEntity, entity.registry };

	bounding_box aabb = bounding_box::negativeInfinity();

	while (colliderEntity)
	{
		collider_component& collider = colliderEntity.getComponent<collider_component>();
		
		bounding_box bb;
		switch (collider.type)
		{
			case collider_type_sphere:
			{
				bb = bounding_box::fromCenterRadius(collider.sphere.center, collider.sphere.radius);
			} break;
			case collider_type_capsule:
			{
				float radius = collider.capsule.radius;
				vec3 radius3(radius);

				bb = bounding_box::negativeInfinity();
				bb.grow(collider.capsule.positionA + radius3);
				bb.grow(collider.capsule.positionA - radius3);
				bb.grow(collider.capsule.positionB + radius3);
				bb.grow(collider.capsule.positionB - radius3);
			} break;
			case collider_type_aabb:
			{
				bb = collider.aabb;
			} break;
			case collider_type_obb:
			{
				bb = collider.obb.transformToAABB(quat::identity, vec3(0.f));
			} break;
			case collider_type_hull:
			{
				assert(false);
				bb = bounding_box::negativeInfinity();
			} break;
		}
		aabb.grow(bb.minCorner);
		aabb.grow(bb.maxCorner);
		
		colliderEntity = { collider.nextEntity, entity.registry };
	}

	vec3 c = aabb.getCenter();
	vec3 r = aabb.getRadius();
	outPositions.p[0] = c - vec3(r.x, 0.f, 0.f);
	outPositions.p[1] = c - vec3(0.f, r.y, 0.f);
	outPositions.p[2] = c - vec3(0.f, 0.f, r.z);
	outPositions.p[3] = c + vec3(r.x, 0.f, 0.f);
	outPositions.p[4] = c + vec3(0.f, r.y, 0.f);
	outPositions.p[5] = c + vec3(0.f, 0.f, r.z);
}

static void getBodyPartTarget(scene_entity entity, scene_entity parent, learning_target& outTarget, const learning_positions& localPositions)
{
	trs& transform = entity.getComponent<trs>();
	rigid_body_component& rb = entity.getComponent<rigid_body_component>();

	for (uint32 i = 0; i < 6; ++i)
	{
		vec3 globalPosition = transformPosition(transform, localPositions.p[i]);
		outTarget.targetPositions[i] = globalPosition;

		vec3 globalVelocity = rb.getGlobalPointVelocity(transform, localPositions.p[i]);
		outTarget.targetVelocities[i] = globalVelocity;
	}

	quat parentRotation = parent ? parent.getComponent<trs>().rotation : quat::identity;
	quat localRotation = transform.rotation * conjugate(parentRotation);
	outTarget.localTargetRotation = localRotation;
}

static body_part_error readPartDifference(scene_entity entity, scene_entity parent, const learning_target& target, const learning_positions& localPositions)
{
	trs& transform = entity.getComponent<trs>();
	rigid_body_component& rb = entity.getComponent<rigid_body_component>();


	float positionError = 0.f;
	float velocityError = 0.f;

	for (uint32 i = 0; i < 6; ++i)
	{
		vec3 globalPosition = transformPosition(transform, localPositions.p[i]);
		positionError += length(globalPosition - target.targetPositions[i]);

		vec3 globalVelocity = rb.getGlobalPointVelocity(transform, localPositions.p[i]);
		velocityError += length(globalVelocity - target.targetVelocities[i]);
	}

	quat parentRotation = parent ? parent.getComponent<trs>().rotation : quat::identity;
	quat localRotation = transform.rotation * conjugate(parentRotation);
	quat rotationDifference = target.localTargetRotation * conjugate(localRotation);

	float rotationError = 2.f * acos(clamp01(rotationDifference.w));

	return { positionError, velocityError, rotationError };
}

float locomotion_environment::getLastActionReward()
{
	//return env->ragdoll.head.getComponent<trs>().position.y;

	float positionError = 0.f;
	float velocityError = 0.f;
	float rotationError = 0.f;

	for (uint32 i = 0; i < NUM_BODY_PARTS; ++i)
	{
		body_part_error err = readPartDifference(ragdoll.bodyParts[i], ragdoll.bodyPartParents[i], targets[i], localPositions[i]);
		positionError += err.positionError;
		velocityError += err.velocityError;
		rotationError += err.rotationError;
	}

	float vcmError = length(ragdoll.torso.getComponent<rigid_body_component>().linearVelocity - torsoVelocityTarget);

	float rp = exp(-10.f / NUM_BODY_PARTS * positionError);
	float rv = exp(-1.f / NUM_BODY_PARTS * velocityError);
	float rlocal = exp(-10.f / NUM_BODY_PARTS * rotationError);
	float rvcm = exp(-vcmError);

	float headHeight = ragdoll.head.getComponent<trs>().position.y;
	float fall = clamp01(1.3f - 1.4f * (headTargetHeight - headHeight));

	float result = fall * (rp + rv + rlocal + rvcm);
	totalReward += result;
	return result;
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

	for (uint32 i = 0; i < NUM_CONE_TWIST_CONSTRAINTS; ++i)
	{
		getLimits(tmpRagdoll.coneTwistConstraints[i], actionMin, minPushIndex, actionMax, maxPushIndex);
	}
	for (uint32 i = 0; i < NUM_HINGE_JOINT_CONSTRAINTS; ++i)
	{
		getLimits(tmpRagdoll.hingeJointConstraints[i], actionMin, minPushIndex, actionMax, maxPushIndex);
	}

	assert(minPushIndex == getPhysicsActionSize());
	assert(maxPushIndex == getPhysicsActionSize());

	deleteAllConstraints();
	tmpScene.clearAll();
}

void locomotion_environment::applyAction(const learning_action& action)
{
	const float beta = 0.2f;

	const float* in = (float*)&action;
	float* out = (float*)&lastSmoothedAction;
	for (uint32 i = 0; i < (uint32)getPhysicsActionSize(); ++i)
	{
		out[i] = lerp(out[i], in[i], beta);
	}

	for (uint32 i = 0; i < NUM_CONE_TWIST_CONSTRAINTS; ++i)
	{
		updateConstraint(ragdoll.coneTwistConstraints[i], lastSmoothedAction.coneTwistActions[i]);
	}
	for (uint32 i = 0; i < NUM_HINGE_JOINT_CONSTRAINTS; ++i)
	{
		updateConstraint(ragdoll.hingeJointConstraints[i], lastSmoothedAction.hingeJointActions[i]);
	}

	++episodeLength;
}

extern "C" __declspec(dllexport) int updatePhysics(float* action, float* outState, float* outReward)
{
	learningEnv->applyAction(*(learning_action*)action);

	if (learningEnv->rng.randomFloat01() < 0.02f)
	{
		uint32 bodyPartIndex = learningEnv->rng.randomUintBetween(0, NUM_BODY_PARTS - 1);

		vec3 part = learningEnv->ragdoll.bodyParts[bodyPartIndex].getComponent<trs>().position + vec3(0.f, 0.2f, 0.f);
		vec3 direction = normalize(vec3(learningEnv->rng.randomFloatBetween(-1.f, 1.f), 0.f, learningEnv->rng.randomFloatBetween(-1.f, 1.f)));
		vec3 origin = part - direction * 5.f;
	
		testPhysicsInteraction(learningEnv->appScene, ray{ origin, direction }, 1000.f);
	}

	physicsStep(learningEnv->appScene, 1.f / 60.f);
	
	bool failure = learningEnv->getState(*(learning_state*)outState);
	*outReward = 0.f;
	if (!failure)
	{
		*outReward = learningEnv->getLastActionReward();
	}
	else
	{
		//std::string out = "Rew: " + std::to_string(learningEnv->totalReward) + ",  EpLen: " + std::to_string(learningEnv->episodeLength) + '\n';
		//std::cout << out;
	}
	return failure;
}

void locomotion_environment::resetForLearning()
{
	deleteAllConstraints();
	appScene.clearAll();

	uint32 seed = (uint32)time(0);
	rng = { seed };

	appScene.createEntity("Test ground")
		.addComponent<trs>(vec3(0.f, -4.f, 0.f), quat(vec3(1.f, 0.f, 0.f), deg2rad(0.f)))
		.addComponent<collider_component>(collider_component::asAABB(bounding_box::fromCenterRadius(vec3(0.f, 0.f, 0.f), vec3(20.f, 4.f, 20.f)), 0.1f, 1.f, 4.f))
		.addComponent<rigid_body_component>(true);

	ragdoll.initialize(appScene, vec3(0.f, 1.25f, 0.f));

	resetCommon();
}

void locomotion_environment::resetForEvaluation()
{
	resetCommon();
}

void locomotion_environment::resetCommon()
{
	episodeLength = 0;
	totalReward = 0.f;
	lastSmoothedAction = {};
	applyAction({});

	headTargetHeight = ragdoll.head.getComponent<trs>().position.y;
	torsoVelocityTarget = vec3(0.f);

	for (uint32 i = 0; i < NUM_BODY_PARTS; ++i)
	{
		getLocalPositions(ragdoll.bodyParts[i], localPositions[i]);
		getBodyPartTarget(ragdoll.bodyParts[i], ragdoll.bodyPartParents[i], targets[i], localPositions[i]);
	}
}

extern "C" __declspec(dllexport) void resetPhysics(float* outState)
{
	static scene appScene;
	if (!learningEnv)
	{
		learningEnv = new locomotion_environment{ appScene };
	}

	learningEnv->resetForLearning();

	bool failure = learningEnv->getState(*(learning_state*)outState);
	assert(!failure);
}







template <uint32 inputSize, uint32 outputSize>
static void applyLayer(const float(&weights)[outputSize][inputSize], const float(&bias)[outputSize], const float* from, float* to, bool activation)
{
	for (uint32 y = 0; y < outputSize; ++y)
	{
		const float* row = weights[y];

		float sum = 0.f;
		for (uint32 x = 0; x < inputSize; ++x)
		{
			sum += row[x] * from[x];
		}
		sum += bias[y];
		to[y] = activation ? tanh(sum) : sum;
	}
}

#if __has_include("../tmp/network.h")
#include "../tmp/network.h"

static locomotion_environment* evaluationEnv;

void initializeLocomotionEval(scene& appScene, humanoid_ragdoll& ragdoll)
{
	evaluationEnv = new locomotion_environment{ appScene, ragdoll };
	evaluationEnv->resetForEvaluation();
}

void stepLocomotionEval()
{
	if (!evaluationEnv)
	{
		return;
	}

	learning_state state;
	if (evaluationEnv->getState(state))
	{
		std::string out = "Rew: " + std::to_string(evaluationEnv->totalReward) + ",  EpLen: " + std::to_string(evaluationEnv->episodeLength) + '\n';
		std::cout << out;
		evaluationEnv->episodeLength = 0;
		evaluationEnv->totalReward = 0.f;
		evaluationEnv->lastSmoothedAction = {};
		evaluationEnv->applyAction({});
	}

	learning_action action;

	float* buffers = (float*)alloca(sizeof(float) * HIDDEN_LAYER_SIZE * 2);

	float* a = buffers;
	float* b = buffers + HIDDEN_LAYER_SIZE;

	// State to a.
	applyLayer(policyWeights1, policyBias1, (const float*)&state, a, true);

	// a to b.
	applyLayer(policyWeights2, policyBias2, a, b, true);

	// b to action.
	applyLayer(actionWeights, actionBias, b, (float*)&action, false);


	evaluationEnv->applyAction(action);
	evaluationEnv->getLastActionReward();
}
#else
void initializeLocomotionEval(scene& appScene, humanoid_ragdoll& ragdoll) {}
void stepLocomotionEval() {}
#endif


