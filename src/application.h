#pragma once

#include "input.h"
#include "camera.h"
#include "camera_controller.h"
#include "mesh.h"
#include "math.h"
#include "dx_renderer.h"
#include "light_source.h"
#include "particle_systems.h"
#include "transformation_gizmo.h"
#include "raytracing.h"

#include "path_tracing.h"

#include "scene.h"



struct application
{
	void loadCustomShaders();
	void initialize(dx_renderer* renderer);
	void update(const user_input& input, float dt);

	void setEnvironment(const std::string& filename);

	void handleFileDrop(const std::string& filename);
	void serializeToFile();
	bool deserializeFromFile();

private:
	void setSelectedEntityEulerRotation();
	void setSelectedEntity(scene_entity entity);
	bool drawSceneHierarchy();
	void drawSettings(float dt);

	void resetRenderPasses();
	void submitRenderPasses(uint32 numSpotLightShadowPasses, uint32 numPointLightShadowPasses);
	bool handleUserInput(const user_input& input, float dt);

	void renderSunShadowMap(bool objectDragged);
	void renderShadowMap(spot_light_cb& spotLight, uint32 lightIndex, bool objectDragged);
	void renderShadowMap(point_light_cb& pointLight, uint32 lightIndex, bool objectDragged);

	void renderStaticGeometryToSunShadowMap();
	void renderStaticGeometryToShadowMap(spot_shadow_render_pass& renderPass);
	void renderStaticGeometryToShadowMap(point_shadow_render_pass& renderPass);

	void renderDynamicGeometryToSunShadowMap();
	void renderDynamicGeometryToShadowMap(spot_shadow_render_pass& renderPass);
	void renderDynamicGeometryToShadowMap(point_shadow_render_pass& renderPass);
	

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

	dx_renderer* renderer;

	render_camera camera;
	camera_controller cameraController;

	scene appScene;
	scene_entity selectedEntity;
	vec3 selectedEntityEulerRotation;


	uint32 numSpotShadowRenderPasses;
	uint32 numPointShadowRenderPasses;

	uint32 numPhysicsSolverIterations = 30;

	opaque_render_pass opaqueRenderPass;
	transparent_render_pass transparentRenderPass;
	sun_shadow_render_pass sunShadowRenderPass;
	spot_shadow_render_pass spotShadowRenderPasses[16];
	point_shadow_render_pass pointShadowRenderPasses[16];
	overlay_render_pass overlayRenderPass;
};
