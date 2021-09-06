#pragma once

//#define ENTT_ID_TYPE uint64
//#include <entt/entt.hpp>
#include <entt/entity/registry.hpp>
#include "core/math.h"

struct tag_component
{
	char name[16];

	tag_component(const char* n)
	{
		strncpy(name, n, sizeof(name));
		name[sizeof(name) - 1] = 0;
	}
};

struct dynamic_geometry_component
{
	trs lastFrameTransform = trs::identity;
};

struct scene_entity
{
	scene_entity() = default;
	inline scene_entity(entt::entity handle, struct scene& scene);
	inline scene_entity(uint32 id, struct scene& scene);
	scene_entity(entt::entity handle, entt::registry* registry) : handle(handle), registry(registry) {}
	scene_entity(const scene_entity&) = default;

	template <typename component_t>
	bool hasComponent()
	{
		return registry->any_of<component_t>(handle);
	}

	template <typename component_t, typename... args>
	scene_entity& addComponent(args&&... a)
	{
		if constexpr (std::is_same_v<component_t, struct collider_component>)
		{
			if (!hasComponent<physics_reference_component>())
			{
				addComponent<physics_reference_component>();
			}

			physics_reference_component& reference = getComponent<physics_reference_component>();
			++reference.numColliders;

			entt::entity child = registry->create();
			collider_component& collider = registry->emplace<collider_component>(child, std::forward<args>(a)...);

			collider.parentEntity = handle;
			collider.nextEntity = reference.firstColliderEntity;
			reference.firstColliderEntity = child;


			if (hasComponent<rigid_body_component>())
			{
				getComponent<rigid_body_component>().recalculateProperties(registry, reference);
			}
		}
		else
		{
			auto& component = registry->emplace<component_t>(handle, std::forward<args>(a)...);

			// If component is rigid body, calculate properties.
			if constexpr (std::is_same_v<component_t, struct rigid_body_component>)
			{
				if (hasComponent<physics_reference_component>())
				{
					component.recalculateProperties(registry, getComponent<physics_reference_component>());
				}
				if (!hasComponent<dynamic_geometry_component>())
				{
					addComponent<dynamic_geometry_component>();
				}
			}
		}

		return *this;
	}

	template <typename component_t>
	component_t& getComponent()
	{
		return registry->get<component_t>(handle);
	}

	template <typename component_t>
	const component_t& getComponent() const
	{
		return registry->get<component_t>(handle);
	}

	template <typename component_t>
	void removeComponent()
	{
		registry->remove<component_t>(handle);
	}

	inline operator uint32()
	{
		return (uint32)handle;
	}

	inline operator bool()
	{
		return handle != entt::null;
	}

	inline bool operator==(const scene_entity& o)
	{
		return handle == o.handle && registry == o.registry;
	}

	inline bool operator!=(const scene_entity& o)
	{
		return !(*this == o);
	}

	inline bool operator==(entt::entity o)
	{
		return handle == o;
	}

	entt::entity handle = entt::null;
	entt::registry* registry;


	scene_entity(uint32 id, entt::registry* reg) : handle((entt::entity)id), registry(reg) {}

	friend struct scene;
	friend void onColliderAdded(entt::registry& registry, entt::entity entityHandle);
};

struct scene
{
	scene();

	scene_entity createEntity(const char* name)
	{
		return scene_entity(registry.create(), &registry)
			.addComponent<tag_component>(name);
	}

	void deleteEntity(scene_entity e);
	void clearAll();

	bool isEntityValid(scene_entity e)
	{
		return registry.valid(e.handle);
	}

	template <typename component_t>
	void copyComponentIfExists(scene_entity src, scene_entity dst)
	{
		if (src.hasComponent<component_t>())
		{
			dst.addComponent<component_t>(src.getComponent<component_t>());
		}
	}

	template <typename first_component_t, typename... tail_component_t>
	void copyComponentsIfExists(scene_entity src, scene_entity dst)
	{
		copyComponentIfExists<first_component_t>(src, dst);
		if constexpr (sizeof...(tail_component_t) > 0)
		{
			copyComponentsIfExists<tail_component_t...>(src, dst);
		}
	}

	template <typename... component_t>
	auto view() 
	{ 
		return registry.view<component_t...>(); 
	}

	template<typename... owned_component_t, typename... Get, typename... Exclude>
	auto group(entt::get_t<Get...> = {}, entt::exclude_t<Exclude...> = {})
	{
		return registry.group<owned_component_t...>(entt::get<Get...>, entt::exclude<Exclude...>);
	}

	template <typename component_t>
	auto raw()
	{
		return registry.view<component_t>().raw();
	}

	template <typename func_t>
	void forEachEntity(func_t func)
	{
		registry.each(func);
	}

	template <typename component_t>
	uint32 numberOfComponentsOfType()
	{
		return (uint32)registry.size<component_t>();
	}

	entt::registry registry;
};

inline scene_entity::scene_entity(entt::entity handle, struct scene& scene) : handle(handle), registry(&scene.registry) {}
inline scene_entity::scene_entity(uint32 id, struct scene& scene) : handle((entt::entity)id), registry(&scene.registry) {}
