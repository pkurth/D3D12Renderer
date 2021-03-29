#pragma once

#include "math.h"
#include "bounding_volumes.h"
#include "scene.h"

#define GRAVITY -9.81f

struct rigid_body_component
{
	rigid_body_component(bool kinematic, float gravityFactor = 1.f);
	void recalculateProperties(const struct collider_reference_component& colliderReference);
	vec3 getGlobalCOGPosition(const trs& transform) const;

	// In entity's local space.
	vec3 localCOGPosition;
	float invMass;
	mat3 invInertia;

	float gravityFactor;

	// In global space.
	vec3 linearVelocity;
	vec3 angularVelocity;

	vec3 forceAccumulator;
	vec3 torqueAccumulator;

	uint16 updateIndex;
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

enum collider_type
{
	collider_type_sphere,
	collider_type_capsule,
	collider_type_box,
};

struct collider_component
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

	physics_properties calculatePhysicsProperties();

	collider_type type;

	union
	{
		bounding_sphere sphere;
		bounding_capsule capsule;
		bounding_box box;
	};

	collider_properties properties;

	entt::entity parentEntity;
	entt::entity nextColliderEntity;
};

struct collider_reference_component
{
	entt::registry* registry;
	uint32 numColliders = 0;
	entt::entity firstColliderEntity = entt::null;
};

void testPhysicsInteraction(scene& appScene, ray r);
void physicsStep(scene& appScene, float dt);
