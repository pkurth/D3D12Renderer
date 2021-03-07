#pragma once

//#define ENTT_ID_TYPE uint64
//#include <entt/entt.hpp>
#include <entt/entity/registry.hpp>


struct tag_component
{
	char name[16];

	tag_component(const char* n)
	{
		strncpy(name, n, sizeof(name));
		name[sizeof(name) - 1] = 0;
	}
};

struct scene_entity
{
	scene_entity() = default;
	inline scene_entity(entt::entity handle, struct scene& scene);
	inline scene_entity(uint32 id, struct scene& scene);
	scene_entity(const scene_entity&) = default;

	template <typename component_t>
	bool hasComponent()
	{
		return registry->any_of<component_t>(handle);
	}

	template <typename component_t, typename... args>
	scene_entity& addComponent(args&&... a)
	{
		registry->emplace<component_t>(handle, std::forward<args>(a)...);
		return *this;
	}

	template <typename component_t>
	component_t& getComponent()
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

	inline bool operator==(entt::entity o)
	{
		return handle == o;
	}

private:
	entt::entity handle = entt::null;
	entt::registry* registry;


	scene_entity(entt::entity handle, entt::registry* registry) : handle(handle), registry(registry) {}

	friend struct scene;
};

struct scene
{
	scene_entity createEntity(const char* name)
	{
		return scene_entity(registry.create(), &registry)
			.addComponent<tag_component>(name);
	}

	void deleteEntity(scene_entity e)
	{
		registry.destroy(e.handle);
	}

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
	auto group(entt::get_t<Get...>, entt::exclude_t<Exclude...> = {})
	{
		return registry.group<owned_component_t...>(entt::get<Get...>, entt::exclude<Exclude...>);
	}

	template <typename func_t>
	void forEachEntity(func_t func)
	{
		registry.each(func);
	}

private:
	entt::registry registry;

	friend scene_entity;
};

inline scene_entity::scene_entity(entt::entity handle, struct scene& scene) : handle(handle), registry(&scene.registry) {}
inline scene_entity::scene_entity(uint32 id, struct scene& scene) : handle((entt::entity)id), registry(&scene.registry) {}



