#pragma once

#include "math.h"
#include "bounding_volumes.h"
#include "scene.h"
#include "constraints.h"

#define GRAVITY -9.81f

struct rigid_body_component
{
	rigid_body_component(bool kinematic, float gravityFactor = 1.f, float linearDamping = 0.4f, float angularDamping = 0.4f);
	void recalculateProperties(entt::registry* registry, const struct physics_reference_component& reference);
	vec3 getGlobalCOGPosition(const trs& transform) const;

	// In entity's local space.
	vec3 localCOGPosition;
	float invMass;
	mat3 invInertia;

	float gravityFactor;
	float linearDamping;
	float angularDamping;

	// In global space.
	vec3 linearVelocity;
	vec3 angularVelocity;

	vec3 forceAccumulator;
	vec3 torqueAccumulator;

	uint16 globalStateIndex;
};

struct physics_properties
{
	mat3 inertia;
	vec3 cog;
	float mass;
};

struct collider_properties
{
	float restitution;
	float friction;
	float density;
};

enum collider_type : uint16
{
	// The order here is important. See collision_narrow.cpp.
	collider_type_sphere,
	collider_type_capsule,
	collider_type_box,
};

static const char* colliderTypeNames[] =
{
	"Sphere",
	"Capsule",
	"Box",
};

struct collider_union
{
	collider_union() {}
	physics_properties calculatePhysicsProperties();

	collider_type type;
	uint16 rigidBodyIndex;

	union
	{
		bounding_sphere sphere;
		bounding_capsule capsule;
		bounding_box box;
	};

	collider_properties properties;
};

struct rigid_body_global_state
{
	quat rotation;
	vec3 position;
	vec3 linearVelocity;
	vec3 angularVelocity;
	mat3 invInertia;
	float invMass;
};

struct collider_component : collider_union
{
	collider_component(bounding_sphere s, float restitution, float friction, float density)
	{
		sphere = s;
		initialize(collider_type_sphere, restitution, friction, density);
	}
	collider_component(bounding_capsule c, float restitution, float friction, float density)
	{
		capsule = c;
		initialize(collider_type_capsule, restitution, friction, density);
	}
	collider_component(bounding_box b, float restitution, float friction, float density)
	{
		box = b;
		initialize(collider_type_box, restitution, friction, density);
	}

	void initialize(collider_type type, float restitution, float friction, float density)
	{
		this->type = type;
		this->properties.restitution = restitution;
		this->properties.friction = friction;
		this->properties.density = density;
	}

	entt::entity parentEntity;
	entt::entity nextEntity;
};

struct physics_reference_component
{
	uint32 numColliders = 0;
	entt::entity firstColliderEntity = entt::null;

	uint16 firstConstraintEdge = INVALID_CONSTRAINT_EDGE;
};

void addDistanceConstraint(scene_entity& a, scene_entity& b, vec3 localAnchorA, vec3 localAnchorB, float distance);

void testPhysicsInteraction(scene& appScene, ray r);
void physicsStep(scene& appScene, float dt, uint32 numSolverIterations = 30);
