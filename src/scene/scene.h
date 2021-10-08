#pragma once

//#define ENTT_ID_TYPE uint64
//#include <entt/entt.hpp>
#include <entt/entity/registry.hpp>
#include "components.h"
#include "rendering/light_source.h"
#include "rendering/pbr.h"
#include "core/camera.h"

struct game_scene;

struct scene_entity
{
	scene_entity() = default;
	inline scene_entity(entt::entity handle, game_scene& scene);
	inline scene_entity(uint32 id, game_scene& scene);
	scene_entity(entt::entity handle, entt::registry* registry) : handle(handle), registry(registry) {}
	scene_entity(uint32 id, entt::registry* reg) : handle((entt::entity)id), registry(reg) {}
	scene_entity(const scene_entity&) = default;

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


			if (rigid_body_component* rb = getComponentIfExists<rigid_body_component>())
			{
				rb->recalculateProperties(registry, reference);
			}
		}
		else
		{
			auto& component = registry->emplace_or_replace<component_t>(handle, std::forward<args>(a)...);

			// If component is rigid body, calculate properties.
			if constexpr (std::is_same_v<component_t, struct rigid_body_component>)
			{
				if (physics_reference_component* ref = getComponentIfExists<physics_reference_component>())
				{
					component.recalculateProperties(registry, *ref);
				}
				if (!hasComponent<dynamic_transform_component>())
				{
					addComponent<dynamic_transform_component>();
				}
			}


			// If component is cloth, transform to correct position.
			if constexpr (std::is_same_v<component_t, struct cloth_component>)
			{
				if (transform_component* transform = getComponentIfExists<transform_component>())
				{
					component.setWorldPositionOfFixedVertices(*transform, true);
				}
			}

			if constexpr (std::is_same_v<component_t, struct transform_component>)
			{
				if (cloth_component* cloth = getComponentIfExists<cloth_component>())
				{
					cloth->setWorldPositionOfFixedVertices(component, true);
				}
			}
		}

		return *this;
	}

	template <typename component_t>
	bool hasComponent()
	{
		return registry->any_of<component_t>(handle);
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
	component_t* getComponentIfExists()
	{
		return registry->try_get<component_t>(handle);
	}

	template <typename component_t>
	const component_t* getComponentIfExists() const
	{
		return registry->try_get<component_t>(handle);
	}

	template <typename component_t>
	void removeComponent()
	{
		registry->remove<component_t>(handle);
	}

	inline operator uint32() const
	{
		return (uint32)handle;
	}

	inline operator bool() const
	{
		return handle != entt::null;
	}

	inline bool operator==(const scene_entity& o) const
	{
		return handle == o.handle && registry == o.registry;
	}

	inline bool operator!=(const scene_entity& o) const
	{
		return !(*this == o);
	}

	inline bool operator==(entt::entity o) const
	{
		return handle == o;
	}

	entt::entity handle = entt::null;
	entt::registry* registry;
};

struct game_scene
{
	game_scene();

	scene_entity createEntity(const char* name)
	{
		return scene_entity(registry.create(), &registry)
			.addComponent<tag_component>(name);
	}

	scene_entity copyEntity(scene_entity src); // Source can be either from the same scene or from another.

	void deleteEntity(scene_entity e);
	void clearAll();

	template <typename component_t>
	void deleteAllComponents()
	{
		registry.clear<component_t>();
	}

	bool isEntityValid(scene_entity e)
	{
		return registry.valid(e.handle);
	}

	template <typename component_t>
	void copyComponentIfExists(scene_entity src, scene_entity dst)
	{
		if (component_t* comp = src.getComponentIfExists<component_t>())
		{
			dst.addComponent<component_t>(*comp);
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
		component_t** r = registry.view<component_t>().raw();
		return r ? *r : 0;
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

	template <typename context_t, typename... args>
	context_t& createOrGetContextVariable(args&&... a)
	{
		return registry.ctx_or_set<context_t>(std::forward<args>(a)...);
	}

	template <typename context_t>
	context_t& getContextVariable()
	{
		return registry.ctx<context_t>();
	}

	template <typename context_t>
	bool doesContextVariableExist()
	{
		return registry.try_ctx<context_t>() != 0;
	}

	template <typename context_t>
	void deleteContextVariable()
	{
		registry.unset<context_T>();
	}

	entt::registry registry;


	render_camera camera;
	directional_light sun;
	ref<pbr_environment> environment;
};

inline scene_entity::scene_entity(entt::entity handle, game_scene& scene) : handle(handle), registry(&scene.registry) {}
inline scene_entity::scene_entity(uint32 id, game_scene& scene) : handle((entt::entity)id), registry(&scene.registry) {}
