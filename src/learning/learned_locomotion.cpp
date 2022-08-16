#include "pch.h"
#include "learned_locomotion.h"


#if __has_include("../tmp/network.h")
#define INFERENCE_POSSIBLE
#include "../tmp/network.h"


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
#endif

void learned_locomotion::initialize(game_scene& scene, const humanoid_ragdoll& ragdoll)
{
	this->ragdoll = ragdoll;
	reset(scene);
}

void learned_locomotion::reset(game_scene& scene)
{
	lastSmoothedAction = {};
	applyAction(scene, {});

	headTargetHeight = ragdoll.head.getComponent<transform_component>().position.y;
	torsoVelocityTarget = vec3(0.f);
}

void learned_locomotion::update(game_scene& scene)
{
#ifdef INFERENCE_POSSIBLE
	learning_state state;
	getState(state);

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

	applyAction(scene, action);
#endif
}




void learned_locomotion::updateConstraint(game_scene& scene, hinge_constraint_handle handle, hinge_action action) const
{
	hinge_constraint& c = getConstraint(scene, handle);
	c.maxMotorTorque = 200.f;
	c.motorType = constraint_position_motor;
	c.motorTargetAngle = action.targetAngle;
}

void learned_locomotion::updateConstraint(game_scene& scene, cone_twist_constraint_handle handle, cone_twist_action action) const
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

void learned_locomotion::applyAction(game_scene& scene, const learning_action& action)
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

trs learned_locomotion::getCoordinateSystem() const
{
	const transform_component& torsoTransform = ragdoll.torso.getComponent<transform_component>();
	vec3 cog = ragdoll.torso.getComponent<rigid_body_component>().getGlobalCOGPosition(torsoTransform);
	cog.y = 0;

	trs transform(cog, quat::identity);
	return transform;
}

void learned_locomotion::readBodyPartState(const trs& transform, scene_entity entity, vec3& position, vec3& velocity) const
{
	transform_component& bodyPartTransform = entity.getComponent<transform_component>();
	rigid_body_component& rb = entity.getComponent<rigid_body_component>();

	position = inverseTransformPosition(transform, rb.getGlobalCOGPosition(bodyPartTransform));
	velocity = inverseTransformDirection(transform, rb.linearVelocity);
}

void learned_locomotion::getState(learning_state& outState) const
{
	trs transform = getCoordinateSystem();

	readBodyPartState(transform, ragdoll.head, outState.headPosition, outState.headVelocity);

	outState.cogVelocity = inverseTransformDirection(transform, ragdoll.torso.getComponent<rigid_body_component>().linearVelocity);

	readBodyPartState(transform, ragdoll.leftToes, outState.leftToesPosition, outState.leftToesVelocity);
	readBodyPartState(transform, ragdoll.rightToes, outState.rightToesPosition, outState.rightToesVelocity);
	readBodyPartState(transform, ragdoll.torso, outState.torsoPosition, outState.torsoVelocity);
	readBodyPartState(transform, ragdoll.leftLowerArm, outState.leftLowerArmPosition, outState.leftLowerArmVelocity);
	readBodyPartState(transform, ragdoll.rightLowerArm, outState.rightLowerArmPosition, outState.rightLowerArmVelocity);

	outState.lastSmoothedAction = lastSmoothedAction;
}

bool learned_locomotion::hasFallen(const learning_state& state) const
{
	return (state.headPosition.y < 1.f);
}





// --------------------------------
// TRAINING
// --------------------------------


struct training_locomotion : learned_locomotion
{
	void reset(game_scene& scene);

	void applyAction(game_scene& scene, const learning_action& action);
	bool getState(learning_state& outState) const;
	float getReward() const;

private:

	struct learning_positions
	{
		vec3 p[6];
	};

	struct body_part_error
	{
		float positionError;
		float velocityError;
		float rotationError;
	};

	void getLocalPositions(scene_entity entity, learning_positions& outPositions) const;
	void getBodyPartTarget(scene_entity entity, scene_entity parent, learning_target& outTarget, const learning_positions& localPositions, const transform_component& torsoTransform) const;
	body_part_error readPartDifference(scene_entity entity, scene_entity parent, const learning_target& target, const learning_positions& localPositions, const transform_component& torsoTransform) const;


	learning_positions localPositions[NUM_BODY_PARTS];
	learning_target targets[NUM_BODY_PARTS];
};

void training_locomotion::getLocalPositions(scene_entity entity, learning_positions& outPositions) const
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

void training_locomotion::getBodyPartTarget(scene_entity entity, scene_entity parent, learning_target& outTarget, const learning_positions& localPositions, const transform_component& torsoTransform) const
{
	transform_component& transform = entity.getComponent<transform_component>();
	rigid_body_component& rb = entity.getComponent<rigid_body_component>();

	for (uint32 i = 0; i < 6; ++i)
	{
		vec3 pos = transformPosition(transform, localPositions.p[i]);
		//pos = inverseTransformPosition(torsoTransform, pos);
		outTarget.targetPositions[i] = pos;

		vec3 vel = rb.getGlobalPointVelocity(transform, localPositions.p[i]);
		//vel = inverseTransformDirection(torsoTransform, vel);
		outTarget.targetVelocities[i] = vel;
	}

	quat parentRotation = parent ? parent.getComponent<transform_component>().rotation : quat::identity;
	quat localRotation = transform.rotation * conjugate(parentRotation);
	outTarget.localTargetRotation = localRotation;
}

training_locomotion::body_part_error training_locomotion::readPartDifference(scene_entity entity, scene_entity parent, const learning_target& target, const learning_positions& localPositions, const transform_component& torsoTransform) const
{
	transform_component& transform = entity.getComponent<transform_component>();
	rigid_body_component& rb = entity.getComponent<rigid_body_component>();

	float positionError = 0.f;
	float velocityError = 0.f;

	for (uint32 i = 0; i < 6; ++i)
	{
		vec3 pos = transformPosition(transform, localPositions.p[i]);
		//pos = inverseTransformPosition(torsoTransform, pos);
		positionError += length(pos - target.targetPositions[i]);

		vec3 vel = rb.getGlobalPointVelocity(transform, localPositions.p[i]);
		//vel = inverseTransformDirection(torsoTransform, vel);
		velocityError += length(vel - target.targetVelocities[i]);
	}

	quat parentRotation = parent ? parent.getComponent<transform_component>().rotation : quat::identity;
	quat localRotation = transform.rotation * conjugate(parentRotation);
	quat rotationDifference = target.localTargetRotation * conjugate(localRotation);

	float rotationError = 2.f * acos(clamp(rotationDifference.cosHalfAngle, -1.f, 1.f));

	return { positionError, velocityError, rotationError };
}

void training_locomotion::reset(game_scene& scene)
{
	transform_component torsoTransform = getCoordinateSystem();

	for (uint32 i = 0; i < NUM_BODY_PARTS; ++i)
	{
		getLocalPositions(ragdoll.bodyParts[i], localPositions[i]);
		getBodyPartTarget(ragdoll.bodyParts[i], ragdoll.bodyPartParents[i], targets[i], localPositions[i], torsoTransform);
	}

	learned_locomotion::reset(scene);
}

void training_locomotion::applyAction(game_scene& scene, const learning_action& action)
{
	learned_locomotion::applyAction(scene, action);
}

bool training_locomotion::getState(learning_state& outState) const
{
	learned_locomotion::getState(outState);
	bool fallen = hasFallen(outState);
	return fallen;
}

float training_locomotion::getReward() const
{
	float positionError = 0.f;
	float velocityError = 0.f;
	float rotationError = 0.f;

	transform_component torsoTransform = getCoordinateSystem();

	for (uint32 i = 0; i < NUM_BODY_PARTS; ++i)
	{
		body_part_error err = readPartDifference(ragdoll.bodyParts[i], ragdoll.bodyPartParents[i], targets[i], localPositions[i], torsoTransform);
		positionError += err.positionError;
		velocityError += err.velocityError;
		rotationError += err.rotationError;
	}

	float vcmError = length(ragdoll.torso.getComponent<rigid_body_component>().linearVelocity - torsoVelocityTarget);

	float rp = exp(-10.f / NUM_BODY_PARTS * positionError);
	float rv = exp(-1.f / NUM_BODY_PARTS * velocityError);
	float rlocal = exp(-10.f / NUM_BODY_PARTS * rotationError);
	float rvcm = exp(-vcmError);

	float headHeight = ragdoll.head.getComponent<transform_component>().position.y;
	float fall = clamp01(1.3f - 1.4f * (headTargetHeight - headHeight));

	float result = fall * (rp + rv + rlocal + rvcm);
	return result;
}







static void getLimits(game_scene& scene, hinge_constraint_handle handle, float* minPtr, uint32& minPushIndex, float* maxPtr, uint32& maxPushIndex)
{
	hinge_constraint& c = getConstraint(scene, handle);
	minPtr[minPushIndex++] = c.minRotationLimit <= 0.f ? c.minRotationLimit : -M_PI;
	maxPtr[maxPushIndex++] = c.maxRotationLimit >= 0.f ? c.maxRotationLimit : M_PI;
}

static void getLimits(game_scene& scene, cone_twist_constraint_handle handle, float* minPtr, uint32& minPushIndex, float* maxPtr, uint32& maxPushIndex)
{
	cone_twist_constraint& c = getConstraint(scene, handle);

	minPtr[minPushIndex++] = c.twistLimit >= 0.f ? -c.twistLimit : -M_PI;
	minPtr[minPushIndex++] = c.swingLimit >= 0.f ? -c.swingLimit : -M_PI;
	minPtr[minPushIndex++] = -M_PI;

	maxPtr[maxPushIndex++] = c.twistLimit >= 0.f ? c.twistLimit : M_PI;
	maxPtr[maxPushIndex++] = c.swingLimit >= 0.f ? c.swingLimit : M_PI;
	maxPtr[maxPushIndex++] = M_PI;
}





static training_locomotion* trainingEnv = 0;
static memory_arena stackArena;
static game_scene trainingScene;
static float totalReward;
static random_number_generator rng = { (uint32)time(0) };

extern "C" __declspec(dllexport) int getPhysicsStateSize() { return sizeof(learned_locomotion::learning_state) / 4; }
extern "C" __declspec(dllexport) int getPhysicsActionSize() { return sizeof(learned_locomotion::learning_action) / 4; }

extern "C" __declspec(dllexport) void getPhysicsRanges(float* stateMin, float* stateMax, float* actionMin, float* actionMax)
{
	game_scene tmpScene;
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

	for (uint32 i = 0; i < learned_locomotion::NUM_CONE_TWIST_CONSTRAINTS; ++i)
	{
		getLimits(tmpScene, tmpRagdoll.coneTwistConstraints[i], actionMin, minPushIndex, actionMax, maxPushIndex);
	}
	for (uint32 i = 0; i < learned_locomotion::NUM_HINGE_CONSTRAINTS; ++i)
	{
		getLimits(tmpScene, tmpRagdoll.hingeConstraints[i], actionMin, minPushIndex, actionMax, maxPushIndex);
	}

	assert(minPushIndex == getPhysicsActionSize());
	assert(maxPushIndex == getPhysicsActionSize());

	tmpScene.clearAll();
}

extern "C" __declspec(dllexport) void resetPhysics(float* outState)
{
	if (!trainingEnv)
	{
		trainingEnv = new training_locomotion;
		stackArena.initialize();
	}

	totalReward = 0.f;
	trainingScene.clearAll();

	physics_material groundMaterial = { physics_material_type_metal, 0.1f, 1.f, 4.f };

	trainingScene.createEntity("Test ground")
		.addComponent<transform_component>(vec3(0.f, -4.f, 0.f), quat(vec3(1.f, 0.f, 0.f), deg2rad(0.f)))
		.addComponent<collider_component>(collider_component::asAABB(bounding_box::fromCenterRadius(vec3(0.f, 0.f, 0.f), vec3(20.f, 4.f, 20.f)), groundMaterial));

	humanoid_ragdoll ragdoll = humanoid_ragdoll::create(trainingScene, vec3(0.f, 1.25f, 0.f));

	trainingEnv->ragdoll = ragdoll;
	trainingEnv->reset(trainingScene);
}

extern "C" __declspec(dllexport) int updatePhysics(float* action, float* outState, float* outReward)
{
	stackArena.reset();

	trainingEnv->applyAction(trainingScene, *(learned_locomotion::learning_action*)action);

	if (rng.randomFloat01() < 0.02f)
	{
		uint32 bodyPartIndex = rng.randomUint32Between(0, learned_locomotion::NUM_BODY_PARTS - 1);

		vec3 part = trainingEnv->ragdoll.bodyParts[bodyPartIndex].getComponent<transform_component>().position + vec3(0.f, 0.2f, 0.f);
		vec3 direction = normalize(vec3(rng.randomFloatBetween(-1.f, 1.f), 0.f, rng.randomFloatBetween(-1.f, 1.f)));
		vec3 origin = part - direction * 5.f;

		testPhysicsInteraction(trainingScene, ray{ origin, direction });
	}

	physicsSettings.frameRate = 60;
	physicsStep(trainingScene, stackArena);

	bool failure = trainingEnv->getState(*(learned_locomotion::learning_state*)outState);
	*outReward = 0.f;
	if (!failure)
	{
		*outReward = trainingEnv->getReward();
		totalReward += *outReward;
	}
	else
	{
		//std::string out = "Rew: " + std::to_string(learningEnv->totalReward) + ",  EpLen: " + std::to_string(learningEnv->episodeLength) + '\n';
		//std::cout << out;
	}
	return failure;
}
