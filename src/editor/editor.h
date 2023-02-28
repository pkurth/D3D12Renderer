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
	struct undo_buffer
	{
		uint8 before[128];

		template <typename T> inline T& as() { return *(T*)before; }
	};


	scene_entity selectedColliderEntity;
	scene_entity selectedConstraintEntity;

	void drawSettings(float dt);
	bool drawMainMenuBar();
	bool drawSceneHierarchy();
	bool handleUserInput(const user_input& input, ldr_render_pass* ldrRenderPass, float dt);
	bool drawEntityCreationPopup();

	void updateSelectedEntityUIRotation();

	void setSelectedEntity(scene_entity entity);
	void setSelectedEntityNoUndo(scene_entity entity);


	bool editCamera(render_camera& camera);
	bool editTonemapping(tonemap_settings& tonemap);
	bool editSunShadowParameters(directional_light& sun);
	bool editAO(bool& enable, hbao_settings& settings, const ref<dx_texture>& aoTexture);
	bool editSSS(bool& enable, sss_settings& settings, const ref<dx_texture>& sssTexture);
	bool editSSR(bool& enable, ssr_settings& settings, const ref<dx_texture>& ssrTexture);
	bool editTAA(bool& enable, taa_settings& settings, const ref<dx_texture>& velocityTexture);
	bool editBloom(bool& enable, bloom_settings& settings, const ref<dx_texture>& bloomTexture);
	bool editSharpen(bool& enable, sharpen_settings& settings);

	void onObjectMoved();

	void serializeToFile();
	bool deserializeFromFile();

	editor_scene* scene;
	main_renderer* renderer;
	editor_panels* editorPanels;

	undo_stack undoStack;
	undo_buffer undoBuffer;
	transformation_gizmo gizmo;

	float physicsTestForce = 1000.f;

	camera_controller cameraController;

	vec3 selectedEntityEulerRotation;

	system_info systemInfo;

	friend struct selection_undo;
};
