#include "pch.h"
#include "scene/scene.h"
#include "physics/physics.h"
#include "core/random.h"

#include "locomotion_environment.h"


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

struct locomotion_learning_environment : locomotion_environment
{
	void reset();

	float getLastActionReward();

	game_scene scene;
	random_number_generator rng;
	float totalReward;

	learning_target targets[NUM_BODY_PARTS];

	learning_positions localPositions[NUM_BODY_PARTS];
};

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

static void getBodyPartTarget(scene_entity entity, scene_entity parent, learning_target& outTarget, const learning_positions& localPositions, const transform_component& torsoTransform)
{
	transform_component& transform = entity.getComponent<transform_component>();
	rigid_body_component& rb = entity.getComponent<rigid_body_component>();

	for (uint32 i = 0; i < 6; ++i)
	{
		vec3 globalPosition = transformPosition(transform, localPositions.p[i]);
		vec3 pos = inverseTransformPosition(torsoTransform, globalPosition);
		outTarget.targetPositions[i] = pos;

		vec3 globalVelocity = rb.getGlobalPointVelocity(transform, localPositions.p[i]);
		vec3 vel = inverseTransformDirection(torsoTransform, globalVelocity);
		outTarget.targetVelocities[i] = vel;
	}

	quat parentRotation = parent ? parent.getComponent<transform_component>().rotation : quat::identity;
	quat localRotation = transform.rotation * conjugate(parentRotation);
	outTarget.localTargetRotation = localRotation;
}

static body_part_error readPartDifference(scene_entity entity, scene_entity parent, const learning_target& target, const learning_positions& localPositions, const transform_component& torsoTransform)
{
	transform_component& transform = entity.getComponent<transform_component>();
	rigid_body_component& rb = entity.getComponent<rigid_body_component>();


	float positionError = 0.f;
	float velocityError = 0.f;

	for (uint32 i = 0; i < 6; ++i)
	{
		vec3 globalPosition = transformPosition(transform, localPositions.p[i]);
		vec3 pos = inverseTransformPosition(torsoTransform, globalPosition);
		positionError += length(pos - target.targetPositions[i]);

		vec3 globalVelocity = rb.getGlobalPointVelocity(transform, localPositions.p[i]);
		vec3 vel = inverseTransformDirection(torsoTransform, globalVelocity);
		velocityError += length(vel - target.targetVelocities[i]);
	}

	quat parentRotation = parent ? parent.getComponent<transform_component>().rotation : quat::identity;
	quat localRotation = transform.rotation * conjugate(parentRotation);
	quat rotationDifference = target.localTargetRotation * conjugate(localRotation);

	float rotationError = 2.f * acos(clamp01(rotationDifference.w));

	return { positionError, velocityError, rotationError };
}

void locomotion_learning_environment::reset()
{
	totalReward = 0.f;

	scene.clearAll();

	rng = { (uint32)time(0) };

	physics_material groundMaterial = { 0.1f, 1.f, 4.f };

	scene.createEntity("Test ground")
		.addComponent<transform_component>(vec3(0.f, -4.f, 0.f), quat(vec3(1.f, 0.f, 0.f), deg2rad(0.f)))
		.addComponent<collider_component>(collider_component::asAABB(bounding_box::fromCenterRadius(vec3(0.f, 0.f, 0.f), vec3(20.f, 4.f, 20.f)), groundMaterial));

	ragdoll.initialize(scene, vec3(0.f, 1.25f, 0.f));


	transform_component torsoTransform = getCoordinateSystem();

	for (uint32 i = 0; i < NUM_BODY_PARTS; ++i)
	{
		getLocalPositions(ragdoll.bodyParts[i], localPositions[i]);
		getBodyPartTarget(ragdoll.bodyParts[i], ragdoll.bodyPartParents[i], targets[i], localPositions[i], torsoTransform);
	}

	locomotion_environment::reset(scene);
}

float locomotion_learning_environment::getLastActionReward()
{
	//return env->ragdoll.head.getComponent<transform_component>().position.y;

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
	totalReward += result;
	return result;
}










static locomotion_learning_environment* learningEnv = 0;
static memory_arena stackArena;


extern "C" __declspec(dllexport) int getPhysicsStateSize() { return sizeof(learning_state) / 4; }
extern "C" __declspec(dllexport) int getPhysicsActionSize() { return sizeof(learning_action) / 4; }

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

	for (uint32 i = 0; i < NUM_CONE_TWIST_CONSTRAINTS; ++i)
	{
		getLimits(tmpScene, tmpRagdoll.coneTwistConstraints[i], actionMin, minPushIndex, actionMax, maxPushIndex);
	}
	for (uint32 i = 0; i < NUM_HINGE_CONSTRAINTS; ++i)
	{
		getLimits(tmpScene, tmpRagdoll.hingeConstraints[i], actionMin, minPushIndex, actionMax, maxPushIndex);
	}

	assert(minPushIndex == getPhysicsActionSize());
	assert(maxPushIndex == getPhysicsActionSize());

	tmpScene.clearAll();
}

extern "C" __declspec(dllexport) void resetPhysics(float* outState)
{
	if (!learningEnv)
	{
		learningEnv = new locomotion_learning_environment;
		stackArena.initialize();
	}

	learningEnv->reset();

	bool failure = learningEnv->getState(*(learning_state*)outState);
	assert(!failure);
}

extern "C" __declspec(dllexport) int updatePhysics(float* action, float* outState, float* outReward)
{
	stackArena.reset();

	learningEnv->applyAction(learningEnv->scene, *(learning_action*)action);

	if (learningEnv->rng.randomFloat01() < 0.02f)
	{
		uint32 bodyPartIndex = learningEnv->rng.randomUint32Between(0, NUM_BODY_PARTS - 1);
	
		vec3 part = learningEnv->ragdoll.bodyParts[bodyPartIndex].getComponent<transform_component>().position + vec3(0.f, 0.2f, 0.f);
		vec3 direction = normalize(vec3(learningEnv->rng.randomFloatBetween(-1.f, 1.f), 0.f, learningEnv->rng.randomFloatBetween(-1.f, 1.f)));
		vec3 origin = part - direction * 5.f;
	
		testPhysicsInteraction(learningEnv->scene, ray{ origin, direction });
	}

	physicsStep(learningEnv->scene, stackArena, 1.f / 60.f);
	
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




void testLocomotionLearning()
{
	float* state = new float[getPhysicsStateSize()];
	float* action = new float[getPhysicsActionSize()];
	float reward;

	resetPhysics(state);
	while (true)
	{
		memset(action, 0, getPhysicsActionSize() * sizeof(float));

		int failure = updatePhysics(action, state, &reward);
		if (failure)
		{
			break;
		}
	}
}
