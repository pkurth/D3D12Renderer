#include "pch.h"
#include "rigid_body.h"
#include "physics.h"


rigid_body_component::rigid_body_component(bool kinematic, float gravityFactor, float linearDamping, float angularDamping)
{
	if (kinematic)
	{
		invMass = 0.f;
		invInertia = mat3::zero;
	}
	else
	{
		invMass = 1.f;
		invInertia = mat3::identity;
	}

	this->gravityFactor = gravityFactor;
	this->linearDamping = linearDamping;
	this->angularDamping = angularDamping;
	this->localCOGPosition = vec3(0.f);
	this->linearVelocity = vec3(0.f);
	this->angularVelocity = vec3(0.f);
	this->forceAccumulator = vec3(0.f);
	this->torqueAccumulator = vec3(0.f);
}

void rigid_body_component::recalculateProperties(entt::registry* registry, const physics_reference_component& reference)
{
	if (invMass == 0.f)
	{
		return; // Kinematic.
	}

	uint32 numColliders = reference.numColliders;
	if (!numColliders)
	{
		return;
	}

	physics_properties* properties = (physics_properties*)alloca(numColliders * (sizeof(physics_properties)));

	uint32 i = 0;

	scene_entity colliderEntity = { reference.firstColliderEntity, registry };
	while (colliderEntity)
	{
		collider_component& collider = colliderEntity.getComponent<collider_component>();
		properties[i++] = collider.calculatePhysicsProperties();
		colliderEntity = { collider.nextEntity, registry };
	}

	ASSERT(i == numColliders);

	mat3 inertia = mat3::zero;
	vec3 cog(0.f);
	float mass = 0.f;

	for (uint32 i = 0; i < numColliders; ++i)
	{
		mass += properties[i].mass;
		cog += properties[i].cog * properties[i].mass;
	}

	invMass = 1.f / mass;
	localCOGPosition = cog = cog * invMass; // TODO: Update linear velocity, since thats given at the COG.

	// Combine inertia tensors: https://pybullet.org/Bullet/phpBB3/viewtopic.php?t=246
	// This assumes that all shapes have the same orientation, which is true in our case, since all shapes are given 
	// in the entity's local coordinate system.
	for (uint32 i = 0; i < numColliders; ++i)
	{
		vec3& localCOG = properties[i].cog;
		mat3& localInertia = properties[i].inertia;
		vec3 r = localCOG - cog;
		inertia += localInertia + (dot(r, r) * mat3::identity - outerProduct(r, r)) * properties[i].mass;
	}

	invInertia = invert(inertia);
}

vec3 rigid_body_component::getGlobalCOGPosition(const trs& transform) const
{
	return transform.position + transform.rotation * localCOGPosition;
}

vec3 rigid_body_component::getGlobalPointVelocity(const trs& transform, vec3 localP) const
{
	vec3 globalP = transformPosition(transform, localP);
	vec3 globalCOG = getGlobalCOGPosition(transform);
	return linearVelocity + cross(angularVelocity, globalP - globalCOG);
}

void rigid_body_component::applyGravityAndIntegrateForces(rigid_body_global_state& global, const trs& transform, float dt)
{
	global.rotation = transform.rotation;
	global.position = transform.position + transform.rotation * localCOGPosition;

	mat3 rot = quaternionToMat3(global.rotation);
	global.invInertia = rot * invInertia * transpose(rot);
	global.invMass = invMass;


	if (invMass > 0.f)
	{
		forceAccumulator.y += (GRAVITY / invMass * gravityFactor);
	}

	vec3 linearAcceleration = forceAccumulator * invMass;
	vec3 angularAcceleration = global.invInertia * torqueAccumulator;

	// Semi-implicit Euler integration.
	linearVelocity += linearAcceleration * dt;
	angularVelocity += angularAcceleration * dt;

	linearVelocity *= 1.f / (1.f + dt * linearDamping);
	angularVelocity *= 1.f / (1.f + dt * angularDamping);


	global.linearVelocity = linearVelocity;
	global.angularVelocity = angularVelocity;
	global.localCOGPosition = localCOGPosition;
}

void rigid_body_component::integrateVelocity(const rigid_body_global_state& global, trs& transform, float dt)
{
	linearVelocity = global.linearVelocity;
	angularVelocity = global.angularVelocity;

	quat deltaRot(0.5f * angularVelocity.x, 0.5f * angularVelocity.y, 0.5f * angularVelocity.z, 0.f);
	deltaRot = deltaRot * global.rotation;

	quat rotation = normalize(global.rotation + (deltaRot * dt));
	vec3 position = global.position + linearVelocity * dt;

	forceAccumulator = vec3(0.f, 0.f, 0.f);
	torqueAccumulator = vec3(0.f, 0.f, 0.f);

	transform.rotation = rotation;
	transform.position = position - rotation * localCOGPosition;
}
