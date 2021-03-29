#include "pch.h"
#include "physics.h"


struct rigid_body_update
{
	quat globalRotation;
	vec3 globalPosition;
	mat3 globalInvInertia;
};

void physicsStep(scene& appScene, float dt)
{
	std::vector<rigid_body_update> rbUpdate;
	rbUpdate.reserve(appScene.numberOfComponentsOfType<rigid_body_component>());

	// Apply gravity and air drag and integrate forces.
	for (auto [entityHandle, rb, transform] : appScene.group(entt::get<rigid_body_component, trs>).each())
	{
		rb.forceAccumulator.y += (GRAVITY / rb.invMass * rb.gravityFactor);

		// Air resistance.
		const float airDensity = 1.2f;
		const float dragCoefficient = 0.2f;
		float sqLinearVelocityLength = squaredLength(rb.linearVelocity);

		const float area = 2.f; // TODO!
		if (sqLinearVelocityLength > 0.f)
		{
			rb.forceAccumulator -= 0.5f * airDensity * dragCoefficient * area * sqLinearVelocityLength * normalize(rb.linearVelocity);
		}

		// TODO: Apply air resistance to angular velocity.

		uint16 updateIndex = (uint16)rbUpdate.size();
		auto& update = rbUpdate.emplace_back();
		update.globalRotation = transform.rotation;
		update.globalPosition = transform.position + transform.rotation * rb.localCOGPosition;
		
		mat3 rot = quaternionToMat3(update.globalRotation);
		update.globalInvInertia = rot * rb.invInertia * transpose(rot);

		vec3 linearAcceleration = rb.forceAccumulator * rb.invMass;
		vec3 angularAcceleration = update.globalInvInertia * rb.torqueAccumulator;

		rb.linearVelocity += linearAcceleration * dt;
		rb.angularVelocity += angularAcceleration * dt;
		rb.updateIndex = updateIndex;
	}

	// TODO: Handle constraints.



	// Integrate velocities.
	for (auto [entityHandle, rb, transform] : appScene.group(entt::get<rigid_body_component, trs>).each())
	{
		auto& update = rbUpdate[rb.updateIndex];

		quat deltaRot(0.5f * rb.angularVelocity.x, 0.5f * rb.angularVelocity.y, 0.5f * rb.angularVelocity.z, 0.f);
		deltaRot = deltaRot * update.globalRotation;

		update.globalRotation = normalize(update.globalRotation + (deltaRot * dt));
		update.globalPosition += rb.linearVelocity * dt;

		rb.forceAccumulator = vec3(0.f, 0.f, 0.f);
		rb.torqueAccumulator = vec3(0.f, 0.f, 0.f);

		transform.rotation = update.globalRotation;
		transform.position = update.globalPosition - update.globalRotation * rb.localCOGPosition;
	}

}

rigid_body_component::rigid_body_component(bool kinematic, float gravityFactor)
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

// This function returns the inertia tensors with respect to the center of gravity, so with a coordinate system centered at the COG.
physics_properties collider_component::calculatePhysicsProperties()
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
