#pragma once

#include "core/math.h"
#include "core/memory.h"
#include "bounding_volumes.h"
#include "scene/scene.h"
#include "constraints.h"
#include "rigid_body.h"
#include "cloth.h"

#define GRAVITY -9.81f

struct physics_properties
{
	mat3 inertia;
	vec3 cog;
	float mass;
};

struct physics_material
{
	float restitution;
	float friction;
	float density;
};

enum physics_object_type : uint8
{
	physics_object_type_rigid_body,
	physics_object_type_static_collider,
	physics_object_type_force_field,
	physics_object_type_trigger,

	physics_object_type_count,
};

enum collider_type : uint8
{
	// The order here is important. See collision_narrow.cpp.
	collider_type_sphere,
	collider_type_capsule,
	collider_type_cylinder,
	collider_type_aabb,
	collider_type_obb,
	collider_type_hull,

	collider_type_count,
};

static const char* colliderTypeNames[] =
{
	"Sphere",
	"Capsule",
	"Cylinder",
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

	// These two are only used internally and should not be read outside.
	physics_object_type objectType;
	uint16 objectIndex; // Depending on objectType: Rigid body index, force field index, ...

	union
	{
		bounding_sphere sphere;
		bounding_capsule capsule;
		bounding_cylinder cylinder;
		bounding_box aabb;
		bounding_oriented_box obb;
		bounding_hull hull;
	};

	physics_material material;
};

struct collider_component : collider_union
{
	static collider_component asSphere(bounding_sphere s, physics_material material)
	{
		collider_component result;
		result.sphere = s;
		result.type = collider_type_sphere;
		result.material = material;
		return result;
	}
	static collider_component asCapsule(bounding_capsule c, physics_material material)
	{
		collider_component result;
		result.capsule = c;
		result.type = collider_type_capsule;
		result.material = material;
		return result;
	}
	static collider_component asCylinder(bounding_cylinder c, physics_material material)
	{
		collider_component result;
		result.cylinder = c;
		result.type = collider_type_cylinder;
		result.material = material;
		return result;
	}
	static collider_component asAABB(bounding_box b, physics_material material)
	{
		collider_component result;
		result.aabb = b;
		result.type = collider_type_aabb;
		result.material = material;
		return result;
	}
	static collider_component asOBB(bounding_oriented_box b, physics_material material)
	{
		collider_component result;
		result.obb = b;
		result.type = collider_type_obb;
		result.material = material;
		return result;
	}
	static collider_component asHull(bounding_hull h, physics_material material)
	{
		collider_component result;
		result.hull = h;
		result.type = collider_type_hull;
		result.material = material;
		return result;
	}

	collider_component() = default;

	// Set by scene on component creation.
	entt::entity parentEntity;
	entt::entity nextEntity;
};

struct physics_reference_component
{
	uint32 numColliders = 0;
	entt::entity firstColliderEntity = entt::null;

	uint16 firstConstraintEdge = INVALID_CONSTRAINT_EDGE;
};

struct force_field_component
{
	vec3 force;
};

enum trigger_event_type
{
	trigger_event_enter,
	trigger_event_leave,
};

struct trigger_event
{
	scene_entity trigger;
	scene_entity other;
	trigger_event_type type;
};

struct trigger_component
{
	std::function<void(trigger_event)> callback;
};

#define INVALID_BOUNDING_HULL_INDEX -1

uint32 allocateBoundingHullGeometry(const std::string& meshFilepath);

struct distance_constraint_handle { entt::entity entity; };
struct ball_constraint_handle { entt::entity entity; };
struct fixed_constraint_handle { entt::entity entity; };
struct hinge_constraint_handle { entt::entity entity; };
struct cone_twist_constraint_handle { entt::entity entity; };
struct slider_constraint_handle { entt::entity entity; };

// Local anchors are always in the space of the entities.
distance_constraint_handle addDistanceConstraintFromLocalPoints(scene_entity& a, scene_entity& b, vec3 localAnchorA, vec3 localAnchorB, float distance);
distance_constraint_handle addDistanceConstraintFromGlobalPoints(scene_entity& a, scene_entity& b, vec3 globalAnchorA, vec3 globalAnchorB); // Calculates distance from current configuration.

ball_constraint_handle addBallConstraintFromLocalPoints(scene_entity& a, scene_entity& b, vec3 localAnchorA, vec3 localAnchorB);
ball_constraint_handle addBallConstraintFromGlobalPoints(scene_entity& a, scene_entity& b, vec3 globalAnchor); // Calculates local anchors from current configuration.

fixed_constraint_handle addFixedConstraintFromGlobalPoints(scene_entity& a, scene_entity& b, vec3 globalAnchor); // Calculates local anchors from current configuration.

// The min limit is in the range [-pi, 0], the max limit in the range [0, pi]. 
// If the specified values are not in this range, the limits are disabled.
// Limits are specified as allowed deviations from the initial relative rotation.
// Usually the absolute of each limit should be a lot smaller than pi.
hinge_constraint_handle addHingeConstraintFromGlobalPoints(scene_entity& a, scene_entity& b, vec3 globalAnchor, vec3 globalHingeAxis,
	float minLimit = 1.f, float maxLimit = -1.f);

cone_twist_constraint_handle addConeTwistConstraintFromGlobalPoints(scene_entity& a, scene_entity& b, vec3 globalAnchor, vec3 globalAxis, 
	float swingLimit, float twistLimit);

slider_constraint_handle addSliderConstraintFromGlobalPoints(scene_entity& a, scene_entity& b, vec3 globalAnchor, vec3 globalAxis, float minLimit = 1.f, float maxLimit = -1.f);


distance_constraint& getConstraint(game_scene& scene, distance_constraint_handle handle);
ball_constraint& getConstraint(game_scene& scene, ball_constraint_handle handle);
fixed_constraint& getConstraint(game_scene& scene, fixed_constraint_handle handle);
hinge_constraint& getConstraint(game_scene& scene, hinge_constraint_handle handle);
cone_twist_constraint& getConstraint(game_scene& scene, cone_twist_constraint_handle handle);
slider_constraint& getConstraint(game_scene& scene, slider_constraint_handle handle);

void deleteAllConstraints(game_scene& scene);

void deleteConstraint(game_scene& scene, distance_constraint_handle handle);
void deleteConstraint(game_scene& scene, ball_constraint_handle handle);
void deleteConstraint(game_scene& scene, fixed_constraint_handle handle);
void deleteConstraint(game_scene& scene, hinge_constraint_handle handle);
void deleteConstraint(game_scene& scene, cone_twist_constraint_handle handle);
void deleteConstraint(game_scene& scene, slider_constraint_handle handle);

void deleteAllConstraintsFromEntity(scene_entity& entity);

static scene_entity getOtherEntity(const constraint_entity_reference_component& constraint, scene_entity first)
{
	entt::entity result = (first == constraint.entityA) ? constraint.entityB : constraint.entityA;
	return { result, first.registry };
}


struct collider_entity_iterator
{
	collider_entity_iterator(scene_entity entity)
	{
		if (physics_reference_component* ref = entity.getComponentIfExists<physics_reference_component>())
		{
			firstColliderEntity = { ref->firstColliderEntity, entity.registry };
		}
	}

	struct iterator
	{
		scene_entity entity;

		friend bool operator!=(const iterator& a, const iterator& b) { return a.entity != b.entity; }
		iterator& operator++() { entity = { entity.getComponent<collider_component>().nextEntity, entity.registry }; return *this; }
		scene_entity operator*() { return entity; }
	};

	iterator begin() { return iterator{ firstColliderEntity }; }
	iterator end() { return iterator{ scene_entity{ entt::entity(entt::null), firstColliderEntity.registry } }; }

	scene_entity firstColliderEntity = {};
};

struct collider_component_iterator : collider_entity_iterator
{
	collider_component_iterator(scene_entity entity) : collider_entity_iterator(entity) {}

	struct iterator : collider_entity_iterator::iterator 
	{
		collider_component& operator*() { return entity.getComponent<collider_component>(); }
	};

	iterator begin() { return iterator{ firstColliderEntity }; }
	iterator end() { return iterator{ scene_entity{ entt::entity(entt::null), firstColliderEntity.registry } }; }
};

struct constraint_entity_iterator
{
	constraint_entity_iterator(scene_entity entity)
		: registry(entity.registry)
	{
		if (physics_reference_component* ref = entity.getComponentIfExists<physics_reference_component>())
		{
			firstConstraintEdgeIndex = ref->firstConstraintEdge;
		}
	}

	struct iterator
	{
		uint16 constraintEdgeIndex;
		entt::registry* registry;

		friend bool operator!=(const iterator& a, const iterator& b) { return a.constraintEdgeIndex != b.constraintEdgeIndex; }
		iterator& operator++();
		std::pair<scene_entity, constraint_type> operator*();
	};

	iterator begin() { return iterator{ firstConstraintEdgeIndex, registry }; }
	iterator end() { return iterator{ INVALID_CONSTRAINT_EDGE, registry }; }

	uint16 firstConstraintEdgeIndex = INVALID_CONSTRAINT_EDGE;
	entt::registry* registry;
};


struct physics_settings
{
	uint32 numRigidSolverIterations = 30;

	uint32 numClothVelocityIterations = 0;
	uint32 numClothPositionIterations = 1;
	uint32 numClothDriftIterations = 0;

	float testForce = 1000.f;

	float globalTimeScale = 1.f;

	bool simd = true;
};


enum collision_event_type
{
	collision_event_start,
	collision_event_end,
};

struct collision_event
{
	scene_entity entityA;
	scene_entity colliderEntityA;
	const collider_component& colliderA;

	scene_entity entityB;
	scene_entity colliderEntityB;
	const collider_component& colliderB;

	collision_event_type type;
};

extern physics_settings physicsSettings;
extern std::function<void(const collision_event&)> collisionCallback;

void testPhysicsInteraction(game_scene& scene, ray r);
void physicsStep(game_scene& scene, memory_arena& arena, float dt);
