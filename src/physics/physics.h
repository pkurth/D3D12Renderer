#pragma once

#include "core/math.h"
#include "bounding_volumes.h"
#include "scene/scene.h"
#include "constraints.h"
#include "rigid_body.h"
#include "cloth.h"
#include "force_field.h"

#define GRAVITY -9.81f

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

enum physics_object_type : uint8
{
	physics_object_type_none,
	physics_object_type_rigid_body,
	physics_object_type_force_field,

	physics_object_type_count,
};

enum collider_type : uint8
{
	// The order here is important. See collision_narrow.cpp.
	collider_type_sphere,
	collider_type_capsule,
	collider_type_aabb,
	collider_type_obb,
	collider_type_hull,

	collider_type_count,
};

static const char* colliderTypeNames[] =
{
	"Sphere",
	"Capsule",
	"AABB",
	"OBB",
	"Hull",
};

static_assert(arraysize(colliderTypeNames) == collider_type_count, "Missing collider name");

struct collider_union
{
	collider_union() {}
	physics_properties calculatePhysicsProperties();

	collider_type type;
	physics_object_type objectType;
	uint16 rigidBodyIndex;

	union
	{
		bounding_sphere sphere;
		bounding_capsule capsule;
		bounding_box aabb;
		bounding_oriented_box obb;
		bounding_hull hull;
	};

	collider_properties properties;
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
		aabb = b;
		initialize(collider_type_aabb, restitution, friction, density);
	}
	collider_component(bounding_oriented_box b, float restitution, float friction, float density)
	{
		obb = b;
		initialize(collider_type_obb, restitution, friction, density);
	}
	collider_component(bounding_hull h, float restitution, float friction, float density)
	{
		hull = h;
		initialize(collider_type_hull, restitution, friction, density);
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

#define INVALID_BOUNDING_HULL_INDEX -1

uint32 allocateBoundingHullGeometry(const struct cpu_mesh& mesh);
uint32 allocateBoundingHullGeometry(const std::string& meshFilepath);

struct distance_constraint_handle { uint16 index; };
struct ball_joint_constraint_handle { uint16 index; };
struct hinge_joint_constraint_handle { uint16 index; };
struct cone_twist_constraint_handle { uint16 index; };

// Local anchors are always in the space of the entities.
distance_constraint_handle addDistanceConstraintFromLocalPoints(scene_entity& a, scene_entity& b, vec3 localAnchorA, vec3 localAnchorB, float distance);
distance_constraint_handle addDistanceConstraintFromGlobalPoints(scene_entity& a, scene_entity& b, vec3 globalAnchorA, vec3 globalAnchorB); // Calculates distance from current configuration.

ball_joint_constraint_handle addBallJointConstraintFromLocalPoints(scene_entity& a, scene_entity& b, vec3 localAnchorA, vec3 localAnchorB);
ball_joint_constraint_handle addBallJointConstraintFromGlobalPoints(scene_entity& a, scene_entity& b, vec3 globalAnchor); // Calculates local anchors from current configuration.

// The min limit is in the range [-pi, 0], the max limit in the range [0, pi]. 
// If the specified values are not in this range, the limits are disabled.
// Limits are specified as allowed deviations from the initial relative rotation.
// Usually the absolute of each limit should be a lot smaller than pi.
hinge_joint_constraint_handle addHingeJointConstraintFromGlobalPoints(scene_entity& a, scene_entity& b, vec3 globalAnchor, vec3 globalHingeAxis,
	float minLimit = 1.f, float maxLimit = -1.f);

cone_twist_constraint_handle addConeTwistConstraintFromGlobalPoints(scene_entity& a, scene_entity& b, vec3 globalAnchor, vec3 globalAxis, 
	float swingLimit, float twistLimit);


distance_constraint& getConstraint(distance_constraint_handle handle);
ball_joint_constraint& getConstraint(ball_joint_constraint_handle handle);
hinge_joint_constraint& getConstraint(hinge_joint_constraint_handle handle);
cone_twist_constraint& getConstraint(cone_twist_constraint_handle handle);


void deleteAllConstraints();


struct physics_settings
{
	uint32 numRigidSolverIterations = 30;

	uint32 numClothVelocityIterations = 0;
	uint32 numClothPositionIterations = 1;
	uint32 numClothDriftIterations = 0;
};

void testPhysicsInteraction(scene& appScene, ray r, float forceAmount);
void physicsStep(scene& appScene, float dt, physics_settings settings = {});
