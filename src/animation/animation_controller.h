#pragma once

#include "animation.h"
#include "scene/scene.h"
#include "geometry/mesh.h"
#include "rendering/material.h"

struct animation_component
{
	ref<struct animation_controller> controller;
};

struct ragdoll_component
{
	std::vector<scene_entity> rigidbodies;
};

enum animation_controller_type
{
	animation_controller_type_simple,
	animation_controller_type_ragdoll,
	animation_controller_type_random_path,
};


struct animation_controller
{
	virtual void update(scene_entity entity, float dt) = 0;
	virtual void edit(scene_entity entity) {}

	material_vertex_buffer_group_view currentVertexBuffer;
	material_vertex_buffer_group_view prevFrameVertexBuffer;

	animation_controller_type type;

protected:
	mat4* allocateSkin(ref<composite_mesh> mesh);
	void defaultVertexBuffer(ref<composite_mesh> mesh);
};

struct simple_animation_controller : animation_controller
{
	simple_animation_controller() { type = animation_controller_type_simple; }

	virtual void update(scene_entity entity, float dt) override;
	virtual void edit(scene_entity entity) override;

	float timeScale = 1.f;
	uint32 selectedClipIndex = 0;
	float transitionTime = 0.1f;

	animation_player animationPlayer;
};

struct ragdoll_animation_controller : animation_controller
{
	ragdoll_animation_controller() { type = animation_controller_type_ragdoll; }
	virtual void update(scene_entity entity, float dt) override;
};

struct random_path_animation_controller : animation_controller 
{
	random_path_animation_controller() { type = animation_controller_type_random_path; }

	virtual void update(scene_entity entity, float dt) override;
	virtual void edit(scene_entity entity) override;
	float timeScale = 1.f;

	animation_player animationPlayer;
};

