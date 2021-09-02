#pragma once

#include "core/input.h"
#include "core/camera.h"
#include "core/camera_controller.h"
#include "geometry/mesh.h"
#include "core/math.h"
#include "rendering/main_renderer.h"
#include "rendering/light_source.h"
#include "scene/particle_systems.h"
#include "editor/transformation_gizmo.h"
#include "rendering/raytracing.h"
#include "physics/ragdoll.h"
#include "editor/undo_stack.h"

#include "rendering/path_tracing.h"

#include "scene/scene.h"



struct application
{
	void loadCustomShaders();
	void initialize(main_renderer* renderer);
	void update(const user_input& input, float dt);

	void setEnvironment(const std::string& filename);

	void handleFileDrop(const std::string& filename);
	void serializeToFile();
	bool deserializeFromFile();

private:
	void setSelectedEntityEulerRotation();
	void setSelectedEntity(scene_entity entity);
	void setSelectedEntityNoUndo(scene_entity entity);
	void drawMainMenuBar();
	bool drawSceneHierarchy();
	void drawSettings(float dt);

	void resetRenderPasses();
	void submitRenderPasses(uint32 numSpotLightShadowPasses, uint32 numPointLightShadowPasses);
	bool handleUserInput(const user_input& input, float dt);

	undo_stack undoStack;

	transformation_gizmo gizmo;

	raytracing_tlas raytracingTLAS;
	path_tracer pathTracer;

	ref<pbr_environment> environment;


	ref<dx_buffer> pointLightBuffer[NUM_BUFFERED_FRAMES];
	ref<dx_buffer> spotLightBuffer[NUM_BUFFERED_FRAMES];
	ref<dx_buffer> decalBuffer[NUM_BUFFERED_FRAMES];

	ref<dx_buffer> spotLightShadowInfoBuffer[NUM_BUFFERED_FRAMES];
	ref<dx_buffer> pointLightShadowInfoBuffer[NUM_BUFFERED_FRAMES];


	std::vector<point_light_cb> pointLights;
	std::vector<spot_light_cb> spotLights;
	std::vector<pbr_decal_cb> decals;

	std::vector<spot_shadow_info> spotLightShadowInfos;
	std::vector<point_shadow_info> pointLightShadowInfos;

	ref<dx_texture> decalTexture;

	fire_particle_system fireParticleSystem;
	smoke_particle_system smokeParticleSystem;
	boid_particle_system boidParticleSystem;

	directional_light sun;

	main_renderer* renderer;

	render_camera camera;
	camera_controller cameraController;

	scene appScene;
	scene_entity selectedEntity;
	vec3 selectedEntityEulerRotation;


	uint32 numSpotShadowRenderPasses;
	uint32 numPointShadowRenderPasses;

	humanoid_ragdoll ragdoll;
	float testPhysicsForce = 300.f;

	physics_settings physicsSettings;

	opaque_render_pass opaqueRenderPass;
	transparent_render_pass transparentRenderPass;
	sun_shadow_render_pass sunShadowRenderPass;
	spot_shadow_render_pass spotShadowRenderPasses[16];
	point_shadow_render_pass pointShadowRenderPasses[16];
	overlay_render_pass overlayRenderPass;
	outline_render_pass outlineRenderPass;



	friend void undoSelection(void* d);
	friend void redoSelection(void* d);
};
