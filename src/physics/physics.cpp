#include "pch.h"
#include "physics.h"
#include "collision_broad.h"
#include "collision_narrow.h"
#include "geometry/geometry.h"
#include "core/cpu_profiling.h"


extern physics_settings physicsSettings = {};
static std::vector<bounding_hull_geometry> boundingHullGeometries;

struct constraint_context
{
	std::vector<constraint_edge> constraintEdges;
	uint16 firstFreeConstraintEdge = INVALID_CONSTRAINT_EDGE; // Free-list in constraintEdges array.


	constraint_edge& getFreeConstraintEdge()
	{
		if (firstFreeConstraintEdge == INVALID_CONSTRAINT_EDGE)
		{
			firstFreeConstraintEdge = (uint16)constraintEdges.size();
			constraintEdges.push_back(constraint_edge{ entt::null, constraint_type_none, INVALID_CONSTRAINT_EDGE, INVALID_CONSTRAINT_EDGE });
		}

		constraint_edge& edge = constraintEdges[firstFreeConstraintEdge];

		firstFreeConstraintEdge = edge.nextConstraintEdge;

		// Edge will be initialized by caller.

		return edge;
	}

	void freeConstraintEdge(constraint_edge& edge)
	{
		uint16 index = (uint16)(&edge - constraintEdges.data());
		edge.nextConstraintEdge = firstFreeConstraintEdge;
		firstFreeConstraintEdge = index;
	}
};

struct force_field_global_state
{
	vec3 force;
};

#ifndef PHYSICS_ONLY
// This is a bit dirty. PHYSICS_ONLY is defined when building the learning DLL, where we don't need bounding hulls.

#include "core/assimp.h"

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


	uint32 index = (uint32)boundingHullGeometries.size();
	boundingHullGeometries.push_back(bounding_hull_geometry::fromMesh(cpuMesh.vertexPositions, cpuMesh.numVertices, cpuMesh.triangles, cpuMesh.numTriangles));
	return index;
}
#endif

static void addConstraintEdge(scene_entity& e, constraint_entity_reference_component& constraintEntityReference, entt::entity constraintEntity, constraint_type type)
{
	if (!e.hasComponent<physics_reference_component>())
	{
		e.addComponent<physics_reference_component>();
	}

	physics_reference_component& reference = e.getComponent<physics_reference_component>();

	constraint_context& context = e.registry->ctx_or_set<constraint_context>();

	constraint_edge& edge = context.getFreeConstraintEdge();
	uint16 edgeIndex = (uint16)(&edge - context.constraintEdges.data());

	edge.constraintEntity = constraintEntity;
	edge.type = type;
	edge.prevConstraintEdge = INVALID_CONSTRAINT_EDGE;
	edge.nextConstraintEdge = reference.firstConstraintEdge;

	if (reference.firstConstraintEdge != INVALID_CONSTRAINT_EDGE)
	{
		context.constraintEdges[reference.firstConstraintEdge].prevConstraintEdge = edgeIndex;
	}

	reference.firstConstraintEdge = edgeIndex;

	if (constraintEntityReference.edgeA == INVALID_CONSTRAINT_EDGE)
	{
		constraintEntityReference.edgeA = edgeIndex;
		constraintEntityReference.entityA = e.handle;
	}
	else
	{
		assert(constraintEntityReference.edgeB == INVALID_CONSTRAINT_EDGE);
		constraintEntityReference.edgeB = edgeIndex;
		constraintEntityReference.entityB = e.handle;
	}
}

distance_constraint_handle addDistanceConstraintFromLocalPoints(scene_entity& a, scene_entity& b, vec3 localAnchorA, vec3 localAnchorB, float distance)
{
	assert(a.registry == b.registry);

	entt::registry& registry = *a.registry;
	entt::entity constraintEntity = registry.create();
	distance_constraint& constraint = registry.emplace<distance_constraint>(constraintEntity);
	constraint_entity_reference_component& ref = registry.emplace<constraint_entity_reference_component>(constraintEntity);

	constraint.localAnchorA = localAnchorA;
	constraint.localAnchorB = localAnchorB;
	constraint.globalLength = distance;

	addConstraintEdge(a, ref, constraintEntity, constraint_type_distance);
	addConstraintEdge(b, ref, constraintEntity, constraint_type_distance);

	return { constraintEntity };
}

distance_constraint_handle addDistanceConstraintFromGlobalPoints(scene_entity& a, scene_entity& b, vec3 globalAnchorA, vec3 globalAnchorB)
{
	assert(a.registry == b.registry);

	vec3 localAnchorA = inverseTransformPosition(a.getComponent<transform_component>(), globalAnchorA);
	vec3 localAnchorB = inverseTransformPosition(b.getComponent<transform_component>(), globalAnchorB);
	float distance = length(globalAnchorA - globalAnchorB);

	return addDistanceConstraintFromLocalPoints(a, b, localAnchorA, localAnchorB, distance);
}

ball_joint_constraint_handle addBallJointConstraintFromLocalPoints(scene_entity& a, scene_entity& b, vec3 localAnchorA, vec3 localAnchorB)
{
	assert(a.registry == b.registry);

	entt::registry& registry = *a.registry;
	entt::entity constraintEntity = registry.create();
	ball_joint_constraint& constraint = registry.emplace<ball_joint_constraint>(constraintEntity);
	constraint_entity_reference_component& ref = registry.emplace<constraint_entity_reference_component>(constraintEntity);

	constraint.localAnchorA = localAnchorA;
	constraint.localAnchorB = localAnchorB;

	addConstraintEdge(a, ref, constraintEntity, constraint_type_ball_joint);
	addConstraintEdge(b, ref, constraintEntity, constraint_type_ball_joint);

	return { constraintEntity };
}

ball_joint_constraint_handle addBallJointConstraintFromGlobalPoints(scene_entity& a, scene_entity& b, vec3 globalAnchor)
{
	assert(a.registry == b.registry);

	vec3 localAnchorA = inverseTransformPosition(a.getComponent<transform_component>(), globalAnchor);
	vec3 localAnchorB = inverseTransformPosition(b.getComponent<transform_component>(), globalAnchor);

	return addBallJointConstraintFromLocalPoints(a, b, localAnchorA, localAnchorB);
}

hinge_joint_constraint_handle addHingeJointConstraintFromGlobalPoints(scene_entity& a, scene_entity& b, vec3 globalAnchor, vec3 globalHingeAxis,
	float minLimit, float maxLimit)
{
	assert(a.registry == b.registry);

	entt::registry& registry = *a.registry;
	entt::entity constraintEntity = registry.create();
	hinge_joint_constraint& constraint = registry.emplace<hinge_joint_constraint>(constraintEntity);
	constraint_entity_reference_component& ref = registry.emplace<constraint_entity_reference_component>(constraintEntity);

	const transform_component& transformA = a.getComponent<transform_component>();
	const transform_component& transformB = b.getComponent<transform_component>();

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

	addConstraintEdge(a, ref, constraintEntity, constraint_type_hinge_joint);
	addConstraintEdge(b, ref, constraintEntity, constraint_type_hinge_joint);

	return { constraintEntity };
}

cone_twist_constraint_handle addConeTwistConstraintFromGlobalPoints(scene_entity& a, scene_entity& b, vec3 globalAnchor, vec3 globalAxis, 
	float swingLimit, float twistLimit)
{
	assert(a.registry == b.registry);

	entt::registry& registry = *a.registry;
	entt::entity constraintEntity = registry.create();
	cone_twist_constraint& constraint = registry.emplace<cone_twist_constraint>(constraintEntity);
	constraint_entity_reference_component& ref = registry.emplace<constraint_entity_reference_component>(constraintEntity);

	const transform_component& transformA = a.getComponent<transform_component>();
	const transform_component& transformB = b.getComponent<transform_component>();

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
	constraint.swingMotorType = constraint_velocity_motor;
	constraint.swingMotorVelocity = 0.f;
	constraint.maxSwingMotorTorque = -1.f; // Disabled by default.
	constraint.swingMotorAxis = 0.f;

	constraint.twistMotorType = constraint_velocity_motor;
	constraint.twistMotorVelocity = 0.f;
	constraint.maxTwistMotorTorque = -1.f; // Disabled by default.

	addConstraintEdge(a, ref, constraintEntity, constraint_type_cone_twist);
	addConstraintEdge(b, ref, constraintEntity, constraint_type_cone_twist);

	return { constraintEntity };
}

distance_constraint& getConstraint(game_scene& scene, distance_constraint_handle handle)
{
	return scene_entity{ handle.entity, scene }.getComponent<distance_constraint>();
}

ball_joint_constraint& getConstraint(game_scene& scene, ball_joint_constraint_handle handle)
{
	return scene_entity{ handle.entity, scene }.getComponent<ball_joint_constraint>();
}

hinge_joint_constraint& getConstraint(game_scene& scene, hinge_joint_constraint_handle handle)
{
	return scene_entity{ handle.entity, scene }.getComponent<hinge_joint_constraint>();
}

cone_twist_constraint& getConstraint(game_scene& scene, cone_twist_constraint_handle handle)
{
	return scene_entity{ handle.entity, scene }.getComponent<cone_twist_constraint>();
}

void deleteAllConstraints(game_scene& scene)
{
	scene.deleteAllComponents<distance_constraint>();
	scene.deleteAllComponents<ball_joint_constraint>();
	scene.deleteAllComponents<hinge_joint_constraint>();
	scene.deleteAllComponents<cone_twist_constraint>();
}

static void removeConstraintEdge(scene_entity entity, constraint_edge& edge, constraint_context& context)
{
	if (edge.prevConstraintEdge != INVALID_CONSTRAINT_EDGE)
	{
		context.constraintEdges[edge.prevConstraintEdge].nextConstraintEdge = edge.nextConstraintEdge;
	}
	else
	{
		physics_reference_component& ref = entity.getComponent<physics_reference_component>();
		ref.firstConstraintEdge = edge.nextConstraintEdge;
	}

	if (edge.nextConstraintEdge != INVALID_CONSTRAINT_EDGE)
	{
		context.constraintEdges[edge.nextConstraintEdge].prevConstraintEdge = edge.prevConstraintEdge;
	}

	context.freeConstraintEdge(edge);
}

static void deleteConstraint(entt::registry* registry, entt::entity constraintEntityHandle)
{
	scene_entity constraintEntity = { constraintEntityHandle, registry };
	constraint_entity_reference_component& constraint = constraintEntity.getComponent<constraint_entity_reference_component>();

	scene_entity parentEntityA = { constraint.entityA, registry };
	scene_entity parentEntityB = { constraint.entityB, registry };

	constraint_context& context = registry->ctx<constraint_context>();

	constraint_edge& edgeA = context.constraintEdges[constraint.edgeA];
	constraint_edge& edgeB = context.constraintEdges[constraint.edgeB];

	removeConstraintEdge(parentEntityA, edgeA, context);
	removeConstraintEdge(parentEntityB, edgeB, context);

	registry->destroy(constraintEntity.handle);
}

void deleteConstraint(game_scene& scene, distance_constraint_handle handle)
{
	deleteConstraint(&scene.registry, handle.entity);
}

void deleteConstraint(game_scene& scene, ball_joint_constraint_handle handle)
{
	deleteConstraint(&scene.registry, handle.entity);
}

void deleteConstraint(game_scene& scene, hinge_joint_constraint_handle handle)
{
	deleteConstraint(&scene.registry, handle.entity);
}

void deleteConstraint(game_scene& scene, cone_twist_constraint_handle handle)
{
	deleteConstraint(&scene.registry, handle.entity);
}

void deleteAllConstraintsFromEntity(scene_entity& entity)
{
	if (physics_reference_component* ref = entity.getComponentIfExists<physics_reference_component>())
	{
		entt::registry* registry = entity.registry;
		constraint_context& context = registry->ctx<constraint_context>();

		for (auto edgeIndex = ref->firstConstraintEdge; edgeIndex != INVALID_CONSTRAINT_EDGE; )
		{
			constraint_edge& edge = context.constraintEdges[edgeIndex];
			edgeIndex = edge.nextConstraintEdge;

			deleteConstraint(registry, edge.constraintEntity);
		}
	}
}

constraint_entity_iterator::iterator& constraint_entity_iterator::iterator::operator++()
{
	constraint_context& context = registry->ctx<constraint_context>();
	constraintEdgeIndex = context.constraintEdges[constraintEdgeIndex].nextConstraintEdge;
	return *this;
}

std::pair<scene_entity, constraint_type> constraint_entity_iterator::iterator::operator*()
{
	constraint_context& context = registry->ctx<constraint_context>();
	return { { context.constraintEdges[constraintEdgeIndex].constraintEntity, registry }, context.constraintEdges[constraintEdgeIndex].type };
}


void testPhysicsInteraction(game_scene& scene, ray r)
{
	float minT = FLT_MAX;
	rigid_body_component* minRB = 0;
	vec3 force;
	vec3 torque;

	for (auto [entityHandle, collider] : scene.view<collider_component>().each())
	{
		scene_entity rbEntity = { collider.parentEntity, scene };
		if (rigid_body_component* rb = rbEntity.getComponentIfExists<rigid_body_component>())
		{
			transform_component& transform = rbEntity.getComponent<transform_component>();

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
				minRB = rb;

				vec3 localHit = localR.origin + t * localR.direction;
				vec3 globalHit = transformPosition(transform, localHit);

				vec3 cogPosition = rb->getGlobalCOGPosition(transform);

				force = r.direction * physicsSettings.testForce;
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

static void getWorldSpaceColliders(game_scene& scene, bounding_box* outWorldspaceAABBs, collider_union* outWorldSpaceColliders, uint16 dummyRigidBodyIndex)
{
	CPU_PROFILE_BLOCK("Get world space colliders");

	uint32 pushIndex = 0;

	for (auto [entityHandle, collider] : scene.view<collider_component>().each())
	{
		bounding_box& bb = outWorldspaceAABBs[pushIndex];
		collider_union& col = outWorldSpaceColliders[pushIndex];
		++pushIndex;

		scene_entity entity = { collider.parentEntity, scene };
		assert(entity.hasComponent<transform_component>());
		transform_component& transform = entity.getComponent<transform_component>();

		col.type = collider.type;
		col.properties = collider.properties;

		if (entity.hasComponent<rigid_body_component>())
		{
			col.objectIndex = (uint16)entity.getComponentIndex<rigid_body_component>();
			col.objectType = physics_object_type_rigid_body;
		}
		else if (entity.hasComponent<force_field_component>())
		{
			col.objectIndex = (uint16)entity.getComponentIndex<force_field_component>();
			col.objectType = physics_object_type_force_field;
		}
		else
		{
			col.objectIndex = dummyRigidBodyIndex;
			col.objectType = physics_object_type_none;
		}

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

// Returns the accumulated force from all global force fields and writes localized forces (from force fields with colliders) in outLocalizedForceFields.
static vec3 getForceFieldStates(game_scene& scene, force_field_global_state* outLocalForceFields)
{
	vec3 globalForceField(0.f);

	for (auto [entityHandle, forceField] : scene.view<force_field_component>().each())
	{
		scene_entity entity = { entityHandle, scene };

		vec3 force = forceField.force; 
		if (transform_component* transform = entity.getComponentIfExists<transform_component>())
		{
			force = transform->rotation * force;
		}

		if (entity.hasComponent<collider_component>())
		{
			// Localized force field.
			uint16 index = (uint16)entity.getComponentIndex<force_field_component>();
			outLocalForceFields[index].force = force;
		}
		else
		{
			// Global force field.
			globalForceField += force;
		}
	}

	return globalForceField;
}

template <typename constraint_t>
static void getConstraintBodyPairs(game_scene& scene, constraint_body_pair* bodyPairs)
{
	uint32 i = scene.numberOfComponentsOfType<constraint_t>() - 1;
	for (auto [entityHandle, _] : scene.view<constraint_t>().each())
	{
		scene_entity entity = { entityHandle, scene };
		constraint_entity_reference_component& reference = entity.getComponent<constraint_entity_reference_component>();

		constraint_body_pair& pair = bodyPairs[i--];

		scene_entity rbAEntity = { reference.entityA, scene };
		scene_entity rbBEntity = { reference.entityB, scene };
		pair.rbA = (uint16)rbAEntity.getComponentIndex<rigid_body_component>();
		pair.rbB = (uint16)rbBEntity.getComponentIndex<rigid_body_component>();
	}
}


void physicsStep(game_scene& scene, memory_arena& arena, float dt)
{
	CPU_PROFILE_BLOCK("Physics step");

	if (physicsSettings.globalTimeScale <= 0.f)
	{
		return;
	}

	dt = min(dt, 1.f / 30.f);
	dt *= physicsSettings.globalTimeScale;

	uint32 numRigidBodies = scene.numberOfComponentsOfType<rigid_body_component>();
	if (numRigidBodies == 0)
	{
		return;
	}

	uint32 numForceFields = scene.numberOfComponentsOfType<force_field_component>();
	uint32 numColliders = scene.numberOfComponentsOfType<collider_component>();

	uint32 numDistanceConstraints = scene.numberOfComponentsOfType<distance_constraint>();
	uint32 numBallJointConstraints = scene.numberOfComponentsOfType<ball_joint_constraint>();
	uint32 numHingeJointConstraints = scene.numberOfComponentsOfType<hinge_joint_constraint>();
	uint32 numConeTwistConstraints = scene.numberOfComponentsOfType<cone_twist_constraint>();
	uint32 numConstraints = numDistanceConstraints + numBallJointConstraints + numHingeJointConstraints + numConeTwistConstraints;



	memory_marker marker = arena.getMarker();

	rigid_body_global_state* rbGlobal = arena.allocate<rigid_body_global_state>(numRigidBodies + 1); // Reserve one slot for dummy.
	force_field_global_state* ffGlobal = arena.allocate<force_field_global_state>(numForceFields);
	bounding_box* worldSpaceAABBs = arena.allocate<bounding_box>(numColliders);
	collider_union* worldSpaceColliders = arena.allocate<collider_union>(numColliders);

	broadphase_collision* possibleCollisions = arena.allocate<broadphase_collision>(numColliders * numColliders); // Conservative estimate.

	uint32 dummyRigidBodyIndex = numRigidBodies;

	// Collision detection.
	getWorldSpaceColliders(scene, worldSpaceAABBs, worldSpaceColliders, dummyRigidBodyIndex);
	uint32 numPossibleCollisions = broadphase(scene, 0, worldSpaceAABBs, arena, possibleCollisions);

	non_collision_interaction* nonCollisionInteractions = arena.allocate<non_collision_interaction>(numPossibleCollisions);
	collision_contact* contacts = arena.allocate<collision_contact>(numPossibleCollisions * 4); // Each collision can have up to 4 contact points.
	constraint_body_pair* allConstraintBodyPairs = arena.allocate<constraint_body_pair>(
		numConstraints + numPossibleCollisions * 4);

	constraint_body_pair* collisionBodyPairs = allConstraintBodyPairs + numConstraints;

	narrowphase_result narrowPhaseResult = narrowphase(worldSpaceColliders, possibleCollisions, numPossibleCollisions, contacts, collisionBodyPairs, nonCollisionInteractions);

	vec3 globalForceField = getForceFieldStates(scene, ffGlobal);


	// Handle non-collision interactions (triggers, force fields etc).
	for (uint32 i = 0; i < narrowPhaseResult.numNonCollisionInteractions; ++i)
	{
		non_collision_interaction interaction = nonCollisionInteractions[i];
		rigid_body_component& rb = scene.getComponentAtIndex<rigid_body_component>(interaction.rigidBodyIndex);

		switch (interaction.otherType)
		{
			case physics_object_type_force_field:
			{
				const force_field_global_state& ff = ffGlobal[interaction.otherIndex];
				rb.forceAccumulator += ff.force;
			} break;
		}
	}

	CPU_PROFILE_STAT("Num rigid bodies: %u", numRigidBodies);
	CPU_PROFILE_STAT("Num colliders: %u", numColliders);
	CPU_PROFILE_STAT("Num broadphase overlaps: %u", numPossibleCollisions);
	CPU_PROFILE_STAT("Num narrowphase contacts: %u", narrowPhaseResult.numContacts);


	//  Apply global forces (including gravity) and air drag and integrate forces.
	{
		CPU_PROFILE_BLOCK("Integrate rigid body forces");

		uint32 rbIndex = numRigidBodies - 1; // EnTT iterates back to front.
		for (auto [entityHandle, rb, transform] : scene.group<rigid_body_component, transform_component>().each())
		{
			rigid_body_global_state& global = rbGlobal[rbIndex--];
			rb.forceAccumulator += globalForceField;
			rb.applyGravityAndIntegrateForces(global, transform, dt);
		}
	}

	// Kinematic rigid body. This is used in collision constraint solving, when a collider has no rigid body.
	memset(&rbGlobal[dummyRigidBodyIndex], 0, sizeof(rigid_body_global_state));


	// Collect constraints.
	uint32 numContacts = narrowPhaseResult.numContacts;

	distance_constraint* distanceConstraints = scene.raw<distance_constraint>();
	ball_joint_constraint* ballJointConstraints = scene.raw<ball_joint_constraint>();
	hinge_joint_constraint* hingeJointConstraints = scene.raw<hinge_joint_constraint>();
	cone_twist_constraint* coneTwistConstraints = scene.raw<cone_twist_constraint>();

	constraint_body_pair* distanceConstraintBodyPairs = allConstraintBodyPairs + 0;
	constraint_body_pair* ballJointConstraintBodyPairs = distanceConstraintBodyPairs + numDistanceConstraints;
	constraint_body_pair* hingeJointConstraintBodyPairs = ballJointConstraintBodyPairs + numBallJointConstraints;
	constraint_body_pair* coneTwistConstraintBodyPairs = hingeJointConstraintBodyPairs + numHingeJointConstraints;

	getConstraintBodyPairs<distance_constraint>(scene, distanceConstraintBodyPairs);
	getConstraintBodyPairs<ball_joint_constraint>(scene, ballJointConstraintBodyPairs);
	getConstraintBodyPairs<hinge_joint_constraint>(scene, hingeJointConstraintBodyPairs);
	getConstraintBodyPairs<cone_twist_constraint>(scene, coneTwistConstraintBodyPairs);

	distance_constraint_solver distanceConstraintSolver;
	simd_distance_constraint_solver distanceConstraintSolverSIMD;

	ball_joint_constraint_solver ballJointConstraintSolver;
	simd_ball_joint_constraint_solver ballJointConstraintSolverSIMD;

	hinge_joint_constraint_solver hingeJointConstraintSolver;
	simd_hinge_joint_constraint_solver hingeJointConstraintSolverSIMD;

	cone_twist_constraint_solver coneTwistConstraintSolver;
	simd_cone_twist_constraint_solver coneTwistConstraintSolverSIMD;

	collision_constraint_solver collisionConstraintSolver;
	simd_collision_constraint_solver collisionConstraintSolverSIMD;


	// Solve constraints.
	{
		CPU_PROFILE_BLOCK("Initialize constraints");

		if (physicsSettings.simd)
		{
			distanceConstraintSolverSIMD = initializeDistanceVelocityConstraintsSIMD(arena, rbGlobal, distanceConstraints, distanceConstraintBodyPairs, numDistanceConstraints, dt);
			ballJointConstraintSolverSIMD = initializeBallJointVelocityConstraintsSIMD(arena, rbGlobal, ballJointConstraints, ballJointConstraintBodyPairs, numBallJointConstraints, dt);
			hingeJointConstraintSolverSIMD = initializeHingeJointVelocityConstraintsSIMD(arena, rbGlobal, hingeJointConstraints, hingeJointConstraintBodyPairs, numHingeJointConstraints, dt);
			coneTwistConstraintSolverSIMD = initializeConeTwistVelocityConstraintsSIMD(arena, rbGlobal, coneTwistConstraints, coneTwistConstraintBodyPairs, numConeTwistConstraints, dt);
			collisionConstraintSolverSIMD = initializeCollisionVelocityConstraintsSIMD(arena, rbGlobal, contacts, collisionBodyPairs, numContacts, dummyRigidBodyIndex, dt);
		}
		else
		{
			distanceConstraintSolver = initializeDistanceVelocityConstraints(arena, rbGlobal, distanceConstraints, distanceConstraintBodyPairs, numDistanceConstraints, dt);
			ballJointConstraintSolver = initializeBallJointVelocityConstraints(arena, rbGlobal, ballJointConstraints, ballJointConstraintBodyPairs, numBallJointConstraints, dt);
			hingeJointConstraintSolver = initializeHingeJointVelocityConstraints(arena, rbGlobal, hingeJointConstraints, hingeJointConstraintBodyPairs, numHingeJointConstraints, dt);
			coneTwistConstraintSolver = initializeConeTwistVelocityConstraints(arena, rbGlobal, coneTwistConstraints, coneTwistConstraintBodyPairs, numConeTwistConstraints, dt);
			collisionConstraintSolver = initializeCollisionVelocityConstraints(arena, rbGlobal, contacts, collisionBodyPairs, numContacts, dt);
		}
	}
	{
		CPU_PROFILE_BLOCK("Solve constraints");

		for (uint32 it = 0; it < physicsSettings.numRigidSolverIterations; ++it)
		{
			if (physicsSettings.simd)
			{
				solveDistanceVelocityConstraintsSIMD(distanceConstraintSolverSIMD, rbGlobal);
				solveBallJointVelocityConstraintsSIMD(ballJointConstraintSolverSIMD, rbGlobal);
				solveHingeJointVelocityConstraintsSIMD(hingeJointConstraintSolverSIMD, rbGlobal);
				solveConeTwistVelocityConstraintsSIMD(coneTwistConstraintSolverSIMD, rbGlobal);
				solveCollisionVelocityConstraintsSIMD(collisionConstraintSolverSIMD, rbGlobal);
			}
			else
			{
				solveDistanceVelocityConstraints(distanceConstraintSolver, rbGlobal);
				solveBallJointVelocityConstraints(ballJointConstraintSolver, rbGlobal);
				solveHingeJointVelocityConstraints(hingeJointConstraintSolver, rbGlobal);
				solveConeTwistVelocityConstraints(coneTwistConstraintSolver, rbGlobal);
				solveCollisionVelocityConstraints(collisionConstraintSolver, rbGlobal);
			}
		}
	}


	// Integrate velocities.
	{
		CPU_PROFILE_BLOCK("Integrate rigid body velocities");

		uint32 rbIndex = numRigidBodies - 1; // EnTT iterates back to front.
		for (auto [entityHandle, rb, transform] : scene.group<rigid_body_component, transform_component>().each())
		{
			rigid_body_global_state& global = rbGlobal[rbIndex--];
			rb.integrateVelocity(global, transform, dt);
		}
	}


	// Cloth. This needs to get integrated with the rest of the system.

	for (auto [entityHandle, cloth] : scene.view<cloth_component>().each())
	{
		cloth.applyWindForce(globalForceField);
		cloth.simulate(physicsSettings.numClothVelocityIterations, physicsSettings.numClothPositionIterations, physicsSettings.numClothDriftIterations, dt);
	}

	arena.resetToMarker(marker);
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
