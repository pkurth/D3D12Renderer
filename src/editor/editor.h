#pragma once

#include "scene/scene.h"
#include "core/camera_controller.h"
#include "core/system.h"
#include "undo_stack.h"
#include "transformation_gizmo.h"
#include "asset_editor_panel.h"
#include "rendering/main_renderer.h"
#include "physics/physics.h"

struct scene_editor
{
	void initialize(editor_scene* scene, main_renderer* renderer, editor_panels* editorPanels);

	bool update(const user_input& input, ldr_render_pass* ldrRenderPass, float dt);

	scene_entity selectedEntity;
	physics_settings physicsSettings;

private:
	scene_entity selectedColliderEntity;

	void drawSettings(float dt);
	void drawMainMenuBar();
	bool drawSceneHierarchy();
	bool handleUserInput(const user_input& input, ldr_render_pass* ldrRenderPass, float dt);
	bool drawEntityCreationPopup();

	void updateSelectedEntityUIRotation();

	void setSelectedEntity(scene_entity entity);
	void setSelectedEntityNoUndo(scene_entity entity);

	void serializeToFile();
	bool deserializeFromFile();

	editor_scene* scene;
	main_renderer* renderer;
	editor_panels* editorPanels;

	undo_stack undoStack;
	transformation_gizmo gizmo;

	float physicsTestForce = 1000.f;

	camera_controller cameraController;

	vec3 selectedEntityEulerRotation;

	system_info systemInfo;

	friend struct selection_undo;
};
