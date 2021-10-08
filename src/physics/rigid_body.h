#pragma once

#include "core/math.h"
#include "scene/scene.h"

struct rigid_body_global_state
{
	// Don't change the order here. It's currently required by the SIMD code.
	quat rotation;
	vec3 position;
	vec3 linearVelocity;
	vec3 angularVelocity;
	float invMass;
	mat3 invInertia;
	vec3 localCOGPosition;
};

struct rigid_body_component
{
	rigid_body_component() : rigid_body_component(true, 1.f) {}
	rigid_body_component(bool kinematic, float gravityFactor = 1.f, float linearDamping = 0.4f, float angularDamping = 0.4f);
	void recalculateProperties(entt::registry* registry, const struct physics_reference_component& reference);
	vec3 getGlobalCOGPosition(const trs& transform) const;
	vec3 getGlobalPointVelocity(const trs& transform, vec3 localP) const;


	void applyGravityAndIntegrateForces(rigid_body_global_state& global, const trs& transform, float dt);
	void integrateVelocity(const rigid_body_global_state& global, trs& transform, float dt);


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
};
