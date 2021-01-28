#include "pch.h"
#include "application.h"
#include "geometry.h"
#include "dx_texture.h"
#include "random.h"
#include "color.h"
#include "imgui.h"
#include "dx_context.h"
#include "skinning.h"
#include "mesh_shader.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include <imgui/imstb_rectpack.h>

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

static ref<dx_buffer> pointLightBuffer[NUM_BUFFERED_FRAMES];
static ref<dx_buffer> spotLightBuffer[NUM_BUFFERED_FRAMES];

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

	
	// Sponza.
	auto sponzaMesh = loadMeshFromFile("assets/meshes/sponza.obj");
	if (sponzaMesh)
	{
		auto sponzaBlas = defineBlasFromMesh(sponzaMesh, pathTracer);

		appScene.createEntity("Sponza")
			.addComponent<trs>(vec3(0.f, 0.f, 0.f), quat::identity, 0.01f)
			.addComponent<raster_component>(sponzaMesh)
			.addComponent<raytrace_component>(sponzaBlas);
	}

#if 1
	// Stormtrooper.
	auto stormtrooperMesh = loadAnimatedMeshFromFile("assets/meshes/stormtrooper.fbx");
	if (stormtrooperMesh)
	{
		stormtrooperMesh->submeshes[0].material = createPBRMaterial(
			"assets/textures/stormtrooper/Stormtrooper_D.png",
			0, 0, 0,
			vec4(0.f),
			vec4(1.f),
			0.f,
			0.f
		);
	}

	// Pilot.
	auto pilotMesh = loadAnimatedMeshFromFile("assets/meshes/pilot.fbx");
	if (pilotMesh)
	{
		pilotMesh->submeshes[0].material = createPBRMaterial(
			"assets/textures/pilot/A.png",
			"assets/textures/pilot/N.png",
			"assets/textures/pilot/R.png",
			"assets/textures/pilot/M.png"
		);
	}

	// Unreal Mannequin.
	auto unrealMesh = loadAnimatedMeshFromFile("assets/meshes/unreal_mannequin.fbx");
	if (unrealMesh)
	{
		unrealMesh->skeleton.pushAssimpAnimationsInDirectory("assets/animations");
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






	setEnvironment("assets/textures/hdri/sunset_in_the_chalk_quarry_4k.hdr");



	random_number_generator rng = { 14878213 };

	spotLights.resize(2);

	spotLights[0] =
	{
		{ 2.f, 3.f, 0.f }, // Position.
		packInnerAndOuterCutoff(cos(deg2rad(20.f)), cos(deg2rad(30.f))),
		{ 1.f, 0.f, 0.f }, // Direction.
		25.f, // Max distance.
		randomRGB(rng) * 5.f,
		0 // Shadow info index.
	};

	spotLights[1] =
	{
		{ -2.f, 3.f, 0.f }, // Position.
		packInnerAndOuterCutoff(cos(deg2rad(20.f)), cos(deg2rad(30.f))),
		{ -1.f, 0.f, 0.f }, // Direction.
		25.f, // Max distance.
		randomRGB(rng) * 5.f,
		1 // Shadow info index.
	};

	pointLights.resize(1);

	pointLights[0] =
	{
		{ 0.f, 8.f, 0.f }, // Position.
		20, // Radius.
		randomRGB(rng),
		0 // Shadow info index.
	};


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

		SET_NAME(pointLightBuffer[i]->resource, "Point lights");
		SET_NAME(spotLightBuffer[i]->resource, "Spot lights");
	}
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

void application::setSelectedEntity(scene_entity entity)
{
	selectedEntity = entity;
	if (entity && entity.hasComponent<trs>())
	{
		selectedEntityEulerRotation = quatToEuler(entity.getComponent<trs>().rotation);
		selectedEntityEulerRotation.x = rad2deg(angleToZeroToTwoPi(selectedEntityEulerRotation.x));
		selectedEntityEulerRotation.y = rad2deg(angleToZeroToTwoPi(selectedEntityEulerRotation.y));
		selectedEntityEulerRotation.z = rad2deg(angleToZeroToTwoPi(selectedEntityEulerRotation.z));
	}
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

void application::drawSceneHierarchy()
{
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

			ImGui::Text(selectedEntity.getComponent<tag_component>().name);

			drawComponent<trs>(selectedEntity, "Transform", [this](trs& transform)
			{
				ImGui::DragFloat3("Translation", transform.position.data, 0.1f, 0.f, 0.f);

				if (ImGui::DragFloat3("Rotation", selectedEntityEulerRotation.data, 0.1f, 0.f, 0.f))
				{
					vec3 euler = selectedEntityEulerRotation;
					euler.x = deg2rad(euler.x);
					euler.y = deg2rad(euler.y);
					euler.z = deg2rad(euler.z);
					transform.rotation = eulerToQuat(euler);
				}

				ImGui::DragFloat3("Scale", transform.scale.data, 0.1f, 0.f, 0.f);
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
	ImGui::End();
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

		ImGui::SliderFloat("Environment intensity", &renderer->settings.environmentIntensity, 0.f, 2.f);
		ImGui::SliderFloat("Sky intensity", &renderer->settings.skyIntensity, 0.f, 2.f);

		ImGui::Checkbox("Enable TAA", &renderer->settings.enableTemporalAntialiasing);


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
			pathTracerDirty |= ImGui::SliderFloat("Light intensity scale", &pathTracer.lightIntensityScale, 0.f, 50.f);
			pathTracerDirty |= ImGui::SliderFloat("Point light radius", &pathTracer.pointLightRadius, 0.f, 1.f);

			pathTracerDirty |= ImGui::Checkbox("Multiple importance sampling", &pathTracer.multipleImportanceSampling);

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
	overlayRenderPass.reset();
	sunShadowRenderPass.reset();

	for (uint32 i = 0; i < arraysize(spotShadowRenderPasses); ++i)
	{
		spotShadowRenderPasses[i].reset();
	}

	for (uint32 i = 0; i < arraysize(pointShadowRenderPasses); ++i)
	{
		pointShadowRenderPasses[i].reset();
	}
}

void application::submitRenderPasses()
{
	renderer->submitRenderPass(&opaqueRenderPass);
	renderer->submitRenderPass(&overlayRenderPass);
	renderer->submitRenderPass(&sunShadowRenderPass);

	for (uint32 i = 0; i < arraysize(spotShadowRenderPasses); ++i)
	{
		renderer->submitRenderPass(&spotShadowRenderPasses[i]);
	}

	for (uint32 i = 0; i < arraysize(pointShadowRenderPasses); ++i)
	{
		renderer->submitRenderPass(&pointShadowRenderPasses[i]);
	}
}

void application::handleUserInput(const user_input& input, float dt)
{
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

	if (selectedEntity && selectedEntity.hasComponent<trs>())
	{
		trs& transform = selectedEntity.getComponent<trs>();

		static transformation_type type = transformation_type_translation;
		static transformation_space space = transformation_global;
		inputCaptured |= manipulateTransformation(transform, type, space, camera, input, !inputCaptured, &overlayRenderPass);
	}

	if (!inputCaptured && input.mouse.left.clickEvent)
	{
		if (renderer->hoveredObjectID != 0xFFFF)
		{
			setSelectedEntity({ renderer->hoveredObjectID, appScene });
		}
		else
		{
			setSelectedEntity({});
		}
		inputCaptured = true;
	}
}

void application::assignShadowMapViewports()
{
	stbrp_node* nodes = (stbrp_node*)alloca(sizeof(stbrp_node) * 1024);

	stbrp_context packContext;
	stbrp_init_target(&packContext, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, nodes, 1024);

	stbrp_rect rects[4 + arraysize(spotShadowRenderPasses) + 2 * arraysize(pointShadowRenderPasses)];

	uint32 rectIndex = 0;
	for (uint32 i = 0; i < sun.numShadowCascades; ++i)
	{
		stbrp_rect& r = rects[rectIndex++];
		r.w = r.h = sun.shadowDimensions;
	}
	for (uint32 i = 0; i < arraysize(spotShadowRenderPasses); ++i)
	{
		stbrp_rect& r = rects[rectIndex++];
		r.w = r.h = 2048;
	}
	for (uint32 i = 0; i < arraysize(pointShadowRenderPasses); ++i)
	{
		stbrp_rect& r0 = rects[rectIndex++];
		stbrp_rect& r1 = rects[rectIndex++];
		r0.w = r0.h = r1.w = r1.h = 2048;
	}

	int result = stbrp_pack_rects(&packContext, rects, rectIndex);
	assert(result);

	rectIndex = 0;
	for (uint32 i = 0; i < sun.numShadowCascades; ++i)
	{
		stbrp_rect& r = rects[rectIndex++];
		sunShadowRenderPass.viewports[i] = { (float)r.x, (float)r.y, (float)r.w, (float)r.h };
	}
	for (uint32 i = 0; i < arraysize(spotShadowRenderPasses); ++i)
	{
		stbrp_rect& r = rects[rectIndex++];
		spotShadowRenderPasses[i].viewport = { (float)r.x, (float)r.y, (float)r.w, (float)r.h };
	}
	for (uint32 i = 0; i < arraysize(pointShadowRenderPasses); ++i)
	{
		stbrp_rect& r0 = rects[rectIndex++];
		stbrp_rect& r1 = rects[rectIndex++];
		pointShadowRenderPasses[i].viewport0 = { (float)r0.x, (float)r0.y, (float)r0.w, (float)r0.h };
		pointShadowRenderPasses[i].viewport1 = { (float)r1.x, (float)r1.y, (float)r1.w, (float)r1.h };
	}
}

void application::update(const user_input& input, float dt)
{
	resetRenderPasses();
	handleUserInput(input, dt);

	drawSceneHierarchy();
	drawSettings(dt);



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


		spotShadowRenderPasses[0].viewProjMatrix = getSpotLightViewProjectionMatrix(spotLights[0]);
		spotShadowRenderPasses[1].viewProjMatrix = getSpotLightViewProjectionMatrix(spotLights[1]);
		pointShadowRenderPasses[0].lightPosition = pointLights[0].position;
		pointShadowRenderPasses[0].maxDistance = pointLights[0].radius;

		assignShadowMapViewports();



		// Upload and set lights.
		if (pointLights.size())
		{
			updateUploadBufferData(pointLightBuffer[dxContext.bufferedFrameID], pointLights.data(), (uint32)(sizeof(point_light_cb) * pointLights.size()));
			renderer->setPointLights(pointLightBuffer[dxContext.bufferedFrameID], (uint32)pointLights.size());
		}
		if (spotLights.size())
		{
			updateUploadBufferData(spotLightBuffer[dxContext.bufferedFrameID], spotLights.data(), (uint32)(sizeof(spot_light_cb) * spotLights.size()));
			renderer->setSpotLights(spotLightBuffer[dxContext.bufferedFrameID], (uint32)spotLights.size());
		}


		// Skin animated meshes.
		appScene.group<animation_component>(entt::get<raster_component>)
			.each([dt](animation_component& anim, raster_component& raster)
		{
			anim.time += dt;
			const dx_mesh& mesh = raster.mesh->mesh;
			const animation_skeleton& skeleton = raster.mesh->skeleton;

			trs localTransforms[128];
			auto [vb, vertexOffset, skinningMatrices] = skinObject(mesh.vertexBuffer, (uint32)skeleton.joints.size());
			skeleton.sampleAnimation(skeleton.clips[anim.animationIndex].name, anim.time, localTransforms);
			skeleton.getSkinningMatricesFromLocalTransforms(localTransforms, skinningMatrices);

			anim.prevFrameVB = anim.vb;
			anim.vb = vb;

			uint32 numSubmeshes = (uint32)raster.mesh->submeshes.size();
			for (uint32 i = 0; i < numSubmeshes; ++i)
			{
				anim.prevFrameSMs[i] = anim.sms[i];

				anim.sms[i] = raster.mesh->submeshes[i].info;
				anim.sms[i].baseVertex += vertexOffset;
			}
		});

		// Submit render calls.
		appScene.group<raster_component>(entt::get<trs>)
			.each([this](entt::entity entityHandle, raster_component& raster, trs& transform)
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

					opaqueRenderPass.renderAnimatedObject(anim.vb, anim.prevFrameVB, mesh.indexBuffer, submesh, prevFrameSubmesh, raster.mesh->submeshes[i].material, m, m,
						(uint32)entityHandle, outline);
					sunShadowRenderPass.renderObject(0, anim.vb, mesh.indexBuffer, submesh, m);
					spotShadowRenderPasses[0].renderObject(anim.vb, mesh.indexBuffer, submesh, m);
					spotShadowRenderPasses[1].renderObject(anim.vb, mesh.indexBuffer, submesh, m);
					pointShadowRenderPasses[0].renderObject(anim.vb, mesh.indexBuffer, submesh, m);
				}
			}
			else
			{
				for (auto& sm : raster.mesh->submeshes)
				{
					submesh_info submesh = sm.info;
					const ref<pbr_material>& material = sm.material;

					opaqueRenderPass.renderStaticObject(mesh.vertexBuffer, mesh.indexBuffer, submesh, material, m, (uint32)entityHandle, outline);
					sunShadowRenderPass.renderObject(0, mesh.vertexBuffer, mesh.indexBuffer, submesh, m);
					spotShadowRenderPasses[0].renderObject(mesh.vertexBuffer, mesh.indexBuffer, submesh, m);
					spotShadowRenderPasses[1].renderObject(mesh.vertexBuffer, mesh.indexBuffer, submesh, m);
					pointShadowRenderPasses[0].renderObject(mesh.vertexBuffer, mesh.indexBuffer, submesh, m);
				}
			}
		});

		submitRenderPasses();
	}
	else
	{
		if (dxContext.raytracingSupported)
		{
			raytracingTLAS.reset();

			appScene.group<raytrace_component>(entt::get<trs>)
				.each([this](entt::entity entityHandle, raytrace_component& raytrace, trs& transform)
			{
				raytracingTLAS.instantiate(raytrace.type, transform);
			});

			raytracingTLAS.build();

			renderer->setRaytracer(&pathTracer, &raytracingTLAS);
		}
	}

}

void application::setEnvironment(const char* filename)
{
	environment = createEnvironment(filename); // Currently synchronous (on render queue).
	pathTracer.numAveragedFrames = 0;

	if (!environment)
	{
		std::cout << "Could not load environment '" << filename << "'. Renderer will use procedural sky box. Procedural sky boxes currently cannot contribute to global illumnation, so expect very dark lighting." << std::endl;
	}
}




// Serialization stuff.

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;


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
		static Node encode(const vec2& v) { Node n; n.push_back(v.x); n.push_back(v.y); return n; }
		static bool decode(const Node& n, vec2& v) { if (!n.IsSequence() || n.size() != 2) return false; v.x = n[0].as<float>(); v.y = n[1].as<float>(); return true; }
	};

	template<>
	struct convert<vec3>
	{
		static Node encode(const vec3& v) { Node n; n.push_back(v.x); n.push_back(v.y); n.push_back(v.z); return n; }
		static bool decode(const Node& n, vec3& v) { if (!n.IsSequence() || n.size() != 3) return false; v.x = n[0].as<float>(); v.y = n[1].as<float>(); v.z = n[2].as<float>(); return true; }
	};

	template<>
	struct convert<vec4>
	{
		static Node encode(const vec4& v) { Node n; n.push_back(v.x); n.push_back(v.y); n.push_back(v.z); n.push_back(v.w); return n; }
		static bool decode(const Node& n, vec4& v) { if (!n.IsSequence() || n.size() != 4) return false; v.x = n[0].as<float>(); v.y = n[1].as<float>(); v.z = n[2].as<float>(); v.w = n[3].as<float>(); return true; }
	};

	template<>
	struct convert<quat>
	{
		static Node encode(const quat& v) { return convert<vec4>::encode(v.v4); }
		static bool decode(const Node& n, quat& v) { return convert<vec4>::decode(n, v.v4); }
	};
}

void application::serializeToFile(const char* filename)
{
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
			<< YAML::EndMap;

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

	out << YAML::Key << "Lighting"
		<< YAML::Value
			<< YAML::BeginMap
				<< YAML::Key << "Environment Intensity" << renderer->settings.environmentIntensity
			<< YAML::EndMap;

	out << YAML::EndMap;

	fs::create_directories(fs::path(filename).parent_path());

	std::ofstream fout(filename);
	fout << out.c_str();
}

bool application::deserializeFromFile(const char* filename)
{
	std::ifstream stream(filename);
	YAML::Node data = YAML::Load(stream);
	if (!data["Scene"])
	{
		return false;
	}

	std::string sceneName = data["Scene"].as<std::string>();

	auto cameraNode = data["Camera"];
	camera.position = cameraNode["Position"].as<vec3>();
	camera.rotation = cameraNode["Rotation"].as<quat>();
	camera.nearPlane = cameraNode["Near Plane"].as<float>();
	camera.farPlane = cameraNode["Far Plane"].as<float>();

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
	sun.blendDistances = sunNode["Blend Area"].as<decltype(sun.blendDistances)>();

	auto lightingNode = data["Lighting"];
	renderer->settings.environmentIntensity = lightingNode["Environment Intensity"].as<float>();

	return true;
}
