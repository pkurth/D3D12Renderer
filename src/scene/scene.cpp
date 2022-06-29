#include "pch.h"
#include "scene.h"
#include "physics/physics.h"
#include "physics/collision_broad.h"
#include "rendering/raytracing.h"


game_scene::game_scene()
{
	// Construct groups early. Ingore the return types.
	(void)registry.group<collider_component, sap_endpoint_indirection_component>(); // Colliders and SAP endpoints are always sorted in the same order.
	(void)registry.group<transform_component, dynamic_transform_component, rigid_body_component>();
	(void)registry.group<transform_component, rigid_body_component>();

#ifndef PHYSICS_ONLY
	(void)registry.group<position_component, point_light_component>();
	(void)registry.group<position_rotation_component, spot_light_component>();
#endif
}

void game_scene::clearAll()
{
	void clearBroadphase(game_scene & scene);
	clearBroadphase(*this);

	deleteAllConstraints(*this);
	registry.clear();
}

void game_scene::cloneTo(game_scene& target)
{
	target.registry.assign(registry.data(), registry.data() + registry.size(), registry.released());

	copyComponentPoolsTo <
		tag_component,
		transform_component,
		dynamic_transform_component,
		position_component,
		position_rotation_component,

#ifndef PHYSICS_ONLY
		point_light_component,
		spot_light_component,
		cloth_render_component,
#endif

		animation_component,
		raster_component,
		raytrace_component,

		collider_component,
		rigid_body_component,
		force_field_component,
		trigger_component,
		cloth_component,
		physics_reference_component,
		sap_endpoint_indirection_component,
		constraint_entity_reference_component,

		distance_constraint,
		ball_constraint,
		fixed_constraint,
		hinge_constraint,
		cone_twist_constraint,
		slider_constraint
	> (target);

	target.registry.ctx() = registry.ctx();

#ifndef PHYSICS_ONLY
	target.camera = camera;
	target.sun = sun;
	target.environment = environment;
#endif
}

scene_entity game_scene::copyEntity(scene_entity src)
{
	assert(src.hasComponent<tag_component>());

	tag_component& tag = src.getComponent<tag_component>();
	scene_entity dest = createEntity(tag.name);

	if (auto* c = src.getComponentIfExists<transform_component>()) { dest.addComponent<transform_component>(*c); }
	if (auto* c = src.getComponentIfExists<position_component>()) { dest.addComponent<position_component>(*c); }
	if (auto* c = src.getComponentIfExists<position_rotation_component>()) { dest.addComponent<position_rotation_component>(*c); }
	if (auto* c = src.getComponentIfExists<dynamic_transform_component>()) { dest.addComponent<dynamic_transform_component>(*c); }

#ifndef PHYSICS_ONLY
	if (auto* c = src.getComponentIfExists<point_light_component>()) { dest.addComponent<point_light_component>(*c); }
	if (auto* c = src.getComponentIfExists<spot_light_component>()) { dest.addComponent<spot_light_component>(*c); }
	if (auto* c = src.getComponentIfExists<cloth_render_component>()) { dest.addComponent<cloth_render_component>(*c); }
#endif

	if (auto* c = src.getComponentIfExists<animation_component>()) { dest.addComponent<animation_component>(*c); }
	if (auto* c = src.getComponentIfExists<raster_component>()) { dest.addComponent<raster_component>(*c); }
	if (auto* c = src.getComponentIfExists<raytrace_component>()) { dest.addComponent<raytrace_component>(*c); }

	for (collider_component& collider : collider_component_iterator(src))
	{
		dest.addComponent<collider_component>(collider);
	}

	if (auto* c = src.getComponentIfExists<rigid_body_component>()) { dest.addComponent<rigid_body_component>(*c); }
	if (auto* c = src.getComponentIfExists<force_field_component>()) { dest.addComponent<force_field_component>(*c); }
	if (auto* c = src.getComponentIfExists<trigger_component>()) { dest.addComponent<trigger_component>(*c); }
	if (auto* c = src.getComponentIfExists<cloth_component>()) { dest.addComponent<cloth_component>(*c); }

	// We don't copy physics constraints (because I cannot think of a good way to do this).

	return dest;
}

void game_scene::deleteEntity(scene_entity e)
{
	void removeColliderFromBroadphase(scene_entity entity);

	if (physics_reference_component* reference = e.getComponentIfExists<physics_reference_component>())
	{
		scene_entity colliderEntity = { reference->firstColliderEntity, &registry };
		while (colliderEntity)
		{
			collider_component& collider = colliderEntity.getComponent<collider_component>();
			entity_handle next = collider.nextEntity;

			removeColliderFromBroadphase(colliderEntity);
		
			registry.destroy(colliderEntity.handle);
		
			colliderEntity = { next, &registry };
		}

		deleteAllConstraintsFromEntity(e);
	}

	registry.destroy(e.handle);
}
