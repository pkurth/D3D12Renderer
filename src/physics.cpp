#include "pch.h"
#include "physics.h"
#include "collision_broad.h"
#include "collision_narrow.h"



void testPhysicsInteraction(scene& appScene, ray r)
{
	float minT = FLT_MAX;
	rigid_body_component* minRB = 0;
	vec3 force;
	vec3 torque;

	for (auto [entityHandle, collider] : appScene.view<collider_component>().each())
	{
		scene_entity rbEntity = { collider.parentEntity, appScene };
		if (rbEntity.hasComponent<rigid_body_component>())
		{
			rigid_body_component& rb = rbEntity.getComponent<rigid_body_component>();
			trs& transform = rbEntity.getComponent<trs>();

			ray localR = { inverseTransformPosition(transform, r.origin), inverseTransformDirection(transform, r.direction) };
			float t;
			bool hit = false;

			switch (collider.type)
			{
				case collider_type_sphere:
				{
					hit = localR.intersectSphere(collider.sphere, t);
				} break;

				case collider_type_capsule:
				{
					hit = localR.intersectCapsule(collider.capsule, t);
				} break;

				case collider_type_box:
				{
					hit = localR.intersectAABB(collider.box, t);
				} break;
			}

			if (hit && t < minT)
			{
				minT = t;
				minRB = &rb;

				vec3 localHit = localR.origin + t * localR.direction;
				vec3 globalHit = transformPosition(transform, localHit);

				vec3 cogPosition = rb.getGlobalCOGPosition(transform);

				force = r.direction * 300.f;
				torque = cross(globalHit - cogPosition, force);
			}
		}
	}

	if (minRB)
	{
		minRB->torqueAccumulator += torque;
		minRB->forceAccumulator += force;
	}
}

static void getWorldSpaceColliders(scene& appScene, bounding_box* outWorldspaceAABBs, collider_union* outWorldSpaceColliders)
{
	uint32 pushIndex = 0;

	auto rbView = appScene.view<rigid_body_component>();
	rigid_body_component* rbBase = rbView.raw();

	for (auto [entityHandle, collider] : appScene.view<collider_component>().each())
	{
		bounding_box& bb = outWorldspaceAABBs[pushIndex];
		collider_union& col = outWorldSpaceColliders[pushIndex];
		++pushIndex;

		scene_entity entity = { collider.parentEntity, appScene };
		trs& transform = entity.getComponent<trs>();

		rigid_body_component& rb = entity.getComponent<rigid_body_component>();

		col.type = collider.type;
		col.properties = collider.properties;
		col.rigidBodyIndex = (uint16)(&rb - rbBase);

		switch (collider.type)
		{
			case collider_type_sphere:
			{
				vec3 center = transform.position + transform.rotation * collider.sphere.center;
				bb = bounding_box::fromCenterRadius(center, collider.sphere.radius);
				col.sphere = { center, collider.sphere.radius };
			} break;

			case collider_type_capsule:
			{
				vec3 posA = transform.rotation * collider.capsule.positionA + transform.position;
				vec3 posB = transform.rotation * collider.capsule.positionB + transform.position;

				float radius = collider.capsule.radius;
				vec3 radius3(radius);

				bb = bounding_box::negativeInfinity();
				bb.grow(posA + radius3);
				bb.grow(posA - radius3);
				bb.grow(posB + radius3);
				bb.grow(posB - radius3);

				col.capsule = { posA, posB, radius };
			} break;

			case collider_type_box:
			{
				bb = bounding_box::negativeInfinity();
				bb.grow(transform.rotation * collider.box.minCorner + transform.position);
				bb.grow(transform.rotation * vec3(collider.box.maxCorner.x, collider.box.minCorner.y, collider.box.minCorner.z) + transform.position);
				bb.grow(transform.rotation * vec3(collider.box.minCorner.x, collider.box.maxCorner.y, collider.box.minCorner.z) + transform.position);
				bb.grow(transform.rotation * vec3(collider.box.maxCorner.x, collider.box.maxCorner.y, collider.box.minCorner.z) + transform.position);
				bb.grow(transform.rotation * vec3(collider.box.minCorner.x, collider.box.minCorner.y, collider.box.maxCorner.z) + transform.position);
				bb.grow(transform.rotation * vec3(collider.box.maxCorner.x, collider.box.minCorner.y, collider.box.maxCorner.z) + transform.position);
				bb.grow(transform.rotation * vec3(collider.box.minCorner.x, collider.box.maxCorner.y, collider.box.maxCorner.z) + transform.position);
				bb.grow(transform.rotation * collider.box.maxCorner + transform.position);

				assert(transform.rotation == quat::identity);
				col.box = bb; // TODO: Output OBB here.
			} break;
		}
	}
}

void physicsStep(scene& appScene, float dt)
{
	// TODO:
	static void* scratchMemory = malloc(1024 * 1024);
	static broadphase_collision* possibleCollisions = new broadphase_collision[1024];
	static rigid_body_global_state* rbGlobal = new rigid_body_global_state[1024];
	static bounding_box* worldSpaceAABBs = new bounding_box[1024];
	static collider_union* worldSpaceColliders = new collider_union[1024];
	static collision_constraint* collisionConstraints = new collision_constraint[1024];


	uint16 rbIndex = 0;

	// Apply gravity and air drag and integrate forces.
	for (auto [entityHandle, rb, transform] : appScene.group(entt::get<rigid_body_component, trs>).each())
	{
		uint16 globalStateIndex = rbIndex++;
		auto& global = rbGlobal[globalStateIndex];
		global.rotation = transform.rotation;
		global.position = transform.position + transform.rotation * rb.localCOGPosition;

		mat3 rot = quaternionToMat3(global.rotation);
		global.invInertia = rot * rb.invInertia * transpose(rot);
		global.invMass = rb.invMass;


		if (rb.invMass > 0.f)
		{
			rb.forceAccumulator.y += (GRAVITY / rb.invMass * rb.gravityFactor);
		}

		vec3 linearAcceleration = rb.forceAccumulator * rb.invMass;
		vec3 angularAcceleration = global.invInertia * rb.torqueAccumulator;

		// Semi-implicit Euler integration.
		rb.linearVelocity += linearAcceleration * dt;
		rb.angularVelocity += angularAcceleration * dt;

		rb.linearVelocity *= 1.f / (1.f + dt * rb.linearDamping);
		rb.angularVelocity *= 1.f / (1.f + dt * rb.angularDamping);


		global.linearVelocity = rb.linearVelocity;
		global.angularVelocity = rb.angularVelocity;

		rb.globalStateIndex = globalStateIndex;
	}
	
	getWorldSpaceColliders(appScene, worldSpaceAABBs, worldSpaceColliders);
	uint32 numPossibleCollisions = broadphase(appScene, 0, worldSpaceAABBs, possibleCollisions, scratchMemory);
	uint32 numCollisionConstraints = narrowphase(worldSpaceColliders, rbGlobal, possibleCollisions, numPossibleCollisions, dt, collisionConstraints);

	if (numCollisionConstraints)
	{
		const uint32 numSolverIterations = 10;
		for (uint32 it = 0; it < numSolverIterations; ++it)
		{
			for (uint32 i = 0; i < numCollisionConstraints; ++i)
			{
				solveCollisionConstraint(collisionConstraints[i], rbGlobal);
			}
		}
	}


	// Integrate velocities.
	for (auto [entityHandle, rb, transform] : appScene.group(entt::get<rigid_body_component, trs>).each())
	{
		auto& global = rbGlobal[rb.globalStateIndex];

		rb.linearVelocity = global.linearVelocity;
		rb.angularVelocity = global.angularVelocity;

		quat deltaRot(0.5f * rb.angularVelocity.x, 0.5f * rb.angularVelocity.y, 0.5f * rb.angularVelocity.z, 0.f);
		deltaRot = deltaRot * global.rotation;

		global.rotation = normalize(global.rotation + (deltaRot * dt));
		global.position += rb.linearVelocity * dt;

		rb.forceAccumulator = vec3(0.f, 0.f, 0.f);
		rb.torqueAccumulator = vec3(0.f, 0.f, 0.f);

		transform.rotation = global.rotation;
		transform.position = global.position - global.rotation * rb.localCOGPosition;
	}

}

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

void rigid_body_component::recalculateProperties(const collider_reference_component& colliderReference)
{
	if (invMass == 0.f)
	{
		return; // Kinematic.
	}

	uint32 numColliders = colliderReference.numColliders;
	physics_properties* properties = (physics_properties*)alloca(numColliders * (sizeof(physics_properties)));

	entt::registry* registry = colliderReference.registry;

	uint32 i = 0;

	scene_entity colliderEntity = { colliderReference.firstColliderEntity, registry };
	while (colliderEntity)
	{
		collider_component& collider = colliderEntity.getComponent<collider_component>();
		properties[i++] = collider.calculatePhysicsProperties();
		colliderEntity = { collider.nextColliderEntity, registry };
	}

	assert(i == numColliders);

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

// This function returns the inertia tensors with respect to the center of gravity, so with a coordinate system centered at the COG.
physics_properties collider_union::calculatePhysicsProperties()
{
	physics_properties result;
	switch (type)
	{
		case collider_type_sphere:
		{
			result.mass = sphere.volume() * properties.density;
			result.cog = sphere.center;
			result.inertia = mat3::identity * (2.f / 5.f * result.mass * sphere.radius * sphere.radius);
		} break;

		case collider_type_capsule:
		{
			vec3 axis = capsule.positionA - capsule.positionB;
			if (axis.y < 0.f)
			{
				axis *= -1.f;
			}
			float height = length(axis);
			axis *= (1.f / height);

			quat rotation = rotateFromTo(vec3(0.f, 1.f, 0.f), axis);
			mat3 rot = quaternionToMat3(rotation);

			result.mass = capsule.volume() * properties.density;
			result.cog = (capsule.positionA + capsule.positionB) * 0.5f;
			
			// Inertia.
			float sqRadius = capsule.radius * capsule.radius;
			float sqRadiusPI = M_PI * sqRadius;

			float cylinderMass = properties.density * sqRadiusPI * height;
			float hemiSphereMass = properties.density * 2.f / 3.f * sqRadiusPI * capsule.radius;

			float sqCapsuleHeight = height * height;

			result.inertia.m11 = sqRadius * cylinderMass * 0.5f;
			result.inertia.m00 = result.inertia.m22 = result.inertia.m11 * 0.5f + cylinderMass * sqCapsuleHeight / 12.f;
			float temp0 = hemiSphereMass * 2.f * sqRadius / 5.f;
			result.inertia.m11 += temp0 * 2.f;
			float temp1 = height * 0.5f;
			float temp2 = temp0 + hemiSphereMass * (temp1 * temp1 + 3.f / 8.f * sqCapsuleHeight);
			result.inertia.m00 += temp2 * 2.f;
			result.inertia.m22 += temp2 * 2.f;
			result.inertia.m01 = result.inertia.m02 = result.inertia.m10 = result.inertia.m12 = result.inertia.m20 = result.inertia.m21 = 0.f;

			result.inertia = rot * result.inertia;
		} break;

		case collider_type_box:
		{
			result.mass = box.volume() * properties.density;
			result.cog = box.getCenter();

			vec3 diameter = box.getRadius() * 2.f;
			result.inertia = mat3::zero;
			result.inertia.m00 = 1.f / 12.f * result.mass * (diameter.y * diameter.y + diameter.z * diameter.z);
			result.inertia.m11 = 1.f / 12.f * result.mass * (diameter.x * diameter.x + diameter.z * diameter.z);
			result.inertia.m22 = 1.f / 12.f * result.mass * (diameter.x * diameter.x + diameter.y * diameter.y);
		} break;
	}
	return result;
}
