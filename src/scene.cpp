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
