#include "pch.h"
#include "application.h"
#include "geometry/geometry.h"
#include "dx/dx_texture.h"
#include "core/random.h"
#include "core/color.h"
#include "core/imgui.h"
#include "dx/dx_context.h"
#include "dx/dx_profiling.h"
#include "animation/animation_controller.h"
#include "physics/physics.h"
#include "core/threading.h"
#include "rendering/mesh_shader.h"
#include "rendering/shadow_map.h"
#include "rendering/shadow_map_renderer.h"
#include "editor/file_dialog.h"
#include "core/yaml.h"
#include "learning/locomotion_learning.h"

#include <fontawesome/list.h>

struct raytrace_component
{
	raytracing_object_type type;
};

struct transform_undo
{
	scene_entity entity;
	trs before;
	trs after;
};

struct selection_undo
{
	application* app;
	scene_entity before;
	scene_entity after;
};

static void undoTransform(void* d)
{
	transform_undo& t = *(transform_undo*)d;
	t.entity.getComponent<trs>() = t.before;
}

static void redoTransform(void* d)
{
	transform_undo& t = *(transform_undo*)d;
	t.entity.getComponent<trs>() = t.after;
}

static void undoSelection(void* d)
{
	selection_undo& t = *(selection_undo*)d;
	t.app->setSelectedEntityNoUndo(t.before);
}

static void redoSelection(void* d)
{
	selection_undo& t = *(selection_undo*)d;
	t.app->setSelectedEntityNoUndo(t.after);
}

static raytracing_object_type defineBlasFromMesh(const ref<composite_mesh>& mesh, path_tracer& pathTracer)
{
	if (dxContext.featureSupport.raytracing())
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
	if (dxContext.featureSupport.meshShaders())
	{
		initializeMeshShader();
	}
}

static ref<pbr_material> lollipopMaterial;

void application::initialize(main_renderer* renderer)
{
	this->renderer = renderer;

	camera.initializeIngame(vec3(0.f, 1.f, 5.f), quat::identity, deg2rad(70.f), 0.1f);
	cameraController.initialize(&camera);

	if (dxContext.featureSupport.raytracing())
	{
		pathTracer.initialize();
		raytracingTLAS.initialize();
	}

	appScene.createEntity("Cloth")
		.addComponent<trs>(trs::identity)
		.addComponent<cloth_component>(10.f, 10.f, 20, 20, 8.f);


#if 0
	if (auto sponzaMesh = loadMeshFromFile("assets/sponza/sponza.obj"))
	{
		auto blas = defineBlasFromMesh(sponzaMesh, pathTracer);
	
		appScene.createEntity("Sponza")
			.addComponent<trs>(vec3(0.f, 0.f, 0.f), quat::identity, 0.01f)
			.addComponent<raster_component>(sponzaMesh)
			.addComponent<raytrace_component>(blas);
	}

	if (auto stormtrooperMesh = loadAnimatedMeshFromFile("assets/stormtrooper/stormtrooper.fbx"))
	{
		appScene.createEntity("Stormtrooper 1")
			.addComponent<trs>(vec3(-5.f, 0.f, -1.f), quat::identity)
			.addComponent<raster_component>(stormtrooperMesh)
			.addComponent<animation_component>(make_ref<simple_animation_controller>())
			.addComponent<dynamic_geometry_component>();

		appScene.createEntity("Stormtrooper 2")
			.addComponent<trs>(vec3(0.f, 0.f, -2.f), quat::identity)
			.addComponent<raster_component>(stormtrooperMesh)
			.addComponent<animation_component>(make_ref<simple_animation_controller>())
			.addComponent<dynamic_geometry_component>();

		appScene.createEntity("Stormtrooper 3")
			.addComponent<trs>(vec3(5.f, 0.f, -1.f), quat::identity)
			.addComponent<raster_component>(stormtrooperMesh)
			.addComponent<animation_component>(make_ref<simple_animation_controller>())
			.addComponent<dynamic_geometry_component>();
	}

	if (auto pilotMesh = loadAnimatedMeshFromFile("assets/pilot/pilot.fbx"))
	{
		appScene.createEntity("Pilot")
			.addComponent<trs>(vec3(2.5f, 0.f, -1.f), quat::identity, 0.2f)
			.addComponent<raster_component>(pilotMesh)
			.addComponent<animation_component>(make_ref<simple_animation_controller>())
			.addComponent<dynamic_geometry_component>();
	}

	if (auto unrealMesh = loadAnimatedMeshFromFile("assets/unreal/unreal_mannequin.fbx"))
	{
		unrealMesh->skeleton.pushAssimpAnimationsInDirectory("assets/unreal/animations");

		appScene.createEntity("Mannequin")
			.addComponent<trs>(vec3(-2.5f, 0.f, -1.f), quat(vec3(1.f, 0.f, 0.f), deg2rad(-90.f)), 0.019f)
			.addComponent<raster_component>(unrealMesh)
			.addComponent<animation_component>(make_ref<simple_animation_controller>())
			.addComponent<dynamic_geometry_component>();
	}
#endif

#if 0
	if (auto ragdollMesh = loadAnimatedMeshFromFile("assets/ragdoll/locomotion_pack/xbot.fbx", true, false))
	{
		//ragdollMesh->skeleton.prettyPrintHierarchy();
		ragdollMesh->skeleton.pushAssimpAnimationsInDirectory("assets/ragdoll/locomotion_pack/animations");
		ragdollMesh->skeleton.readAnimationPropertiesFromFile("assets/ragdoll/animation_properties.yaml");

		appScene.createEntity("Ragdoll")
			.addComponent<trs>(vec3(-2.5f, 0.f, -1.f), quat::identity, 0.01f)
			.addComponent<raster_component>(ragdollMesh)
			.addComponent<animation_component>(make_ref<random_path_animation_controller>())
			.addComponent<dynamic_geometry_component>();
	}
#endif

#if 1
	{
		//lollipopMaterial = createPBRMaterial(
		//	"assets/sphere/Tiles074_2K_Color.jpg",
		//	"assets/sphere/Tiles074_2K_Normal.jpg",
		//	"assets/sphere/Tiles074_2K_Roughness.jpg",
		//	{}, vec4(0.f), vec4(1.f), 1.f, 1.f, true);

		lollipopMaterial = createPBRMaterial(
			"assets/sponza/textures/Sponza_Curtain_Red_diffuse.tga",
			"assets/sponza/textures/Sponza_Curtain_Red_normal.tga",
			"assets/sponza/textures/Sponza_Curtain_roughness.tga",
			"assets/sponza/textures/Sponza_Curtain_metallic.tga",
			vec4(0.f), vec4(1.f), 1.f, 1.f, true);

#if 0
		cpu_mesh primitiveMesh(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents);
		auto testMesh = make_ref<composite_mesh>();
		testMesh->submeshes.push_back({ primitiveMesh.pushSphere(15, 15, 1.f, vec3(0.f, 0.f, 0.f)), {}, trs::identity, lollipopMaterial });
		testMesh->mesh = primitiveMesh.createDXMesh();

		float extents = 100.f;
		for (float z = -extents; z < extents; z += 10.f)
		{
			for (float y = -extents; y < extents; y += 10.f)
			{
				for (float x = -extents; x < extents; x += 10.f)
				{
					appScene.createEntity("Ball")
						.addComponent<trs>(vec3(x, y, z), quat::identity, 1.f)
						.addComponent<raster_component>(testMesh);
				}
			}
		}
#endif
	}
#endif

#if 1
	{
		appScene.createEntity("Force field")
			.addComponent<trs>(vec3(0.f), quat::identity)
			.addComponent<force_field_component>(vec3(0.f, 0.f, -1.f));

		cpu_mesh primitiveMesh(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents);

		auto lollipopMaterial = createPBRMaterial(
			"assets/sphere/Tiles074_2K_Color.jpg",
			"assets/sphere/Tiles074_2K_Normal.jpg",
			"assets/sphere/Tiles074_2K_Roughness.jpg",
			{}, vec4(0.f), vec4(1.f), 1.f, 1.f);

		auto testMesh = make_ref<composite_mesh>();
		testMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, 1.f, 0.1f), {}, trs::identity, lollipopMaterial });
		testMesh->submeshes.push_back({ primitiveMesh.pushSphere(15, 15, 0.4f, vec3(0.f, 0.5f + 0.1f + 0.4f, 0.f)), {}, trs::identity, lollipopMaterial });

		auto groundMesh = make_ref<composite_mesh>();
		groundMesh->submeshes.push_back({ primitiveMesh.pushCube(vec3(20.f, 4.f, 20.f)), {}, trs::identity, createPBRMaterial(
				"assets/desert/textures/BlueContainer_Albedo.png",
				"assets/desert/textures/Container_Normal.png",
				{}, 
				"assets/desert/textures/Container_Metallic.png", vec4(0.f), vec4(1.f), 0.2f)
			});

		auto boxMesh = make_ref<composite_mesh>();
		boxMesh->submeshes.push_back({ primitiveMesh.pushCube(vec3(1.f, 1.f, 2.f)), {}, trs::identity, 
			createPBRMaterial(
				"assets/desert/textures/WoodenCrate2_Albedo.png", 
				"assets/desert/textures/WoodenCrate2_Normal.png", 
				{}, {})
			});

		auto test1 = appScene.createEntity("Lollipop 1")
			.addComponent<trs>(vec3(20.f, 5.f, 0.f), quat::identity)
			.addComponent<raster_component>(testMesh)
			.addComponent<collider_component>(collider_component::asCapsule({ vec3(0.f, -0.5f, 0.f), vec3(0.f, 0.5f, 0.f), 0.1f }, 0.2f, 0.5f, 4.f))
			.addComponent<collider_component>(collider_component::asSphere({ vec3(0.f, 0.5f + 0.1f + 0.4f, 0.f), 0.4f }, 0.2f, 0.5f, 4.f))
			.addComponent<rigid_body_component>(true, 1.f);

		auto test2 = appScene.createEntity("Lollipop 2")
			.addComponent<trs>(vec3(20.f, 5.f, -2.f), quat::identity)
			.addComponent<raster_component>(testMesh)
			.addComponent<collider_component>(collider_component::asCapsule({ vec3(0.f, -0.5f, 0.f), vec3(0.f, 0.5f, 0.f), 0.1f }, 0.2f, 0.5f, 4.f))
			.addComponent<collider_component>(collider_component::asSphere({ vec3(0.f, 0.5f + 0.1f + 0.4f, 0.f), 0.4f }, 0.2f, 0.5f, 4.f))
			.addComponent<rigid_body_component>(true, 1.f);

		for (uint32 i = 0; i < 10; ++i)
		{
			appScene.createEntity("Cube")
				.addComponent<trs>(vec3(25.f, 10.f + i * 3.f, -5.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(1.f)))
				.addComponent<raster_component>(boxMesh)
				.addComponent<collider_component>(collider_component::asAABB(bounding_box::fromCenterRadius(vec3(0.f, 0.f, 0.f), vec3(1.f, 1.f, 2.f)), 0.1f, 0.5f, 1.f))
				.addComponent<rigid_body_component>(false, 1.f);
		}

		bounding_hull hull =
		{
			quat::identity,
			vec3(0.f),
			allocateBoundingHullGeometry("assets/colliders/hull.fbx")
		};

		if (hull.geometryIndex != INVALID_BOUNDING_HULL_INDEX)
		{
			appScene.createEntity("Hull")
				.addComponent<trs>(vec3(20.f, 15.f, 0.f), quat::identity)
				.addComponent<raster_component>(loadMeshFromFile("assets/colliders/hull.fbx"))
				.addComponent<collider_component>(collider_component::asHull(hull, 0.1f, 0.5f, 0.1f))
				.addComponent<rigid_body_component>(false, 0.f);
		}

		appScene.createEntity("Test ground")
			.addComponent<trs>(vec3(30.f, -4.f, 0.f), quat(vec3(1.f, 0.f, 0.f), deg2rad(0.f)))
			.addComponent<raster_component>(groundMesh)
			.addComponent<collider_component>(collider_component::asAABB(bounding_box::fromCenterRadius(vec3(0.f, 0.f, 0.f), vec3(20.f, 4.f, 20.f)), 0.1f, 1.f, 4.f))
			.addComponent<rigid_body_component>(true);

		/*appScene.createEntity("Test ground")
			.addComponent<trs>(vec3(20.f, -5.f, 0.f), quat(vec3(1.f, 0.f, 0.f), deg2rad(0.f)))
			.addComponent<raster_component>(groundMesh)
			.addComponent<collider_component>(collider_component::asAABB(bounding_box::fromCenterRadius(vec3(0.f, 0.f, 0.f), vec3(20.f, 4.f, 20.f)), 0.1f, 1.f, 4.f))
			.addComponent<rigid_body_component>(true);*/


#if 1
		auto chainMesh = make_ref<composite_mesh>();
		chainMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, 2.f, 0.18f, vec3(0.f)), {}, trs::identity, lollipopMaterial });

		auto fixed = appScene.createEntity("Fixed")
			.addComponent<trs>(vec3(37.f, 15.f, -2.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(90.f)))
			.addComponent<raster_component>(chainMesh)
			.addComponent<collider_component>(collider_component::asCapsule({ vec3(0.f, -1.f, 0.f), vec3(0.f, 1.f, 0.f), 0.18f }, 0.2f, 0.5f, 1.f))
			.addComponent<rigid_body_component>(true, 1.f);

		//fixed.getComponent<rigid_body_component>().angularVelocity = vec3(0.f, 0.1f, 0.f);
		//fixed.getComponent<rigid_body_component>().angularDamping = 0.f;

		auto prev = fixed;

		for (uint32 i = 0; i < 10; ++i)
		{
			float xPrev = 37.f + 2.5f * i;
			float xCurr = 37.f + 2.5f * (i + 1);

			auto chain = appScene.createEntity("Chain")
				.addComponent<trs>(vec3(xCurr, 15.f, -2.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(90.f)))
				.addComponent<raster_component>(chainMesh)
				.addComponent<collider_component>(collider_component::asCapsule({ vec3(0.f, -1.f, 0.f), vec3(0.f, 1.f, 0.f), 0.18f }, 0.2f, 0.5f, 1.f))
				.addComponent<rigid_body_component>(false, 1.f);

			//addHingeJointConstraintFromGlobalPoints(prev, chain, vec3(xPrev + 1.18f, 15.f, -2.f), vec3(0.f, 0.f, 1.f), deg2rad(5.f), deg2rad(20.f));
			addConeTwistConstraintFromGlobalPoints(prev, chain, vec3(xPrev + 1.18f, 15.f, -2.f), vec3(1.f, 0.f, 0.f), deg2rad(20.f), deg2rad(30.f));

			prev = chain;
		}
#endif


		testMesh->mesh = 
		groundMesh->mesh = 
		boxMesh->mesh = 
		chainMesh->mesh =
			primitiveMesh.createDXMesh();
	}
#endif

	//ragdoll.initialize(appScene, vec3(60.f, 1.25f, -2.f));
	ragdoll.initialize(appScene, vec3(20.f, 1.25f, 0.f));

	//initializeLocomotionEval(appScene, ragdoll);

	// Raytracing.
	if (dxContext.featureSupport.raytracing())
	{
		pathTracer.finish();
	}



	setEnvironment("assets/sky/sunset_in_the_chalk_quarry_4k.hdr");



	random_number_generator rng = { 14878213 };

#if 0
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
#endif

#if 0
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
	sun.stabilize = true;

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

#if 1
	fireParticleSystem.initialize(10000, 50.f, "assets/particles/fire_explosion.tif", 6, 6);
	smokeParticleSystem.initialize(10000, 500.f, "assets/particles/smoke1.tif", 5, 5);
	boidParticleSystem.initialize(10000, 2000.f);
#endif
}

static bool plotAndEditTonemapping(tonemap_settings& tonemap)
{
	bool result = false;
	if (ImGui::TreeNode("Tonemapping"))
	{
		ImGui::PlotLines("Tone map",
			[](void* data, int idx)
			{
				float t = idx * 0.01f;
				tonemap_settings& aces = *(tonemap_settings*)data;
				return aces.tonemap(t);
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
#define BLEND_SETTINGS		"Blend distances", sun.blendDistances.data, 0.f, 10.f, "%.6f"

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

static bool editSSR(bool& enable, ssr_settings& settings)
{
	bool result = false;
	result |= ImGui::Checkbox("Enable SSR", &enable);
	if (enable)
	{
		result |= ImGui::SliderInt("Num iterations", (int*)&settings.numSteps, 1, 1024);
		result |= ImGui::SliderFloat("Max distance", &settings.maxDistance, 5.f, 1000.f);
		result |= ImGui::SliderFloat("Min. stride", &settings.minStride, 1.f, 50.f);
		result |= ImGui::SliderFloat("Max. stride", &settings.maxStride, settings.minStride, 50.f);
	}
	return result;
}

static bool editTAA(bool& enable, taa_settings& settings)
{
	bool result = false;
	result |= ImGui::Checkbox("Enable TAA", &enable);
	if (enable)
	{
		result |= ImGui::SliderFloat("Jitter strength", &settings.cameraJitterStrength, 0.f, 1.f);
	}
	return result;
}

static bool editBloom(bool& enable, bloom_settings& settings)
{
	bool result = false;
	result |= ImGui::Checkbox("Enable bloom", &enable);
	if (enable)
	{
		result |= ImGui::SliderFloat("Bloom threshold", &settings.threshold, 0.5f, 100.f);
		result |= ImGui::SliderFloat("Bloom strength", &settings.strength, 0.f, 1.f);
	}
	return result;
}

static bool editSharpen(bool& enable, sharpen_settings& settings)
{
	bool result = false;
	result |= ImGui::Checkbox("Enable sharpen", &enable);
	if (enable)
	{
		result |= ImGui::SliderFloat("Sharpen strength", &settings.strength, 0.f, 1.f);
	}
	return result;
}

static bool editFireParticleSystem(fire_particle_system& particleSystem)
{
	bool result = false;
	if (ImGui::TreeNode("Fire particles"))
	{
		result |= ImGui::SliderFloat("Emit rate", &particleSystem.emitRate, 0.f, 1000.f);
		result |= ImGui::Spline("Size over lifetime", ImVec2(200, 200), particleSystem.settings.sizeOverLifetime);
		result |= ImGui::Spline("Atlas progression over lifetime", ImVec2(200, 200), particleSystem.settings.atlasProgressionOverLifetime);
		result |= ImGui::Spline("Intensity over lifetime", ImVec2(200, 200), particleSystem.settings.intensityOverLifetime);

		ImGui::TreePop();
	}
	return result;
}

static bool editBoidParticleSystem(boid_particle_system& particleSystem)
{
	bool result = false;
	if (ImGui::TreeNode("Boid particles"))
	{
		result |= ImGui::SliderFloat("Emit rate", &particleSystem.emitRate, 0.f, 5000.f);
		result |= ImGui::SliderFloat("Emit radius", &particleSystem.settings.radius, 5.f, 100.f);

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
	if (selectedEntity != entity)
	{
		undoStack.pushAction("selection", undoSelection, redoSelection, selection_undo{ this, selectedEntity, entity });
	}

	setSelectedEntityNoUndo(entity);
}

void application::setSelectedEntityNoUndo(scene_entity entity)
{
	selectedEntity = entity;
	setSelectedEntityEulerRotation();
}

void application::drawMainMenuBar()
{
	static bool showIconsWindow = false;
	static bool showDemoWindow = false;

	bool controlsClicked = false;
	bool aboutClicked = false;

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu(ICON_FA_FILE "  File"))
		{
			char textBuffer[128];
			snprintf(textBuffer, sizeof(textBuffer), ICON_FA_UNDO " Undo %s", undoStack.undoPossible() ? undoStack.getUndoName() : "");
			if (ImGui::MenuItem(textBuffer, "Ctrl+Z", false, undoStack.undoPossible()))
			{
				undoStack.undo();
			}

			snprintf(textBuffer, sizeof(textBuffer), ICON_FA_REDO " Redo %s", undoStack.redoPossible() ? undoStack.getRedoName() : "");
			if (ImGui::MenuItem(textBuffer, "Ctrl+Y", false, undoStack.redoPossible()))
			{
				undoStack.redo();
			}
			ImGui::Separator();

			if (ImGui::MenuItem(ICON_FA_SAVE "  Save scene", "Ctrl+S"))
			{
				serializeToFile();
			}

			if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  Load scene", "Ctrl+O"))
			{
				deserializeFromFile();
			}

			ImGui::Separator();
			if (ImGui::MenuItem(ICON_FA_TIMES "  Exit", "Esc"))
			{
				PostQuitMessage(0);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(ICON_FA_TOOLS "  Developer"))
		{
			if (ImGui::MenuItem(showIconsWindow ? (ICON_FA_ICONS "  Hide available icons") : (ICON_FA_ICONS "  Show available icons")))
			{
				showIconsWindow = !showIconsWindow;
			}

			if (ImGui::MenuItem(showDemoWindow ? (ICON_FA_PUZZLE_PIECE "  Hide demo window") : (ICON_FA_PUZZLE_PIECE "  Show demo window")))
			{
				showDemoWindow = !showDemoWindow;
			}

			if (ImGui::MenuItem(profilerWindowOpen ? (ICON_FA_CHART_BAR "  Hide GPU profiler") : (ICON_FA_CHART_BAR "  Show GPU profiler"), nullptr, nullptr, ENABLE_DX_PROFILING))
			{
				profilerWindowOpen = !profilerWindowOpen;
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(ICON_FA_CHILD "  Help"))
		{
			if (ImGui::MenuItem(ICON_FA_COMPASS "  Controls"))
			{
				controlsClicked = true;
			}

			if (ImGui::MenuItem(ICON_FA_QUESTION "  About"))
			{
				aboutClicked = true;
			}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	if (controlsClicked)
	{
		ImGui::OpenPopup("Controls");
	}

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Controls", 0, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("The camera can be controlled in two modes:");
		ImGui::BulletText(
			"Free flying: Hold the right mouse button and move the mouse to turn.\n"
			"Move with WASD (while holding right mouse). Q & E let you move down and up.\n"
			"Holding Shift will make you fly faster, Ctrl will make you slower."
		);
		ImGui::BulletText(
			"Orbit: While holding Alt, press and hold the left mouse button to\n"
			"orbit around a point in front of the camera."
		);
		ImGui::Separator();
		ImGui::Text(
			"Left-click on objects to select them. Toggle through gizmos using\n"
			"Q (no gizmo), W (translate), E (rotate), R (scale).\n"
			"Press G to toggle between global and local coordinate system.\n"
			"You can also change the object's properties in the Scene Hierarchy window."
		);
		ImGui::Separator();
		ImGui::Text(
			"Press F to focus the camera on the selected object. This automatically\n"
			"sets the orbit distance such that you now orbit around this object."
		);
		ImGui::Separator();
		ImGui::Text(
			"Press V to toggle Vsync on or off."
		);
		ImGui::Separator();
		ImGui::Text(
			"You can drag and drop meshes from the Windows explorer into the game\n"
			"window to add it to the scene."
		);
		ImGui::Separator();

		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - 120) * 0.5f);

		if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
		ImGui::SetItemDefaultFocus();
		ImGui::EndPopup();
	}

	if (aboutClicked)
	{
		ImGui::OpenPopup("About");
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("About", 0, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Direct3D renderer");
		ImGui::Separator();

		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - 120) * 0.5f);

		if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
		ImGui::SetItemDefaultFocus();
		ImGui::EndPopup();
	}


	if (showIconsWindow)
	{
		ImGui::Begin("Icons", &showIconsWindow);

		static ImGuiTextFilter filter;
		filter.Draw();
		for (uint32 i = 0; i < arraysize(awesomeIcons); ++i)
		{
			ImGui::PushID(i);
			if (filter.PassFilter(awesomeIconNames[i]))
			{
				ImGui::Text("%s: %s", awesomeIconNames[i], awesomeIcons[i]);
				ImGui::SameLine();
				if (ImGui::Button("Copy to clipboard"))
				{
					ImGui::SetClipboardText(awesomeIconNames[i]);
				}
			}
			ImGui::PopID();
		}
		ImGui::End();
	}

	if (showDemoWindow)
	{
		ImGui::ShowDemoWindow(&showDemoWindow);
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
				setSelectedEntityNoUndo({});
			}
		});

		ImGui::Separator();

		if (selectedEntity)
		{
			ImGui::AlignTextToFramePadding();

			ImGui::PushID((uint32)selectedEntity);
			ImGui::InputText("Name", selectedEntity.getComponent<tag_component>().name, sizeof(tag_component::name));
			ImGui::PopID();
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_TRASH_ALT))
			{
				appScene.deleteEntity(selectedEntity);
				setSelectedEntityNoUndo({});
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
					anim.controller->edit(selectedEntity);
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

		ImGui::Dropdown("Aspect ratio", aspectRatioNames, aspect_ratio_mode_count, (uint32&)renderer->aspectRatioMode);

		plotAndEditTonemapping(renderer->tonemapSettings);
		editSunShadowParameters(sun);

		if (ImGui::TreeNode("Post processing"))
		{
			if (renderer->spec.allowSSR) { editSSR(renderer->enableSSR, renderer->ssrSettings); }
			if (renderer->spec.allowTAA) { editTAA(renderer->enableTAA, renderer->taaSettings); }
			if (renderer->spec.allowBloom) { editBloom(renderer->enableBloom, renderer->bloomSettings); }
			editSharpen(renderer->enableSharpen, renderer->sharpenSettings);

			ImGui::TreePop();
		}

		ImGui::SliderFloat("Environment intensity", &renderer->environmentIntensity, 0.f, 2.f);
		ImGui::SliderFloat("Sky intensity", &renderer->skyIntensity, 0.f, 2.f);

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
		else
		{
			editFireParticleSystem(fireParticleSystem);
			editBoidParticleSystem(boidParticleSystem);

			//ragdoll.edit();
			ImGui::SliderInt("Physics rigid solver iterations", (int*)&physicsSettings.numRigidSolverIterations, 1, 200);

			ImGui::SliderInt("Physics cloth velocity iterations", (int*)&physicsSettings.numClothVelocityIterations, 0, 10);
			ImGui::SliderInt("Physics cloth position iterations", (int*)&physicsSettings.numClothPositionIterations, 0, 10);
			ImGui::SliderInt("Physics cloth drift iterations", (int*)&physicsSettings.numClothDriftIterations, 0, 10);

			ImGui::SliderFloat("Physics test force", &testPhysicsForce, 1.f, 10000.f);
		}
	}

	ImGui::End();
}

void application::resetRenderPasses()
{
	opaqueRenderPass.reset();
	transparentRenderPass.reset();
	overlayRenderPass.reset();
	outlineRenderPass.reset();
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
	opaqueRenderPass.sort();
	transparentRenderPass.sort();
	overlayRenderPass.sort();

	renderer->submitRenderPass(&opaqueRenderPass);
	renderer->submitRenderPass(&transparentRenderPass);
	renderer->submitRenderPass(&overlayRenderPass);
	renderer->submitRenderPass(&outlineRenderPass);

	shadow_map_renderer::submitRenderPass(&sunShadowRenderPass);

	for (uint32 i = 0; i < numSpotLightShadowPasses; ++i)
	{
		shadow_map_renderer::submitRenderPass(&spotShadowRenderPasses[i]);
	}

	for (uint32 i = 0; i < numPointLightShadowPasses; ++i)
	{
		shadow_map_renderer::submitRenderPass(&pointShadowRenderPasses[i]);
	}
}

bool application::handleUserInput(const user_input& input, float dt)
{
	// Returns true, if the user dragged an object using a gizmo.

	if (input.keyboard['F'].pressEvent && selectedEntity && selectedEntity.hasComponent<trs>())
	{
		auto& transform = selectedEntity.getComponent<trs>();

		auto aabb = selectedEntity.hasComponent<raster_component>() ? selectedEntity.getComponent<raster_component>().mesh->aabb : bounding_box::fromCenterRadius(0.f, 1.f);
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


	{
		int iconSize = 40;

		if (ImGui::BeginWindowHiddenTabBar("Controls", 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			transformation_space constantLocal = transformation_local;
			transformation_space& space = (gizmo.type == transformation_type_scale) ? constantLocal : gizmo.space;

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

			ImGui::PushID(&gizmo.space);
			ImGui::IconRadioButton(imgui_icon_global, (int*)&space, transformation_global, gizmo.type != transformation_type_scale, iconSize);
			ImGui::SameLine(0, 0);
			ImGui::IconRadioButton(imgui_icon_local, (int*)&space, transformation_local, gizmo.type != transformation_type_scale, iconSize);
			ImGui::PopID();

			ImGui::SameLine(0.f, (float)iconSize);


			ImGui::PushID(&gizmo.type);
			ImGui::IconRadioButton(imgui_icon_translate, (int*)&gizmo.type, transformation_type_translation, true, iconSize);
			ImGui::SameLine(0, 0);
			ImGui::IconRadioButton(imgui_icon_rotate, (int*)&gizmo.type, transformation_type_rotation, true, iconSize);
			ImGui::SameLine(0, 0);
			ImGui::IconRadioButton(imgui_icon_scale, (int*)&gizmo.type, transformation_type_scale, true, iconSize);
			ImGui::SameLine(0, 0);
			ImGui::IconRadioButton(imgui_icon_cross, (int*)&gizmo.type, transformation_type_none, true, iconSize);
			ImGui::PopID();

			ImGui::SameLine(0.f, (float)iconSize);

			ImGui::PopStyleColor();
		}

		ImGui::End();
	}

	if (!inputCaptured)
	{
		inputCaptured = gizmo.handleKeyboardInput(input);
	}

	if (selectedEntity)
	{
		if (selectedEntity.hasComponent<trs>())
		{
			// Transform entity.
			trs& transform = selectedEntity.getComponent<trs>();

			// Saved rigid-body properties. When a RB is dragged, we make it kinematic.
			static bool saved = false;
			static float invMass;

			bool draggingBefore = gizmo.dragging;

			if (gizmo.manipulateTransformation(transform, camera, input, !inputCaptured, &overlayRenderPass))
			{
				setSelectedEntityEulerRotation();
				inputCaptured = true;
				objectMovedByGizmo = true;

				if (!saved && selectedEntity.hasComponent<rigid_body_component>())
				{
					rigid_body_component& rb = selectedEntity.getComponent<rigid_body_component>();
					invMass = rb.invMass;

					rb.invMass = 0.f;
					rb.linearVelocity = 0.f;

					saved = true;
				}
			}
			else
			{
				if (draggingBefore)
				{
					undoStack.pushAction("transform entity", undoTransform, redoTransform, transform_undo{ selectedEntity, gizmo.originalTransform, transform });
				}

				if (saved)
				{
					assert(selectedEntity.hasComponent<rigid_body_component>());
					rigid_body_component& rb = selectedEntity.getComponent<rigid_body_component>();

					rb.invMass = invMass;
					saved = false;
				}
			}
		}

		if (!inputCaptured)
		{
			if (input.keyboard[key_ctrl].down && input.keyboard['D'].pressEvent)
			{
				// Duplicate entity.
				scene_entity newEntity = appScene.createEntity(selectedEntity.getComponent<tag_component>().name);
				appScene.copyComponentsIfExists<trs, raster_component, animation_component, raytrace_component>(selectedEntity, newEntity);
				setSelectedEntity(newEntity);
				inputCaptured = true;
				objectMovedByGizmo = true;
			}
			else if (input.keyboard[key_backspace].pressEvent || input.keyboard[key_delete].pressEvent)
			{
				// Delete entity.
				appScene.deleteEntity(selectedEntity);
				setSelectedEntity({});
				inputCaptured = true;
				objectMovedByGizmo = true;
			}
		}
	}

	if (!inputCaptured)
	{
		if (input.keyboard[key_ctrl].down && input.keyboard['Z'].pressEvent)
		{
			undoStack.undo();
			inputCaptured = true;
			objectMovedByGizmo = true;
		}
		if (input.keyboard[key_ctrl].down && input.keyboard['Y'].pressEvent)
		{
			undoStack.redo();
			inputCaptured = true;
			objectMovedByGizmo = true;
		}
		if (input.keyboard[key_ctrl].down && input.keyboard['S'].pressEvent)
		{
			serializeToFile();
			inputCaptured = true;
			ImGui::GetIO().KeysDown['S'] = false; // Hack: Window does not get notified of inputs due to the file dialog.
		}
		if (input.keyboard[key_ctrl].down && input.keyboard['O'].pressEvent)
		{
			deserializeFromFile();
			inputCaptured = true;
			ImGui::GetIO().KeysDown['O'] = false; // Hack: Window does not get notified of inputs due to the file dialog.
		}
	}

	if (!inputCaptured && input.mouse.left.clickEvent)
	{
		// Temporary.
		if (input.keyboard[key_shift].down)
		{
			testPhysicsInteraction(appScene, camera.generateWorldSpaceRay(input.mouse.relX, input.mouse.relY), testPhysicsForce);
		}
		else
		{
			if (renderer->hoveredObjectID != -1)
			{
				setSelectedEntity({ renderer->hoveredObjectID, appScene });
			}
			else
			{
				setSelectedEntity({});
			}
		}
		inputCaptured = true;
	}

	if (!inputCaptured && input.mouse.left.down && input.keyboard[key_ctrl].down)
	{
		vec3 dir = camera.generateWorldSpaceRay(input.mouse.relX, input.mouse.relY).direction;
		sun.direction = -dir;
		inputCaptured = true;
	}

	return objectMovedByGizmo;
}

void application::update(const user_input& input, float dt)
{
	//dt = min(dt, 1.f / 30.f);
	//dt = 1.f / 60.f;

	//stepLocomotionEval();


	resetRenderPasses();

	bool objectDragged = false;
	objectDragged |= handleUserInput(input, dt);
	objectDragged |= drawSceneHierarchy();
	drawSettings(dt);
	drawMainMenuBar();

	//undoStack.verify();
	//undoStack.display();
	
	physicsStep(appScene, dt, physicsSettings);
	
	// Particles.

#if 0
	fireParticleSystem.settings.cameraPosition = camera.position;
	smokeParticleSystem.settings.cameraPosition = camera.position;

	boidParticleSystem.update(dt);
	boidParticleSystem.render(&transparentRenderPass);

	fireParticleSystem.update(dt);
	fireParticleSystem.render(&transparentRenderPass);

	//smokeParticleSystem.update(dt);
	//smokeParticleSystem.render(&transparentRenderPass);
#endif

	sun.updateMatrices(camera);

	// Set global rendering stuff.
	renderer->setCamera(camera);
	renderer->setSun(sun);
	renderer->setEnvironment(environment);

#if 1
	if (renderer->mode == renderer_mode_rasterized)
	{
		if (dxContext.featureSupport.meshShaders())
		{
			//testRenderMeshShader(&overlayRenderPass);
		}


		thread_job_context context;

		// Update animated meshes.
		for (auto [entityHandle, anim, raster] : appScene.group(entt::get<animation_component, raster_component>).each())
		{
			scene_entity entity = { entityHandle, appScene };

			auto controller = anim.controller;
			context.addWork([controller, entity, dt]()
			{
				controller->update(entity, dt);
			});
		}

		context.waitForWorkCompletion();




		// Render shadow maps.
		renderSunShadowMap(sun, &sunShadowRenderPass, appScene, objectDragged);

		for (uint32 i = 0; i < (uint32)spotLights.size(); ++i)
		{
			if (spotLights[i].shadowInfoIndex >= 0)
			{
				uint32 renderPassIndex = numSpotShadowRenderPasses++;
				spot_shadow_info shadowInfo = renderSpotShadowMap(spotLights[i], i, &spotShadowRenderPasses[renderPassIndex], renderPassIndex, appScene, objectDragged);
				spotLightShadowInfos.push_back(shadowInfo);
			}
		}

		for (uint32 i = 0; i < (uint32)pointLights.size(); ++i)
		{
			if (pointLights[i].shadowInfoIndex >= 0)
			{
				uint32 renderPassIndex = numPointShadowRenderPasses++;
				point_shadow_info shadowInfo = renderPointShadowMap(pointLights[i], i, &pointShadowRenderPasses[renderPassIndex], renderPassIndex, appScene, objectDragged);
				pointLightShadowInfos.push_back(shadowInfo);
			}
		}



		// Submit render calls.
		for (auto [entityHandle, raster, transform] : appScene.group(entt::get<raster_component, trs>).each())
		{
			const dx_mesh& mesh = raster.mesh->mesh;
			mat4 m = trsToMat4(transform);

			scene_entity entity = { entityHandle, appScene };
			bool outline = selectedEntity == entity;

			bool dynamic = entity.hasComponent<dynamic_geometry_component>();
			mat4 lastM = dynamic ? trsToMat4(entity.getComponent<dynamic_geometry_component>().lastFrameTransform) : m;

			if (entity.hasComponent<animation_component>())
			{
				auto& anim = entity.getComponent<animation_component>();
				auto controller = anim.controller;

				uint32 numSubmeshes = (uint32)raster.mesh->submeshes.size();

				for (uint32 i = 0; i < numSubmeshes; ++i)
				{
					submesh_info submesh = controller->currentSubmeshes[i];
					submesh_info prevFrameSubmesh = controller->prevFrameSubmeshes[i];

					const ref<pbr_material>& material = raster.mesh->submeshes[i].material;

					if (material->albedoTint.a < 1.f)
					{
						transparentRenderPass.renderObject(m, controller->currentVertexBuffer, mesh.indexBuffer, submesh, material);
					}
					else
					{
						opaqueRenderPass.renderAnimatedObject(m, lastM, 
							controller->currentVertexBuffer, controller->prevFrameVertexBuffer, mesh.indexBuffer, 
							submesh, prevFrameSubmesh, material,
							(uint32)entityHandle);
					}

					if (outline)
					{
						outlineRenderPass.renderOutline(m, controller->currentVertexBuffer, mesh.indexBuffer, submesh);
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
						transparentRenderPass.renderObject(m, mesh.vertexBuffer, mesh.indexBuffer, submesh, material);
					}
					else
					{
						if (dynamic)
						{
							opaqueRenderPass.renderDynamicObject(m, lastM, mesh.vertexBuffer, mesh.indexBuffer, submesh, material, (uint32)entityHandle);
						}
						else
						{
							opaqueRenderPass.renderStaticObject(m, mesh.vertexBuffer, mesh.indexBuffer, submesh, material, (uint32)entityHandle);
						}
					}

					if (outline)
					{
						outlineRenderPass.renderOutline(m, mesh.vertexBuffer, mesh.indexBuffer, submesh);
					}
				}
			}
		}

		for (auto [entityHandle, cloth] : appScene.view<cloth_component>().each())
		{
			uint32 numVertices = cloth.getRenderableVertexCount();
			auto [positionVertexBuffer, positionPtr] = dxContext.createDynamicVertexBuffer(sizeof(vec3), numVertices);
			auto [otherVertexBuffer, otherPtr] = dxContext.createDynamicVertexBuffer(sizeof(vertex_uv_normal_tangent), numVertices);
			auto [indexBuffer, indexPtr] = dxContext.createDynamicIndexBuffer(sizeof(uint16), cloth.getRenderableTriangleCount() * 3);

			submesh_info submesh = cloth.getRenderData((vec3*)positionPtr, (vertex_uv_normal_tangent*)otherPtr, (indexed_triangle16*)indexPtr);

			opaqueRenderPass.renderStaticObject(mat4::identity, material_vertex_buffer_group_view{ positionVertexBuffer, otherVertexBuffer }, indexBuffer,
				submesh, lollipopMaterial, (uint32)entityHandle);

			scene_entity entity = { entityHandle, appScene };
			bool outline = selectedEntity == entity;

			if (outline)
			{
				outlineRenderPass.renderOutline(mat4::identity, positionVertexBuffer, indexBuffer, submesh);
			}

			//for (const auto& p : cloth.particles)
			//{
			//	mat4 m = createModelMatrix(p.position, quat::identity, 0.1f);
			//	opaqueRenderPass.renderStaticObject(m, testMesh->mesh.vertexBuffer, testMesh->mesh.indexBuffer, testMesh->submeshes[0].info, testMesh->submeshes[0].material,
			//		(uint32)entityHandle);
			//}
		}

		void collisionDebugDraw(transparent_render_pass* renderPass);
		collisionDebugDraw(&transparentRenderPass);

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
		if (dxContext.featureSupport.raytracing())
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
#endif

	for (auto [entityHandle, transform, dynamic] : appScene.group(entt::get<trs, dynamic_geometry_component>).each())
	{
		dynamic.lastFrameTransform = transform;
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
		fs::path path = filename;
		path = path.stem();

		appScene.createEntity(path.string().c_str())
			.addComponent<trs>(vec3(0.f), quat::identity)
			.addComponent<raster_component>(mesh);
	}
}




// Serialization stuff.

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
				<< YAML::Key << "A" << YAML::Value << renderer->tonemapSettings.A
				<< YAML::Key << "B" << YAML::Value << renderer->tonemapSettings.B
				<< YAML::Key << "C" << YAML::Value << renderer->tonemapSettings.C
				<< YAML::Key << "D" << YAML::Value << renderer->tonemapSettings.D
				<< YAML::Key << "E" << YAML::Value << renderer->tonemapSettings.E
				<< YAML::Key << "F" << YAML::Value << renderer->tonemapSettings.F
				<< YAML::Key << "Linear White" << YAML::Value << renderer->tonemapSettings.linearWhite
				<< YAML::Key << "Exposure" << YAML::Value << renderer->tonemapSettings.exposure
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
				<< YAML::Key << "Intensity" << renderer->environmentIntensity
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
					<< YAML::Key << "Type" << YAML::Value << (uint32)anim.controller->type
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

	setSelectedEntityNoUndo({});

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
	renderer->tonemapSettings.A = tonemapNode["A"].as<float>();
	renderer->tonemapSettings.B = tonemapNode["B"].as<float>();
	renderer->tonemapSettings.C = tonemapNode["C"].as<float>();
	renderer->tonemapSettings.D = tonemapNode["D"].as<float>();
	renderer->tonemapSettings.E = tonemapNode["E"].as<float>();
	renderer->tonemapSettings.F = tonemapNode["F"].as<float>();
	renderer->tonemapSettings.linearWhite = tonemapNode["Linear White"].as<float>();
	renderer->tonemapSettings.exposure = tonemapNode["Exposure"].as<float>();

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
	renderer->environmentIntensity = environmentNode["Intensity"].as<float>();

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
			//entity.addComponent<animation_component>(createAnimationController((animation_controller_type)animNode["Type"].as<uint32>()));
		}
	}

	return true;
}
