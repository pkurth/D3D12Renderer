#include "pch.h"
#include "physics.h"
#include "collision_broad.h"
#include "collision_narrow.h"
#include "geometry.h"
#include "assimp.h"


static std::vector<distance_constraint> distanceConstraints;
static std::vector<ball_joint_constraint> ballJointConstraints;
static std::vector<hinge_joint_constraint> hingeJointConstraints;
static std::vector<cone_twist_constraint> coneTwistConstraints;

static std::vector<constraint_edge> constraintEdges;


static std::vector<bounding_hull_geometry> boundingHullGeometries;

uint32 allocateBoundingHullGeometry(const cpu_mesh& mesh)
{
	uint32 index = (uint32)boundingHullGeometries.size();
	boundingHullGeometries.push_back(boundingHullFromMesh((vec3*)mesh.vertexPositions, mesh.numVertices, mesh.triangles, mesh.numTriangles));
	return index;
}

uint32 allocateBoundingHullGeometry(const std::string& meshFilepath)
{
	Assimp::Importer importer;
	importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, aiComponent_TEXCOORDS | aiComponent_NORMALS | aiComponent_TANGENTS_AND_BITANGENTS | aiComponent_COLORS);

	const aiScene* scene = loadAssimpSceneFile(meshFilepath, importer);

	if (!scene)
	{
		return INVALID_BOUNDING_HULL_INDEX;
	}

	cpu_mesh cpuMesh(mesh_creation_flags_with_positions);

	assert(scene->mNumMeshes == 1);
	cpuMesh.pushAssimpMesh(scene->mMeshes[0], 1.f);

	return allocateBoundingHullGeometry(cpuMesh);
}

static void addConstraintEdge(scene_entity& e, physics_constraint& constraint, uint16 constraintIndex, constraint_type type)
{
	if (!e.hasComponent<physics_reference_component>())
	{
		e.addComponent<physics_reference_component>();
	}

	physics_reference_component& reference = e.getComponent<physics_reference_component>();

	uint16 edgeIndex = (uint16)constraintEdges.size();
	constraint_edge& edge = constraintEdges.emplace_back();
	edge.constraint = constraintIndex;
	edge.type = type;
	edge.prevConstraintEdge = INVALID_CONSTRAINT_EDGE;
	edge.nextConstraintEdge = reference.firstConstraintEdge;

	if (reference.firstConstraintEdge != INVALID_CONSTRAINT_EDGE)
	{
		constraintEdges[reference.firstConstraintEdge].prevConstraintEdge = edgeIndex;
	}

	reference.firstConstraintEdge = edgeIndex;

	if (constraint.edgeA == INVALID_CONSTRAINT_EDGE)
	{
		constraint.edgeA = edgeIndex;
		constraint.entityA = e.handle;
	}
	else
	{
		assert(constraint.edgeB == INVALID_CONSTRAINT_EDGE);
		constraint.edgeB = INVALID_CONSTRAINT_EDGE;
		constraint.entityB = e.handle;
	}
}

distance_constraint_handle addDistanceConstraintFromLocalPoints(scene_entity& a, scene_entity& b, vec3 localAnchorA, vec3 localAnchorB, float distance)
{
	uint16 constraintIndex = (uint16)distanceConstraints.size();
	distance_constraint& constraint = distanceConstraints.emplace_back();
	constraint.localAnchorA = localAnchorA;
	constraint.localAnchorB = localAnchorB;
	constraint.globalLength = distance;

	addConstraintEdge(a, constraint, constraintIndex, constraint_type_distance);
	addConstraintEdge(b, constraint, constraintIndex, constraint_type_distance);

	return { constraintIndex };
}

distance_constraint_handle addDistanceConstraintFromGlobalPoints(scene_entity& a, scene_entity& b, vec3 globalAnchorA, vec3 globalAnchorB)
{
	vec3 localAnchorA = inverseTransformPosition(a.getComponent<trs>(), globalAnchorA);
	vec3 localAnchorB = inverseTransformPosition(b.getComponent<trs>(), globalAnchorB);
	float distance = length(globalAnchorA - globalAnchorB);

	return addDistanceConstraintFromLocalPoints(a, b, localAnchorA, localAnchorB, distance);
}

ball_joint_constraint_handle addBallJointConstraintFromLocalPoints(scene_entity& a, scene_entity& b, vec3 localAnchorA, vec3 localAnchorB)
{
	uint16 constraintIndex = (uint16)ballJointConstraints.size();
	ball_joint_constraint& constraint = ballJointConstraints.emplace_back();
	constraint.localAnchorA = localAnchorA;
	constraint.localAnchorB = localAnchorB;

	addConstraintEdge(a, constraint, constraintIndex, constraint_type_ball_joint);
	addConstraintEdge(b, constraint, constraintIndex, constraint_type_ball_joint);

	return { constraintIndex };
}

ball_joint_constraint_handle addBallJointConstraintFromGlobalPoints(scene_entity& a, scene_entity& b, vec3 globalAnchor)
{
	vec3 localAnchorA = inverseTransformPosition(a.getComponent<trs>(), globalAnchor);
	vec3 localAnchorB = inverseTransformPosition(b.getComponent<trs>(), globalAnchor);

	return addBallJointConstraintFromLocalPoints(a, b, localAnchorA, localAnchorB);
}

hinge_joint_constraint_handle addHingeJointConstraintFromGlobalPoints(scene_entity& a, scene_entity& b, vec3 globalAnchor, vec3 globalHingeAxis,
	float minLimit, float maxLimit)
{
	uint16 constraintIndex = (uint16)hingeJointConstraints.size();
	hinge_joint_constraint& constraint = hingeJointConstraints.emplace_back();

	const trs& transformA = a.getComponent<trs>();
	const trs& transformB = b.getComponent<trs>();

	constraint.localAnchorA = inverseTransformPosition(transformA, globalAnchor);
	constraint.localAnchorB = inverseTransformPosition(transformB, globalAnchor);
	constraint.localHingeAxisA = inverseTransformDirection(transformA, globalHingeAxis);
	constraint.localHingeAxisB = inverseTransformDirection(transformB, globalHingeAxis);

	getTangents(constraint.localHingeAxisA, constraint.localHingeTangentA, constraint.localHingeBitangentA);
	constraint.localHingeTangentB = conjugate(transformB.rotation) * (transformA.rotation * constraint.localHingeTangentA);

	// Limits.
	constraint.minRotationLimit = minLimit;
	constraint.maxRotationLimit = maxLimit;

	// Motor.
	constraint.motorType = constraint_velocity_motor;
	constraint.motorVelocity = 0.f;
	constraint.maxMotorTorque = -1.f; // Disabled by default.

	addConstraintEdge(a, constraint, constraintIndex, constraint_type_hinge_joint);
	addConstraintEdge(b, constraint, constraintIndex, constraint_type_hinge_joint);

	return { constraintIndex };
}

cone_twist_constraint_handle addConeTwistConstraintFromGlobalPoints(scene_entity& a, scene_entity& b, vec3 globalAnchor, vec3 globalAxis, 
	float swingLimit, float twistLimit)
{
	uint16 constraintIndex = (uint16)coneTwistConstraints.size();
	cone_twist_constraint& constraint = coneTwistConstraints.emplace_back();

	const trs& transformA = a.getComponent<trs>();
	const trs& transformB = b.getComponent<trs>();

	constraint.localAnchorA = inverseTransformPosition(transformA, globalAnchor);
	constraint.localAnchorB = inverseTransformPosition(transformB, globalAnchor);

	// Limits.
	constraint.swingLimit = swingLimit;
	constraint.twistLimit = twistLimit;
	constraint.localLimitAxisA = inverseTransformDirection(transformA, globalAxis);
	constraint.localLimitAxisB = inverseTransformDirection(transformB, globalAxis);

	getTangents(constraint.localLimitAxisA, constraint.localLimitTangentA, constraint.localLimitBitangentA);
	constraint.localLimitTangentB = conjugate(transformB.rotation) * (transformA.rotation * constraint.localLimitTangentA);

	// Motor.
	constraint.twistMotorType = constraint_velocity_motor;
	constraint.twistMotorVelocity = 0.f;
	constraint.maxTwistMotorTorque = -1.f; // Disabled by default.

	addConstraintEdge(a, constraint, constraintIndex, constraint_type_cone_twist);
	addConstraintEdge(b, constraint, constraintIndex, constraint_type_cone_twist);

	return { constraintIndex };
}

distance_constraint& getConstraint(distance_constraint_handle handle)
{
	assert(handle.index < distanceConstraints.size());
	return distanceConstraints[handle.index];
}

ball_joint_constraint& getConstraint(ball_joint_constraint_handle handle)
{
	assert(handle.index < ballJointConstraints.size());
	return ballJointConstraints[handle.index];
}

hinge_joint_constraint& getConstraint(hinge_joint_constraint_handle handle)
{
	assert(handle.index < hingeJointConstraints.size());
	return hingeJointConstraints[handle.index];
}

cone_twist_constraint& getConstraint(cone_twist_constraint_handle handle)
{
	assert(handle.index < coneTwistConstraints.size());
	return coneTwistConstraints[handle.index];
}


void testPhysicsInteraction(scene& appScene, ray r, float forceAmount)
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

				case collider_type_aabb:
				{
					hit = localR.intersectAABB(collider.aabb, t);
				} break;

				case collider_type_obb:
				{
					hit = localR.intersectOBB(collider.obb, t);
				} break;

				case collider_type_hull:
				{
					hit = localR.intersectHull(collider.hull, boundingHullGeometries[collider.hull.geometryIndex], t);
				} break;
			}

			if (hit && t < minT)
			{
				minT = t;
				minRB = &rb;

				vec3 localHit = localR.origin + t * localR.direction;
				vec3 globalHit = transformPosition(transform, localHit);

				vec3 cogPosition = rb.getGlobalCOGPosition(transform);

				force = r.direction * forceAmount;
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

	auto colliderView = appScene.view<collider_component>();

	for (auto [entityHandle, collider] : colliderView.each())
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

			case collider_type_aabb:
			{
				bb = collider.aabb.transformToAABB(transform.rotation, transform.position);
				if (transform.rotation == quat::identity)
				{
					col.aabb = bb;
				}
				else
				{
					col.type = collider_type_obb;
					col.obb = collider.aabb.transformToOBB(transform.rotation, transform.position);
				}
			} break;

			case collider_type_obb:
			{
				bb = collider.obb.transformToAABB(transform.rotation, transform.position);
				col.obb = collider.obb.transformToOBB(transform.rotation, transform.position);
			} break;

			case collider_type_hull:
			{
				const bounding_hull_geometry& geometry = boundingHullGeometries[collider.hull.geometryIndex];

				quat rotation = transform.rotation * collider.hull.rotation;
				vec3 position = transform.rotation * collider.hull.position + transform.position;

				bb = geometry.aabb.transformToAABB(rotation, position);
				col.hull.rotation = rotation;
				col.hull.position = position;
				col.hull.geometryPtr = &geometry;
			} break;
		}
	}
}

void physicsStep(scene& appScene, float dt, uint32 numSolverIterations)
{
	// TODO:
	static void* scratchMemory = malloc(1024 * 1024);
	static broadphase_collision* possibleCollisions = new broadphase_collision[10000];
	static rigid_body_global_state* rbGlobal = new rigid_body_global_state[1024];
	static bounding_box* worldSpaceAABBs = new bounding_box[1024];
	static collider_union* worldSpaceColliders = new collider_union[1024];
	static collision_constraint* collisionConstraints = new collision_constraint[10000];
	
	static distance_constraint_update* distanceConstraintUpdates = new distance_constraint_update[1024];
	static ball_joint_constraint_update* ballJointConstraintUpdates = new ball_joint_constraint_update[1024];
	static hinge_joint_constraint_update* hingeJointConstraintUpdates = new hinge_joint_constraint_update[1024];
	static cone_twist_constraint_update* coneTwistConstraintUpdates = new cone_twist_constraint_update[1024];

	  
	auto rbView = appScene.view<rigid_body_component>();
	rigid_body_component* rbBase = rbView.raw();

	// Apply gravity and air drag and integrate forces.
	for (auto [entityHandle, rb, transform] : appScene.group(entt::get<rigid_body_component, trs>).each())
	{
		uint16 globalStateIndex = (uint16)(&rb - rbBase);
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
	
	initializeDistanceVelocityConstraints(appScene, rbGlobal, distanceConstraints.data(), distanceConstraintUpdates, (uint32)distanceConstraints.size(), dt);
	initializeBallJointVelocityConstraints(appScene, rbGlobal, ballJointConstraints.data(), ballJointConstraintUpdates, (uint32)ballJointConstraints.size(), dt);
	initializeHingeJointVelocityConstraints(appScene, rbGlobal, hingeJointConstraints.data(), hingeJointConstraintUpdates, (uint32)hingeJointConstraints.size(), dt);
	initializeConeTwistVelocityConstraints(appScene, rbGlobal, coneTwistConstraints.data(), coneTwistConstraintUpdates, (uint32)coneTwistConstraints.size(), dt);

	for (uint32 it = 0; it < numSolverIterations; ++it)
	{
		solveDistanceVelocityConstraints(distanceConstraintUpdates, (uint32)distanceConstraints.size(), rbGlobal);
		solveBallJointVelocityConstraints(ballJointConstraintUpdates, (uint32)ballJointConstraints.size(), rbGlobal);
		solveHingeJointVelocityConstraints(hingeJointConstraintUpdates, (uint32)hingeJointConstraints.size(), rbGlobal);
		solveConeTwistVelocityConstraints(coneTwistConstraintUpdates, (uint32)coneTwistConstraints.size(), rbGlobal);
		solveCollisionVelocityConstraints(collisionConstraints, numCollisionConstraints, rbGlobal);
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

			result.inertia = transpose(rot) * result.inertia * rot;
		} break;

		case collider_type_aabb:
		{
			result.mass = aabb.volume() * properties.density;
			result.cog = aabb.getCenter();

			vec3 diameter = aabb.getRadius() * 2.f;
			result.inertia = mat3::zero;
			result.inertia.m00 = 1.f / 12.f * result.mass * (diameter.y * diameter.y + diameter.z * diameter.z);
			result.inertia.m11 = 1.f / 12.f * result.mass * (diameter.x * diameter.x + diameter.z * diameter.z);
			result.inertia.m22 = 1.f / 12.f * result.mass * (diameter.x * diameter.x + diameter.y * diameter.y);
		} break;

		case collider_type_obb:
		{
			result.mass = obb.volume() * properties.density;
			result.cog = obb.center;

			vec3 diameter = obb.radius * 2.f;
			result.inertia = mat3::zero;
			result.inertia.m00 = 1.f / 12.f * result.mass * (diameter.y * diameter.y + diameter.z * diameter.z);
			result.inertia.m11 = 1.f / 12.f * result.mass * (diameter.x * diameter.x + diameter.z * diameter.z);
			result.inertia.m22 = 1.f / 12.f * result.mass * (diameter.x * diameter.x + diameter.y * diameter.y);

			mat3 rot = quaternionToMat3(obb.rotation);
			result.inertia = transpose(rot) * result.inertia * rot;
		} break;

		case collider_type_hull:
		{
			// http://number-none.com/blow/inertia/
			// http://number-none.com/blow/inertia/bb_inertia.doc

			const bounding_hull_geometry& geom = boundingHullGeometries[hull.geometryIndex];

			const float s60 = 1.f / 60.f;
			const float s120 = 1.f / 120.f;

			// Covariance-matrix of a tetrahedron with v0 = (0, 0, 0), v1 = (1, 0, 0), v2 = (0, 1, 0), v3 = (0, 0, 1).
			const mat3 Ccanonical(
				s60, s120, s120,
				s120, s60, s120,
				s120, s120, s60
			);

			uint32 numFaces = (uint32)geom.faces.size();

			float totalMass = 0.f;
			mat3 totalCovariance = mat3::zero;
			vec3 totalCOG(0.f);

			for (uint32 i = 0; i < numFaces; ++i)
			{
				auto& face = geom.faces[i];

				//vec3 w0 = 0.f;
				vec3 w1 = hull.position + hull.rotation * geom.vertices[face.a];
				vec3 w2 = hull.position + hull.rotation * geom.vertices[face.b];
				vec3 w3 = hull.position + hull.rotation * geom.vertices[face.c];

				mat3 A(
					w1.x, w2.x, w3.x,
					w1.y, w2.y, w3.y,
					w1.z, w2.z, w3.z
				);


				float detA = determinant(A);
				mat3 covariance = detA * A * Ccanonical * transpose(A);

				float volume = 1.f / 6.f * detA;
				float mass = volume;
				vec3 cog = (w1 + w2 + w3) * 0.25f;

				totalMass += mass;
				totalCovariance += covariance;
				totalCOG += cog * mass;
			}

			totalCOG /= totalMass;

			// This is actually different in the Blow-paper, but this one is correct.
			mat3 CprimeTotal = totalCovariance - totalMass * outerProduct(totalCOG, totalCOG);

			result.cog = totalCOG;
			result.mass = totalMass * properties.density;
			result.inertia = mat3::identity * trace(CprimeTotal) - CprimeTotal;
			result.inertia *= properties.density;
		} break;

		default:
		{
			assert(false);
		} break;
	}
	return result;
}
