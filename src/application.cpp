#include "pch.h"
#include "application.h"
#include "geometry.h"
#include "dx_texture.h"
#include "random.h"
#include "color.h"
#include "imgui.h"
#include "dx_context.h"
#include "skinning.h"
#include "threading.h"
#include "mesh_shader.h"
#include "shadow_map_cache.h"
#include "file_dialog.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include <imgui/imstb_rectpack.h>

#include <filesystem>
namespace fs = std::filesystem;

struct raster_component
{
	ref<composite_mesh> mesh;
};

struct animation_component
{
	float time;

	uint32 animationIndex = 0;

	ref<dx_vertex_buffer> vb;
	submesh_info sms[16];

	ref<dx_vertex_buffer> prevFrameVB;
	submesh_info prevFrameSMs[16];
};

struct raytrace_component
{
	raytracing_object_type type;
};

static raytracing_object_type defineBlasFromMesh(const ref<composite_mesh>& mesh, path_tracer& pathTracer)
{
	if (dxContext.raytracingSupported)
	{
		raytracing_blas_builder blasBuilder;
		std::vector<ref<pbr_material>> raytracingMaterials;

		for (auto& sm : mesh->submeshes)
		{
			blasBuilder.push(mesh->mesh.vertexBuffer, mesh->mesh.indexBuffer, sm.info);
			raytracingMaterials.push_back(sm.material);
		}

		ref<raytracing_blas> blas = blasBuilder.finish();
		raytracing_object_type type = pathTracer.defineObjectType(blas, raytracingMaterials);
		return type;
	}
	else
	{
		return {};
	}
}

void application::loadCustomShaders()
{
	if (dxContext.meshShaderSupported)
	{
		initializeMeshShader();
	}
}

void application::initialize(dx_renderer* renderer)
{
	this->renderer = renderer;

	camera.initializeIngame(vec3(0.f, 30.f, 40.f), quat::identity, deg2rad(70.f), 0.1f);
	cameraController.initialize(&camera);

	if (dxContext.raytracingSupported)
	{
		pathTracer.initialize();
		raytracingTLAS.initialize();
	}
	
	auto sponzaMesh = loadMeshFromFile("assets/sponza/sponza.obj");
	if (sponzaMesh)
	{
		auto blas = defineBlasFromMesh(sponzaMesh, pathTracer);
	
		appScene.createEntity("Sponza")
			.addComponent<trs>(vec3(0.f, 0.f, 0.f), quat::identity, 0.01f)
			.addComponent<raster_component>(sponzaMesh)
			.addComponent<raytrace_component>(blas);
	}

#if 1
	auto stormtrooperMesh = loadAnimatedMeshFromFile("assets/stormtrooper/stormtrooper.fbx");
	auto pilotMesh = loadAnimatedMeshFromFile("assets/pilot/pilot.fbx");
	auto unrealMesh = loadAnimatedMeshFromFile("assets/unreal/unreal_mannequin.fbx");
	if (unrealMesh)
	{
		unrealMesh->skeleton.pushAssimpAnimationsInDirectory("assets/unreal/animations");
	}

	if (stormtrooperMesh)
	{
		appScene.createEntity("Stormtrooper 1")
			.addComponent<trs>(vec3(-5.f, 0.f, -1.f), quat::identity)
			.addComponent<raster_component>(stormtrooperMesh)
			.addComponent<animation_component>(1.5f);

		appScene.createEntity("Stormtrooper 2")
			.addComponent<trs>(vec3(0.f, 0.f, -2.f), quat::identity)
			.addComponent<raster_component>(stormtrooperMesh)
			.addComponent<animation_component>(0.f);

		appScene.createEntity("Stormtrooper 3")
			.addComponent<trs>(vec3(5.f, 0.f, -1.f), quat::identity)
			.addComponent<raster_component>(stormtrooperMesh)
			.addComponent<animation_component>(1.5f);
	}

	if (pilotMesh)
	{
		appScene.createEntity("Pilot")
			.addComponent<trs>(vec3(2.5f, 0.f, -1.f), quat::identity, 0.2f)
			.addComponent<raster_component>(pilotMesh)
			.addComponent<animation_component>(0.f);
	}

	if (unrealMesh)
	{
		appScene.createEntity("Mannequin")
			.addComponent<trs>(vec3(-2.5f, 0.f, -1.f), quat(vec3(1.f, 0.f, 0.f), deg2rad(-90.f)), 0.019f)
			.addComponent<raster_component>(unrealMesh)
			.addComponent<animation_component>(0.f);
	}
#endif


	// Raytracing.
	if (dxContext.raytracingSupported)
	{
		pathTracer.finish();
	}






	setEnvironment("assets/sky/sunset_in_the_chalk_quarry_4k.hdr");



	random_number_generator rng = { 14878213 };

	spotLights.resize(2);

	spotLights[0].initialize(
		{ 2.f, 3.f, 0.f },
		{ 1.f, 0.f, 0.f },
		randomRGB(rng) * 5.f,
		deg2rad(20.f),
		deg2rad(30.f),
		25.f,
		0
	);
	
	spotLights[1].initialize(
		{ -2.f, 3.f, 0.f },
		{ -1.f, 0.f, 0.f },
		randomRGB(rng) * 5.f,
		deg2rad(20.f),
		deg2rad(30.f),
		25.f,
		0
	);

	pointLights.resize(1);

	pointLights[0].initialize(
		{ 0.f, 8.f, 0.f },
		randomRGB(rng),
		10,
		0
	);

#if 1
	decalTexture = loadTextureFromFile("assets/decals/explosion.png");

	if (decalTexture)
	{
		decals.resize(5);

		for (uint32 i = 0; i < decals.size(); ++i)
		{
			decals[i].initialize(
				vec3(-7.f + i * 3, 2.f, -3.f),
				vec3(3.f, 0.f, 0.f),
				vec3(0.f, 3.f, 0.f),
				vec3(0.f, 0.f, -0.75f),
				vec4(1.f, 1.f, 1.f, 1.f),
				1.f,
				1.f,
				vec4(0.f, 0.f, 1.f, 1.f)
			);
		}

		SET_NAME(decalTexture->resource, "Decal");
	}
#endif


	sun.direction = normalize(vec3(-0.6f, -1.f, -0.3f));
	sun.color = vec3(1.f, 0.93f, 0.76f);
	sun.intensity = 50.f;

	sun.numShadowCascades = 3;
	sun.shadowDimensions = 2048;
	sun.cascadeDistances = vec4(9.f, 39.f, 74.f, 10000.f);
	sun.bias = vec4(0.000049f, 0.000114f, 0.000082f, 0.0035f);
	sun.blendDistances = vec4(3.f, 3.f, 10.f, 10.f);

	for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; ++i)
	{
		pointLightBuffer[i] = createUploadBuffer(sizeof(point_light_cb), 512, 0);
		spotLightBuffer[i] = createUploadBuffer(sizeof(spot_light_cb), 512, 0);
		decalBuffer[i] = createUploadBuffer(sizeof(pbr_decal_cb), 512, 0);

		spotLightShadowInfoBuffer[i] = createUploadBuffer(sizeof(spot_shadow_info), 512, 0);
		pointLightShadowInfoBuffer[i] = createUploadBuffer(sizeof(point_shadow_info), 512, 0);

		SET_NAME(pointLightBuffer[i]->resource, "Point lights");
		SET_NAME(spotLightBuffer[i]->resource, "Spot lights");
		SET_NAME(decalBuffer[i]->resource, "Decals");

		SET_NAME(spotLightShadowInfoBuffer[i]->resource, "Spot light shadow infos");
		SET_NAME(pointLightShadowInfoBuffer[i]->resource, "Point light shadow infos");
	}


	particleSystem.initialize(10000);
	particleSystem.color.initializeAsLinear(vec4(1.f, 0.f, 0.f, 1.f), vec4(0.8f, 0.8f, 0.1f, 1.f));
	particleSystem.maxLifetime.initializeAsConstant(0.5f);
	particleSystem.startVelocity.initializeAsRandom(vec3(-1.f, -1.f, -0.3f), vec3(1.f, 1.f, 0.3f));
	particleSystem.spawnRate = 2000.f;
	particleSystem.gravityFactor = 0.1f;
}

static bool plotAndEditTonemapping(tonemap_cb& tonemap)
{
	bool result = false;
	if (ImGui::TreeNode("Tonemapping"))
	{
		ImGui::PlotLines("Tone map",
			[](void* data, int idx)
			{
				float t = idx * 0.01f;
				tonemap_cb& aces = *(tonemap_cb*)data;

				return filmicTonemapping(t, aces);
			},
			&tonemap, 100, 0, 0, 0.f, 1.f, ImVec2(100.f, 100.f));

		result |= ImGui::SliderFloat("[ACES] Shoulder strength", &tonemap.A, 0.f, 1.f);
		result |= ImGui::SliderFloat("[ACES] Linear strength", &tonemap.B, 0.f, 1.f);
		result |= ImGui::SliderFloat("[ACES] Linear angle", &tonemap.C, 0.f, 1.f);
		result |= ImGui::SliderFloat("[ACES] Toe strength", &tonemap.D, 0.f, 1.f);
		result |= ImGui::SliderFloat("[ACES] Tone numerator", &tonemap.E, 0.f, 1.f);
		result |= ImGui::SliderFloat("[ACES] Toe denominator", &tonemap.F, 0.f, 1.f);
		result |= ImGui::SliderFloat("[ACES] Linear white", &tonemap.linearWhite, 0.f, 100.f);
		result |= ImGui::SliderFloat("[ACES] Exposure", &tonemap.exposure, -3.f, 3.f);

		ImGui::TreePop();
	}
	return result;
}

static bool editSunShadowParameters(directional_light& sun)
{
	bool result = false;
	if (ImGui::TreeNode("Sun"))
	{
		result |= ImGui::SliderFloat("Intensity", &sun.intensity, 50.f, 1000.f);
		result |= ImGui::ColorEdit3("Color", sun.color.data);
		result |= ImGui::SliderInt("# Cascades", (int*)&sun.numShadowCascades, 1, 4);

#define DISTANCE_SETTINGS	"Distance", sun.cascadeDistances.data, 0.f, 300.f
#define BIAS_SETTINGS		"Bias", sun.bias.data, 0.f, 0.005f, "%.6f"
#define BLEND_SETTINGS		"Blend distances", sun.blendDistances.data, 0.f, 50.f, "%.6f"

		if (sun.numShadowCascades == 1)
		{
			result |= ImGui::SliderFloat(DISTANCE_SETTINGS);
			result |= ImGui::SliderFloat(BIAS_SETTINGS);
			result |= ImGui::SliderFloat(BLEND_SETTINGS);
		}
		else if (sun.numShadowCascades == 2)
		{
			result |= ImGui::SliderFloat2(DISTANCE_SETTINGS);
			result |= ImGui::SliderFloat2(BIAS_SETTINGS);
			result |= ImGui::SliderFloat2(BLEND_SETTINGS);
		}
		else if (sun.numShadowCascades == 3)
		{
			result |= ImGui::SliderFloat3(DISTANCE_SETTINGS);
			result |= ImGui::SliderFloat3(BIAS_SETTINGS);
			result |= ImGui::SliderFloat3(BLEND_SETTINGS);
		}
		else if (sun.numShadowCascades == 4)
		{
			result |= ImGui::SliderFloat4(DISTANCE_SETTINGS);
			result |= ImGui::SliderFloat4(BIAS_SETTINGS);
			result |= ImGui::SliderFloat4(BLEND_SETTINGS);
		}

#undef DISTANCE_SETTINGS
#undef BIAS_SETTINGS
#undef BLEND_SETTINGS

		result |= ImGui::SliderFloat4("Blend distances", sun.blendDistances.data, 0.f, 50.f, "%.6f");

		ImGui::TreePop();
	}
	return result;
}

static bool editSSR(renderer_settings& settings)
{
	bool result = false;
	if (ImGui::TreeNode("SSR"))
	{
		result |= ImGui::Checkbox("Enable SSR", &settings.enableSSR);

		result |= ImGui::SliderInt("Num iterations", (int*)&settings.ssr.numSteps, 1, 1024);
		result |= ImGui::SliderFloat("Max distance", &settings.ssr.maxDistance, 5.f, 1000.f);
		result |= ImGui::SliderFloat("Min. stride", &settings.ssr.minStride, 1.f, 10.f);
		result |= ImGui::SliderFloat("Max. stride", &settings.ssr.maxStride, settings.ssr.minStride, 50.f);

		ImGui::TreePop();
	}
	return result;
}

static bool editPostProcessing(renderer_mode mode, renderer_settings& settings)
{
	bool result = false;
	if (ImGui::TreeNode("Post processing"))
	{
		if (mode == renderer_mode_rasterized)
		{
			result |= ImGui::Checkbox("Enable TAA", &settings.enableTemporalAntialiasing);
			if (settings.enableTemporalAntialiasing)
			{
				result |= ImGui::SliderFloat("Jitter strength", &settings.cameraJitterStrength, 0.f, 1.f);
			}

			result |= ImGui::Checkbox("Enable bloom", &settings.enableBloom);
			if (settings.enableBloom)
			{
				result |= ImGui::SliderFloat("Bloom threshold", &settings.bloomThreshold, 0.5f, 100.f);
				result |= ImGui::SliderFloat("Bloom strength", &settings.bloomStrength, 0.f, 1.f);
			}
		}

		result |= ImGui::Checkbox("Enable sharpen", &settings.enableSharpen);
		if (settings.enableSharpen)
		{
			result |= ImGui::SliderFloat("Sharpen strength", &settings.sharpenStrength, 0.f, 1.f);
		}

		ImGui::TreePop();
	}
	return result;
}

void application::setSelectedEntityEulerRotation()
{
	if (selectedEntity && selectedEntity.hasComponent<trs>())
	{
		selectedEntityEulerRotation = quatToEuler(selectedEntity.getComponent<trs>().rotation);
		selectedEntityEulerRotation.x = rad2deg(angleToZeroToTwoPi(selectedEntityEulerRotation.x));
		selectedEntityEulerRotation.y = rad2deg(angleToZeroToTwoPi(selectedEntityEulerRotation.y));
		selectedEntityEulerRotation.z = rad2deg(angleToZeroToTwoPi(selectedEntityEulerRotation.z));
	}
}

void application::setSelectedEntity(scene_entity entity)
{
	selectedEntity = entity;
	setSelectedEntityEulerRotation();
}

template<typename component_t, typename ui_func>
static void drawComponent(scene_entity entity, const char* componentName, ui_func func)
{
	const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;
	if (entity.hasComponent<component_t>())
	{
		auto& component = entity.getComponent<component_t>();
		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		float lineHeight = ImGui::GetIO().Fonts->Fonts[0]->FontSize + ImGui::GetStyle().FramePadding.y * 2.f;
		ImGui::Separator();
		bool open = ImGui::TreeNodeEx((void*)typeid(component_t).hash_code(), treeNodeFlags, componentName);
		ImGui::PopStyleVar();

		if (open)
		{
			func(component);
			ImGui::TreePop();
		}
	}
}

bool application::drawSceneHierarchy()
{
	bool objectMovedByWidget = false;

	if (ImGui::Begin("Scene Hierarchy"))
	{
		appScene.view<tag_component>()
			.each([this](auto entityHandle, tag_component& tag)
		{
			const char* name = tag.name;
			scene_entity entity = { entityHandle, appScene };

			if (entity == selectedEntity)
			{
				ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), name);
			}
			else
			{
				ImGui::Text(name);
			}

			if (ImGui::IsItemClicked(0) || ImGui::IsItemClicked(1))
			{
				setSelectedEntity(entity);
			}

			bool entityDeleted = false;
			if (ImGui::BeginPopupContextItem(name))
			{
				if (ImGui::MenuItem("Delete"))
					entityDeleted = true;

				ImGui::EndPopup();
			}

			if (entityDeleted)
			{
				appScene.deleteEntity(entity);
				setSelectedEntity({});
			}
		});

		ImGui::Separator();

		if (selectedEntity)
		{
			ImGui::AlignTextToFramePadding();

			ImGui::InputText("Name", selectedEntity.getComponent<tag_component>().name, sizeof(tag_component::name));
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_TRASH_ALT))
			{
				appScene.deleteEntity(selectedEntity);
				setSelectedEntity({});
				objectMovedByWidget = true;
			}
			else
			{
				drawComponent<trs>(selectedEntity, "Transform", [this, &objectMovedByWidget](trs& transform)
				{
					objectMovedByWidget |= ImGui::DragFloat3("Translation", transform.position.data, 0.1f, 0.f, 0.f);

					if (ImGui::DragFloat3("Rotation", selectedEntityEulerRotation.data, 0.1f, 0.f, 0.f))
					{
						vec3 euler = selectedEntityEulerRotation;
						euler.x = deg2rad(euler.x);
						euler.y = deg2rad(euler.y);
						euler.z = deg2rad(euler.z);
						transform.rotation = eulerToQuat(euler);

						objectMovedByWidget = true;
					}

					objectMovedByWidget |= ImGui::DragFloat3("Scale", transform.scale.data, 0.1f, 0.f, 0.f);
				});

				drawComponent<animation_component>(selectedEntity, "Animation", [this](animation_component& anim)
				{
					assert(selectedEntity.hasComponent<raster_component>());
					raster_component& raster = selectedEntity.getComponent<raster_component>();

					bool animationChanged = ImGui::Dropdown("Currently playing", [](uint32 index, void* data)
					{
						animation_skeleton& skeleton = *(animation_skeleton*)data;
						const char* result = 0;
						if (index < (uint32)skeleton.clips.size())
						{
							result = skeleton.clips[index].name.c_str();
						}
						return result;
					}, anim.animationIndex, &raster.mesh->skeleton);

					if (animationChanged)
					{
						anim.time = 0.f;
					}
				});
			}
		}
	}
	ImGui::End();

	return objectMovedByWidget;
}

void application::drawSettings(float dt)
{
	if (ImGui::Begin("Settings"))
	{
		ImGui::Text("%.3f ms, %u FPS", dt * 1000.f, (uint32)(1.f / dt));

		if (ImGui::Dropdown("Renderer mode", rendererModeNames, renderer_mode_count, (uint32&)renderer->mode))
		{
			pathTracer.numAveragedFrames = 0;
		}

		dx_memory_usage memoryUsage = dxContext.getMemoryUsage();

		ImGui::Text("Video memory available: %uMB", memoryUsage.available);
		ImGui::Text("Video memory used: %uMB", memoryUsage.currentlyUsed);

		ImGui::Dropdown("Aspect ratio", aspectRatioNames, aspect_ratio_mode_count, (uint32&)renderer->settings.aspectRatioMode);

		plotAndEditTonemapping(renderer->settings.tonemap);
		editSunShadowParameters(sun);
		editSSR(renderer->settings);
		editPostProcessing(renderer->mode, renderer->settings);

		ImGui::SliderFloat("Environment intensity", &renderer->settings.environmentIntensity, 0.f, 2.f);
		ImGui::SliderFloat("Sky intensity", &renderer->settings.skyIntensity, 0.f, 2.f);

		if (renderer->mode == renderer_mode_pathtraced)
		{
			bool pathTracerDirty = false;
			pathTracerDirty |= ImGui::SliderInt("Max recursion depth", (int*)&pathTracer.recursionDepth, 0, pathTracer.maxRecursionDepth - 1);
			pathTracerDirty |= ImGui::SliderInt("Start russian roulette after", (int*)&pathTracer.startRussianRouletteAfter, 0, pathTracer.recursionDepth);
			pathTracerDirty |= ImGui::Checkbox("Use thin lens camera", &pathTracer.useThinLensCamera);
			if (pathTracer.useThinLensCamera)
			{
				pathTracerDirty |= ImGui::SliderFloat("Focal length", &pathTracer.focalLength, 0.5f, 50.f);
				pathTracerDirty |= ImGui::SliderFloat("F-Number", &pathTracer.fNumber, 1.f, 128.f);
			}
			pathTracerDirty |= ImGui::Checkbox("Use real materials", &pathTracer.useRealMaterials);
			pathTracerDirty |= ImGui::Checkbox("Enable direct lighting", &pathTracer.enableDirectLighting);
			if (pathTracer.enableDirectLighting)
			{
				pathTracerDirty |= ImGui::SliderFloat("Light intensity scale", &pathTracer.lightIntensityScale, 0.f, 50.f);
				pathTracerDirty |= ImGui::SliderFloat("Point light radius", &pathTracer.pointLightRadius, 0.01f, 1.f);

				pathTracerDirty |= ImGui::Checkbox("Multiple importance sampling", &pathTracer.multipleImportanceSampling);
			}


			if (pathTracerDirty)
			{
				pathTracer.numAveragedFrames = 0;
			}
		}
	}

	ImGui::End();
}

void application::resetRenderPasses()
{
	opaqueRenderPass.reset();
	transparentRenderPass.reset();
	overlayRenderPass.reset();
	sunShadowRenderPass.reset();

	for (uint32 i = 0; i < numSpotShadowRenderPasses; ++i)
	{
		spotShadowRenderPasses[i].reset();
	}

	for (uint32 i = 0; i < numPointShadowRenderPasses; ++i)
	{
		pointShadowRenderPasses[i].reset();
	}

	numSpotShadowRenderPasses = 0;
	numPointShadowRenderPasses = 0;

	spotLightShadowInfos.clear();
	pointLightShadowInfos.clear();
}

void application::submitRenderPasses(uint32 numSpotLightShadowPasses, uint32 numPointLightShadowPasses)
{
	renderer->submitRenderPass(&opaqueRenderPass);
	renderer->submitRenderPass(&transparentRenderPass);
	renderer->submitRenderPass(&overlayRenderPass);
	renderer->submitRenderPass(&sunShadowRenderPass);

	for (uint32 i = 0; i < numSpotLightShadowPasses; ++i)
	{
		renderer->submitRenderPass(&spotShadowRenderPasses[i]);
	}

	for (uint32 i = 0; i < numPointLightShadowPasses; ++i)
	{
		renderer->submitRenderPass(&pointShadowRenderPasses[i]);
	}
}

bool application::handleUserInput(const user_input& input, float dt)
{
	// Returns true, if the user dragged an object using a gizmo.

	if (input.keyboard['F'].pressEvent && selectedEntity)
	{
		auto& raster = selectedEntity.getComponent<raster_component>();
		auto& transform = selectedEntity.getComponent<trs>();

		auto aabb = raster.mesh->aabb;
		aabb.minCorner *= transform.scale;
		aabb.maxCorner *= transform.scale;

		aabb.minCorner += transform.position;
		aabb.maxCorner += transform.position;

		cameraController.centerCameraOnObject(aabb);
	}

	bool inputCaptured = cameraController.update(input, renderer->renderWidth, renderer->renderHeight, dt);
	if (inputCaptured)
	{
		pathTracer.numAveragedFrames = 0;
	}

	bool objectMovedByGizmo = false;

	if (selectedEntity)
	{
		if (selectedEntity.hasComponent<trs>())
		{
			trs& transform = selectedEntity.getComponent<trs>();

			static transformation_type type = transformation_type_translation;
			static transformation_space space = transformation_global;
			if (manipulateTransformation(transform, type, space, camera, input, !inputCaptured, &overlayRenderPass))
			{
				setSelectedEntityEulerRotation();
				inputCaptured = true;
				objectMovedByGizmo = true;
			}
		}

		if (input.keyboard[key_backspace].pressEvent || input.keyboard[key_delete].pressEvent)
		{
			appScene.deleteEntity(selectedEntity);
			setSelectedEntity({});
			objectMovedByGizmo = true;
		}
	}

	if (!inputCaptured && input.mouse.left.clickEvent)
	{
		if (renderer->hoveredObjectID != -1)
		{
			setSelectedEntity({ renderer->hoveredObjectID, appScene });
		}
		else
		{
			setSelectedEntity({});
		}
		inputCaptured = true;
	}

	return objectMovedByGizmo;
}

void application::renderSunShadowMap(bool objectDragged)
{
	sun_shadow_render_pass& renderPass = sunShadowRenderPass;

	bool staticCacheAvailable = !objectDragged;

	for (uint32 i = 0; i < sun.numShadowCascades; ++i)
	{
		uint64 sunMovementHash = getLightMovementHash(sun, i);
		auto [vp, cache] = assignShadowMapViewport(i, sunMovementHash, sun.shadowDimensions);
		sunShadowRenderPass.viewports[i] = vp;

		staticCacheAvailable &= cache;
	}

	if (staticCacheAvailable)
	{
		renderPass.copyFromStaticCache = true;
	}
	else
	{
		renderStaticGeometryToSunShadowMap();
		renderPass.copyToStaticCache = true;
	}

	renderDynamicGeometryToSunShadowMap();
}

void application::renderShadowMap(spot_light_cb& spotLight, uint32 lightIndex, bool objectDragged)
{
	uint32 uniqueID = lightIndex + 1;

	int32 index = numSpotShadowRenderPasses++;
	spot_shadow_render_pass& renderPass = spotShadowRenderPasses[index];

	spotLight.shadowInfoIndex = index;
	renderPass.viewProjMatrix = getSpotLightViewProjectionMatrix(spotLight);

	auto [vp, staticCacheAvailable] = assignShadowMapViewport(uniqueID << 10, 0, 512);
	renderPass.viewport = vp;

	if (staticCacheAvailable && !objectDragged)
	{
		renderPass.copyFromStaticCache = true;
	}
	else
	{
		renderStaticGeometryToShadowMap(renderPass);
		renderPass.copyToStaticCache = true;
	}

	renderDynamicGeometryToShadowMap(renderPass);

	spot_shadow_info si;
	si.viewport = vec4(vp.x, vp.y, vp.size, vp.size) / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);
	si.viewProj = renderPass.viewProjMatrix;
	si.bias = 0.00002f;

	assert(spotLightShadowInfos.size() == index);
	spotLightShadowInfos.push_back(si);
}

void application::renderShadowMap(point_light_cb& pointLight, uint32 lightIndex, bool objectDragged)
{
	uint32 uniqueID = lightIndex + 1;

	int32 index = numPointShadowRenderPasses++;
	point_shadow_render_pass& renderPass = pointShadowRenderPasses[index];

	pointLight.shadowInfoIndex = index;
	renderPass.lightPosition = pointLight.position;
	renderPass.maxDistance = pointLight.radius;

	auto [vp0, staticCacheAvailable0] = assignShadowMapViewport((2 * uniqueID + 0) << 20, 0, 512);
	auto [vp1, staticCacheAvailable1] = assignShadowMapViewport((2 * uniqueID + 1) << 20, 0, 512);
	renderPass.viewport0 = vp0;
	renderPass.viewport1 = vp1;

	if (staticCacheAvailable0 && staticCacheAvailable1 && !objectDragged) // TODO
	{
		renderPass.copyFromStaticCache0 = true;
		renderPass.copyFromStaticCache1 = true;
	}
	else
	{
		renderStaticGeometryToShadowMap(renderPass);
		renderPass.copyToStaticCache0 = true;
		renderPass.copyToStaticCache1 = true;
	}

	renderDynamicGeometryToShadowMap(renderPass);


	point_shadow_info si;
	si.viewport0 = vec4(vp0.x, vp0.y, vp0.size, vp0.size) / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);
	si.viewport1 = vec4(vp1.x, vp1.y, vp1.size, vp1.size) / vec4((float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT, (float)SHADOW_MAP_WIDTH, (float)SHADOW_MAP_HEIGHT);

	assert(pointLightShadowInfos.size() == index);
	pointLightShadowInfos.push_back(si);
}

void application::renderStaticGeometryToSunShadowMap()
{
	sun_shadow_render_pass& renderPass = sunShadowRenderPass;

	for (auto [entityHandle, raster, transform] : appScene.group(entt::get<raster_component, trs>, entt::exclude<animation_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (auto& sm : raster.mesh->submeshes)
		{
			submesh_info submesh = sm.info;
			const ref<pbr_material>& material = sm.material;

			renderPass.renderStaticObject(0, mesh.vertexBuffer, mesh.indexBuffer, submesh, m);
		}
	}
}

void application::renderStaticGeometryToShadowMap(spot_shadow_render_pass& renderPass)
{
	for (auto [entityHandle, raster, transform] : appScene.group(entt::get<raster_component, trs>, entt::exclude<animation_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (auto& sm : raster.mesh->submeshes)
		{
			submesh_info submesh = sm.info;
			const ref<pbr_material>& material = sm.material;

			renderPass.renderStaticObject(mesh.vertexBuffer, mesh.indexBuffer, submesh, m);
		}
	}
}

void application::renderStaticGeometryToShadowMap(point_shadow_render_pass& renderPass)
{
	for (auto [entityHandle, raster, transform] : appScene.group(entt::get<raster_component, trs>, entt::exclude<animation_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (auto& sm : raster.mesh->submeshes)
		{
			submesh_info submesh = sm.info;
			const ref<pbr_material>& material = sm.material;

			renderPass.renderStaticObject(mesh.vertexBuffer, mesh.indexBuffer, submesh, m);
		}
	}
}

void application::renderDynamicGeometryToSunShadowMap()
{
	sun_shadow_render_pass& renderPass = sunShadowRenderPass;

	for (auto [entityHandle, raster, transform, anim] : appScene.group(entt::get<raster_component, trs, animation_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (uint32 i = 0; i < (uint32)raster.mesh->submeshes.size(); ++i)
		{
			submesh_info submesh = anim.sms[i];
			renderPass.renderDynamicObject(0, anim.vb, mesh.indexBuffer, submesh, m);
		}
	}
}

void application::renderDynamicGeometryToShadowMap(spot_shadow_render_pass& renderPass)
{
	for (auto [entityHandle, raster, transform, anim] : appScene.group(entt::get<raster_component, trs, animation_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (uint32 i = 0; i < (uint32)raster.mesh->submeshes.size(); ++i)
		{
			submesh_info submesh = anim.sms[i];
			renderPass.renderDynamicObject(anim.vb, mesh.indexBuffer, submesh, m);
		}
	}
}

void application::renderDynamicGeometryToShadowMap(point_shadow_render_pass& renderPass)
{
	for (auto [entityHandle, raster, transform, anim] : appScene.group(entt::get<raster_component, trs, animation_component>).each())
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		for (uint32 i = 0; i < (uint32)raster.mesh->submeshes.size(); ++i)
		{
			submesh_info submesh = anim.sms[i];
			renderPass.renderDynamicObject(anim.vb, mesh.indexBuffer, submesh, m);
		}
	}
}

void application::update(const user_input& input, float dt)
{
	resetRenderPasses();

	bool objectDragged = false;
	objectDragged |= handleUserInput(input, dt);
	objectDragged |= drawSceneHierarchy();
	drawSettings(dt);


	// Particles.
	static float particleSystemTime = 0.f;
	particleSystemTime += dt;

	particleSystem.spawnPosition.x = cos(particleSystemTime) * 5.f;
	particleSystem.spawnPosition.y = sin(particleSystemTime) * 4.f + 5.f;
	particleSystem.update(dt);
	particleSystem.render(camera, &transparentRenderPass);



	sun.updateMatrices(camera);

	// Set global rendering stuff.
	renderer->setCamera(camera);
	renderer->setSun(sun);
	renderer->setEnvironment(environment);


	if (renderer->mode == renderer_mode_rasterized)
	{
		if (dxContext.meshShaderSupported)
		{
			//testRenderMeshShader(&overlayRenderPass);
		}


		thread_job_context context;

		// Skin animated meshes.
		for (auto [entityHandle, anim, raster] : appScene.group(entt::get<animation_component, raster_component>).each())
		{
			anim.time += dt;
			const dx_mesh& mesh = raster.mesh->mesh;
			animation_skeleton& skeleton = raster.mesh->skeleton;

			auto [vb, vertexOffset, skinningMatrices] = skinObject(mesh.vertexBuffer, (uint32)skeleton.joints.size());

			mat4* mats = skinningMatrices;
			//context.addWork([&skeleton, &anim, mats]()
			//{
			trs localTransforms[128];
			skeleton.sampleAnimation(skeleton.clips[anim.animationIndex].name, anim.time, localTransforms);
			skeleton.getSkinningMatricesFromLocalTransforms(localTransforms, mats);
			//});

			anim.prevFrameVB = anim.vb;
			anim.vb = vb;

			uint32 numSubmeshes = (uint32)raster.mesh->submeshes.size();
			for (uint32 i = 0; i < numSubmeshes; ++i)
			{
				anim.prevFrameSMs[i] = anim.sms[i];

				anim.sms[i] = raster.mesh->submeshes[i].info;
				anim.sms[i].baseVertex += vertexOffset;
			}
		}


		renderSunShadowMap(objectDragged);

		for (uint32 i = 0; i < (uint32)spotLights.size(); ++i)
		{
			if (spotLights[i].shadowInfoIndex >= 0)
			{
				renderShadowMap(spotLights[i], i, objectDragged);
			}
		}

		for (uint32 i = 0; i < (uint32)pointLights.size(); ++i)
		{
			if (pointLights[i].shadowInfoIndex >= 0)
			{
				renderShadowMap(pointLights[i], i, objectDragged);
			}
		}



		// Submit render calls.
		for (auto [entityHandle, raster, transform] : appScene.group(entt::get<raster_component, trs>).each())
		{
			const dx_mesh& mesh = raster.mesh->mesh;
			mat4 m = trsToMat4(transform);

			scene_entity entity = { entityHandle, appScene };
			bool outline = selectedEntity == entity;

			if (entity.hasComponent<animation_component>())
			{
				auto& anim = entity.getComponent<animation_component>();

				uint32 numSubmeshes = (uint32)raster.mesh->submeshes.size();

				for (uint32 i = 0; i < numSubmeshes; ++i)
				{
					submesh_info submesh = anim.sms[i];
					submesh_info prevFrameSubmesh = anim.prevFrameSMs[i];

					const ref<pbr_material>& material = raster.mesh->submeshes[i].material;

					if (material->albedoTint.a < 1.f)
					{
						transparentRenderPass.renderObject(anim.vb, mesh.indexBuffer, submesh, material, m, outline);
					}
					else
					{
						opaqueRenderPass.renderAnimatedObject(anim.vb, anim.prevFrameVB, mesh.indexBuffer, submesh, prevFrameSubmesh, material, m, m,
							(uint32)entityHandle, outline);
					}
				}
			}
			else
			{
				for (auto& sm : raster.mesh->submeshes)
				{
					submesh_info submesh = sm.info;
					const ref<pbr_material>& material = sm.material;

					if (material->albedoTint.a < 1.f)
					{
						transparentRenderPass.renderObject(mesh.vertexBuffer, mesh.indexBuffer, submesh, material, m, outline);
					}
					else
					{
						opaqueRenderPass.renderStaticObject(mesh.vertexBuffer, mesh.indexBuffer, submesh, material, m, (uint32)entityHandle, outline);
					}
				}
			}
		}


		context.waitForWorkCompletion();
		submitRenderPasses(numSpotShadowRenderPasses, numPointShadowRenderPasses);



		// Upload and set lights.
		if (pointLights.size())
		{
			updateUploadBufferData(pointLightBuffer[dxContext.bufferedFrameID], pointLights.data(), (uint32)(sizeof(point_light_cb) * pointLights.size()));
			updateUploadBufferData(pointLightShadowInfoBuffer[dxContext.bufferedFrameID], pointLightShadowInfos.data(), (uint32)(sizeof(point_shadow_info) * numPointShadowRenderPasses));
			renderer->setPointLights(pointLightBuffer[dxContext.bufferedFrameID], (uint32)pointLights.size(), pointLightShadowInfoBuffer[dxContext.bufferedFrameID]);
		}
		if (spotLights.size())
		{
			updateUploadBufferData(spotLightBuffer[dxContext.bufferedFrameID], spotLights.data(), (uint32)(sizeof(spot_light_cb) * spotLights.size()));
			updateUploadBufferData(spotLightShadowInfoBuffer[dxContext.bufferedFrameID], spotLightShadowInfos.data(), (uint32)(sizeof(spot_shadow_info) * numSpotShadowRenderPasses));
			renderer->setSpotLights(spotLightBuffer[dxContext.bufferedFrameID], (uint32)spotLights.size(), spotLightShadowInfoBuffer[dxContext.bufferedFrameID]);
		}
		if (decals.size())
		{
			updateUploadBufferData(decalBuffer[dxContext.bufferedFrameID], decals.data(), (uint32)(sizeof(pbr_decal_cb) * decals.size()));
			renderer->setDecals(decalBuffer[dxContext.bufferedFrameID], (uint32)decals.size(), decalTexture);
		}
	}
	else
	{
		if (dxContext.raytracingSupported)
		{
			raytracingTLAS.reset();

			for (auto [entityHandle, raytrace, transform] : appScene.group(entt::get<raytrace_component, trs>).each())
			{
				raytracingTLAS.instantiate(raytrace.type, transform);
			}

			raytracingTLAS.build();

			renderer->setRaytracer(&pathTracer, &raytracingTLAS);
		}
	}
}

void application::setEnvironment(const std::string& filename)
{
	environment = createEnvironment(filename); // Currently synchronous (on render queue).
	pathTracer.numAveragedFrames = 0;

	if (!environment)
	{
		std::cout << "Could not load environment '" << filename << "'. Renderer will use procedural sky box. Procedural sky boxes currently cannot contribute to global illumnation, so expect very dark lighting.\n";
	}
}

void application::handleFileDrop(const std::string& filename)
{
	fs::path path = filename;
	fs::path relative = fs::relative(path, fs::current_path());

	auto mesh = loadMeshFromFile(relative.string());
	if (mesh)
	{
		appScene.createEntity("Dropped mesh")
			.addComponent<trs>(vec3(0.f), quat::identity)
			.addComponent<raster_component>(mesh);
	}
}




// Serialization stuff.

#include <yaml-cpp/yaml.h>
#include <fstream>


static YAML::Emitter& operator<<(YAML::Emitter& out, const vec2& v)
{
	out << YAML::Flow << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const vec3& v)
{
	out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const vec4& v)
{
	out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const quat& v)
{
	out << v.v4;
	return out;
}

namespace YAML
{
	template<> 
	struct convert<vec2>
	{
		static bool decode(const Node& n, vec2& v) { if (!n.IsSequence() || n.size() != 2) return false; v.x = n[0].as<float>(); v.y = n[1].as<float>(); return true; }
	};

	template<>
	struct convert<vec3>
	{
		static bool decode(const Node& n, vec3& v) { if (!n.IsSequence() || n.size() != 3) return false; v.x = n[0].as<float>(); v.y = n[1].as<float>(); v.z = n[2].as<float>(); return true; }
	};

	template<>
	struct convert<vec4>
	{
		static bool decode(const Node& n, vec4& v) { if (!n.IsSequence() || n.size() != 4) return false; v.x = n[0].as<float>(); v.y = n[1].as<float>(); v.z = n[2].as<float>(); v.w = n[3].as<float>(); return true; }
	};

	template<>
	struct convert<quat>
	{
		static bool decode(const Node& n, quat& v) { return convert<vec4>::decode(n, v.v4); }
	};
}

void application::serializeToFile()
{
	std::string filename = saveFileDialog("Scene files", "sc");
	if (filename.empty())
	{
		return;
	}

	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "Scene" << YAML::Value << "My scene";

	out << YAML::Key << "Camera"
		<< YAML::Value
		<< YAML::BeginMap
			<< YAML::Key << "Position" << YAML::Value << camera.position
			<< YAML::Key << "Rotation" << YAML::Value << camera.rotation
			<< YAML::Key << "Near Plane" << YAML::Value << camera.nearPlane
			<< YAML::Key << "Far Plane" << YAML::Value << camera.farPlane
			<< YAML::Key << "Type" << YAML::Value << camera.type;
	if (camera.type == camera_type_ingame)
	{
		out << YAML::Key << "FOV" << YAML::Value << camera.verticalFOV;
	}
	else
	{
		out << YAML::Key << "Fx" << YAML::Value << camera.fx
			<< YAML::Key << "Fy" << YAML::Value << camera.fy
			<< YAML::Key << "Cx" << YAML::Value << camera.cx
			<< YAML::Key << "Cy" << YAML::Value << camera.cy;
	}
	out	<< YAML::EndMap;


	out << YAML::Key << "Tone Map"
		<< YAML::Value
			<< YAML::BeginMap
				<< YAML::Key << "A" << YAML::Value << renderer->settings.tonemap.A
				<< YAML::Key << "B" << YAML::Value << renderer->settings.tonemap.B
				<< YAML::Key << "C" << YAML::Value << renderer->settings.tonemap.C
				<< YAML::Key << "D" << YAML::Value << renderer->settings.tonemap.D
				<< YAML::Key << "E" << YAML::Value << renderer->settings.tonemap.E
				<< YAML::Key << "F" << YAML::Value << renderer->settings.tonemap.F
				<< YAML::Key << "Linear White" << YAML::Value << renderer->settings.tonemap.linearWhite
				<< YAML::Key << "Exposure" << YAML::Value << renderer->settings.tonemap.exposure
			<< YAML::EndMap;


	out << YAML::Key << "Sun" 
		<< YAML::Value 
			<< YAML::BeginMap
				<< YAML::Key << "Color" << YAML::Value << sun.color
				<< YAML::Key << "Intensity" << YAML::Value << sun.intensity
				<< YAML::Key << "Direction" << YAML::Value << sun.direction
				<< YAML::Key << "Cascades" << YAML::Value << sun.numShadowCascades
				<< YAML::Key << "Distances" << YAML::Value << sun.cascadeDistances
				<< YAML::Key << "Bias" << YAML::Value << sun.bias
				<< YAML::Key << "Blend Distances" << YAML::Value << sun.blendDistances
			<< YAML::EndMap;


	out << YAML::Key << "Environment"
		<< YAML::Value
			<< YAML::BeginMap
				<< YAML::Key << "Name" << YAML::Value << environment->name
				<< YAML::Key << "Intensity" << renderer->settings.environmentIntensity
			<< YAML::EndMap;

	out << YAML::Key << "Entities"
		<< YAML::Value
		<< YAML::BeginSeq;

	appScene.forEachEntity([this, &out](entt::entity entityID)
	{
		scene_entity entity = { entityID, appScene };
		
		out << YAML::BeginMap;

		tag_component& tag = entity.getComponent<tag_component>();
		out << YAML::Key << "Tag" << YAML::Value << tag.name;

		if (entity.hasComponent<trs>())
		{
			trs& transform = entity.getComponent<trs>();
			out << YAML::Key << "Transform" << YAML::Value
				<< YAML::BeginMap
					<< YAML::Key << "Rotation" << YAML::Value << transform.rotation
					<< YAML::Key << "Position" << YAML::Value << transform.position
					<< YAML::Key << "Scale" << YAML::Value << transform.scale
				<< YAML::EndMap;
		}

		if (entity.hasComponent<raster_component>())
		{
			raster_component& raster = entity.getComponent<raster_component>();
			out << YAML::Key << "Raster" << YAML::Value
				<< YAML::BeginMap 
					<< YAML::Key << "Mesh" << YAML::Value << raster.mesh->filepath
					<< YAML::Key << "Flags" << YAML::Value << raster.mesh->flags
					<< YAML::Key << "Animation files" << YAML::Value << YAML::BeginSeq;

			for (const std::string& s : raster.mesh->skeleton.files)
			{
				out << s;
			}

			out		<< YAML::EndSeq
				<< YAML::EndMap;
		}

		if (entity.hasComponent<animation_component>())
		{
			animation_component& anim = entity.getComponent<animation_component>();
			out << YAML::Key << "Animation" << YAML::Value
				<< YAML::BeginMap 
					<< YAML::Key << "Time" << YAML::Value << anim.time
				<< YAML::EndMap;
		}

		out << YAML::EndMap;
	});

	out << YAML::EndSeq;

	out << YAML::EndMap;

	fs::create_directories(fs::path(filename).parent_path());

	std::ofstream fout(filename);
	fout << out.c_str();
}

bool application::deserializeFromFile()
{
	std::string filename = openFileDialog("Scene files", "sc");
	if (filename.empty())
	{
		return false;
	}

	std::ifstream stream(filename);
	YAML::Node data = YAML::Load(stream);
	if (!data["Scene"])
	{
		return false;
	}

	appScene = scene();

	std::string sceneName = data["Scene"].as<std::string>();

	auto cameraNode = data["Camera"];
	camera.position = cameraNode["Position"].as<vec3>();
	camera.rotation = cameraNode["Rotation"].as<quat>();
	camera.nearPlane = cameraNode["Near Plane"].as<float>();
	camera.farPlane = cameraNode["Far Plane"].as<float>();
	camera.type = (camera_type)cameraNode["Type"].as<int>();
	if (camera.type == camera_type_ingame)
	{
		camera.verticalFOV = cameraNode["FOV"].as<float>();
	}
	else
	{
		camera.fx = cameraNode["Fx"].as<float>();
		camera.fy = cameraNode["Fy"].as<float>();
		camera.cx = cameraNode["Cx"].as<float>();
		camera.cy = cameraNode["Cy"].as<float>();
	}

	auto tonemapNode = data["Tone Map"];
	renderer->settings.tonemap.A = tonemapNode["A"].as<float>();
	renderer->settings.tonemap.B = tonemapNode["B"].as<float>();
	renderer->settings.tonemap.C = tonemapNode["C"].as<float>();
	renderer->settings.tonemap.D = tonemapNode["D"].as<float>();
	renderer->settings.tonemap.E = tonemapNode["E"].as<float>();
	renderer->settings.tonemap.F = tonemapNode["F"].as<float>();
	renderer->settings.tonemap.linearWhite = tonemapNode["Linear White"].as<float>();
	renderer->settings.tonemap.exposure = tonemapNode["Exposure"].as<float>();

	auto sunNode = data["Sun"];
	sun.color = sunNode["Color"].as<decltype(sun.color)>();
	sun.intensity = sunNode["Intensity"].as<decltype(sun.intensity)>();
	sun.direction = sunNode["Direction"].as<decltype(sun.direction)>();
	sun.numShadowCascades = sunNode["Cascades"].as<decltype(sun.numShadowCascades)>();
	sun.cascadeDistances = sunNode["Distances"].as<decltype(sun.cascadeDistances)>();
	sun.bias = sunNode["Bias"].as<decltype(sun.bias)>();
	sun.blendDistances = sunNode["Blend Distances"].as<decltype(sun.blendDistances)>();

	auto environmentNode = data["Environment"];
	setEnvironment(environmentNode["Name"].as<std::string>());
	renderer->settings.environmentIntensity = environmentNode["Intensity"].as<float>();

	auto entitiesNode = data["Entities"];
	for (auto entityNode : entitiesNode)
	{
		std::string name = entityNode["Tag"].as<std::string>();
		scene_entity entity = appScene.createEntity(name.c_str());

		if (entityNode["Transform"])
		{
			auto transformNode = entityNode["Transform"];
			entity.addComponent<trs>(transformNode["Position"].as<vec3>(), transformNode["Rotation"].as<quat>(), transformNode["Scale"].as<vec3>());
		}

		if (entityNode["Raster"])
		{
			auto rasterNode = entityNode["Raster"];
			auto mesh = loadMeshFromFile(rasterNode["Mesh"].as<std::string>(), rasterNode["Flags"].as<uint32>());

			auto animationsNode = rasterNode["Animation files"];
			for (auto file : animationsNode)
			{
				mesh->skeleton.pushAssimpAnimations(file.as<std::string>());
			}

			entity.addComponent<raster_component>(mesh);
		}

		if (entityNode["Animation"])
		{
			auto animNode = entityNode["Animation"];
			entity.addComponent<animation_component>(animNode["Time"].as<float>());
		}
	}

	return true;
}
