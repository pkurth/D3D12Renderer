#pragma once

#define ENTT_ID_TYPE uint64
#include <entt/entt.hpp>


struct scene_entity
{
	scene_entity(const scene_entity&) = default;

	template <typename component_t>
	bool hasComponent()
	{
		return registry->has<component_t>(handle);
	}

	template <typename component_t, typename... args>
	scene_entity& addComponent(args&&... a)
	{
		registry->emplace<component_t>(handle, std::forward<args>(a)...);
		return this;
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

private:
	entt::entity handle;
	entt::registry* registry;


	scene_entity(entt::entity handle, entt::registry* registry) : handle(handle), registry(registry) {}

	friend struct scene;
};

struct scene
{
	scene_entity createEntity();

	template <typename... component_t>
	auto view() { return registry.view<component_t...>(); }

private:
	entt::registry registry;
};



