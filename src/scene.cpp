#include "pch.h"
#include "scene.h"
#include "physics.h"
#include "collision_broad.h"

scene::scene()
{
	auto _ = registry.group<collider_component, sap_endpoint_indirection_component>(); // Colliders and SAP endpoints are always sorted in the same order.
	
	registry.on_construct<collider_component>().connect<&onColliderAdded>();
	registry.on_destroy<collider_component>().connect<&onColliderRemoved>();
}

void scene::clearAll()
{
	registry.clear();
}

void scene::deleteEntity(scene_entity e)
{
	if (e.hasComponent<physics_reference_component>())
	{
		auto colliderView = view<collider_component>();

		physics_reference_component& reference = e.getComponent<physics_reference_component>();

		scene_entity colliderEntity = { reference.firstColliderEntity, &registry };
		while (colliderEntity)
		{
			collider_component& collider = colliderEntity.getComponent<collider_component>();
			entt::entity next = collider.nextEntity;
		
			registry.destroy(colliderEntity.handle);
		
			colliderEntity = { next, &registry };
		}
	}

	registry.destroy(e.handle);
}
