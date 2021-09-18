#include "pch.h"
#include "application.h"
#include "geometry/geometry.h"
#include "dx/dx_texture.h"
#include "core/random.h"
#include "core/color.h"
#include "core/imgui.h"
#include "dx/dx_context.h"
#include "dx/dx_profiling.h"
#include "physics/physics.h"
#include "core/threading.h"
#include "rendering/mesh_shader.h"
#include "rendering/shadow_map.h"
#include "rendering/shadow_map_renderer.h"
#include "rendering/debug_visualization.h"
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

	void undo() { entity.getComponent<transform_component>() = before; }
	void redo() { entity.getComponent<transform_component>() = after; }
};

struct selection_undo
{
	application* app;
	scene_entity before;
	scene_entity after;

	void undo() { app->setSelectedEntityNoUndo(before); }
	void redo() { app->setSelectedEntityNoUndo(after); }
};

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

static ref<pbr_material> clothMaterial;

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

	//appScene.createEntity("Cloth")
	//	.addComponent<transform_component>(trs::identity)
	//	.addComponent<cloth_component>(vec3(0.f, 0.f, 0.f), quat::identity, 10.f, 10.f, 20, 20, 8.f);

	//clothMaterial = createPBRMaterial(
	//	"assets/sphere/Tiles074_2K_Color.jpg",
	//	"assets/sphere/Tiles074_2K_Normal.jpg",
	//	"assets/sphere/Tiles074_2K_Roughness.jpg",
	//	{}, vec4(0.f), vec4(1.f), 1.f, 1.f, true);

	clothMaterial = createPBRMaterial(
		"assets/sponza/textures/Sponza_Curtain_Red_diffuse.tga",
		"assets/sponza/textures/Sponza_Curtain_Red_normal.tga",
		"assets/sponza/textures/Sponza_Curtain_roughness.tga",
		"assets/sponza/textures/Sponza_Curtain_metallic.tga",
		vec4(0.f), vec4(1.f), 1.f, 1.f, true);

#if 1
	if (auto sponzaMesh = loadMeshFromFile("assets/sponza/sponza.obj"))
	{
		auto blas = defineBlasFromMesh(sponzaMesh, pathTracer);
	
		appScene.createEntity("Sponza")
			.addComponent<transform_component>(vec3(0.f, 0.f, 0.f), quat::identity, 0.01f)
			.addComponent<raster_component>(sponzaMesh)
			.addComponent<raytrace_component>(blas);
	}

	if (auto stormtrooperMesh = loadAnimatedMeshFromFile("assets/stormtrooper/stormtrooper.fbx"))
	{
		appScene.createEntity("Stormtrooper 1")
			.addComponent<transform_component>(vec3(-5.f, 0.f, -1.f), quat::identity)
			.addComponent<raster_component>(stormtrooperMesh)
			.addComponent<animation_component>()
			.addComponent<dynamic_transform_component>();

		appScene.createEntity("Stormtrooper 2")
			.addComponent<transform_component>(vec3(0.f, 0.f, -2.f), quat::identity)
			.addComponent<raster_component>(stormtrooperMesh)
			.addComponent<animation_component>()
			.addComponent<dynamic_transform_component>();

		appScene.createEntity("Stormtrooper 3")
			.addComponent<transform_component>(vec3(5.f, 0.f, -1.f), quat::identity)
			.addComponent<raster_component>(stormtrooperMesh)
			.addComponent<animation_component>()
			.addComponent<dynamic_transform_component>();
	}

	if (auto pilotMesh = loadAnimatedMeshFromFile("assets/pilot/pilot.fbx"))
	{
		appScene.createEntity("Pilot")
			.addComponent<transform_component>(vec3(2.5f, 0.f, -1.f), quat::identity, 0.2f)
			.addComponent<raster_component>(pilotMesh)
			.addComponent<animation_component>()
			.addComponent<dynamic_transform_component>();
	}

	if (auto unrealMesh = loadAnimatedMeshFromFile("assets/unreal/unreal_mannequin.fbx"))
	{
		unrealMesh->skeleton.pushAssimpAnimationsInDirectory("assets/unreal/animations");

		appScene.createEntity("Mannequin")
			.addComponent<transform_component>(vec3(-2.5f, 0.f, -1.f), quat(vec3(1.f, 0.f, 0.f), deg2rad(-90.f)), 0.019f)
			.addComponent<raster_component>(unrealMesh)
			.addComponent<animation_component>()
			.addComponent<dynamic_transform_component>();
	}
#endif

#if 0
	if (auto ragdollMesh = loadAnimatedMeshFromFile("assets/ragdoll/locomotion_pack/xbot.fbx", true, false))
	{
		//ragdollMesh->skeleton.prettyPrintHierarchy();
		ragdollMesh->skeleton.pushAssimpAnimationsInDirectory("assets/ragdoll/locomotion_pack/animations");

		appScene.createEntity("Ragdoll")
			.addComponent<transform_component>(vec3(-2.5f, 0.f, -1.f), quat::identity, 0.01f)
			.addComponent<raster_component>(ragdollMesh)
			.addComponent<animation_component>()
			.addComponent<dynamic_geometry_component>();
	}
#endif

#if 0
	{
		appScene.createEntity("Force field")
			.addComponent<transform_component>(vec3(0.f), quat::identity)
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
				"assets/desert/textures/Container_Metallic.png", vec4(0.f), vec4(1.f), 1.f)
			});

		auto boxMesh = make_ref<composite_mesh>();
		boxMesh->submeshes.push_back({ primitiveMesh.pushCube(vec3(1.f, 1.f, 2.f)), {}, trs::identity, 
			createPBRMaterial(
				"assets/desert/textures/WoodenCrate2_Albedo.png", 
				"assets/desert/textures/WoodenCrate2_Normal.png", 
				{}, {})
			});

		auto test1 = appScene.createEntity("Lollipop 1")
			.addComponent<transform_component>(vec3(20.f, 5.f, 0.f), quat::identity)
			.addComponent<raster_component>(testMesh)
			.addComponent<collider_component>(collider_component::asCapsule({ vec3(0.f, -0.5f, 0.f), vec3(0.f, 0.5f, 0.f), 0.1f }, 0.2f, 0.5f, 4.f))
			.addComponent<collider_component>(collider_component::asSphere({ vec3(0.f, 0.5f + 0.1f + 0.4f, 0.f), 0.4f }, 0.2f, 0.5f, 4.f))
			.addComponent<rigid_body_component>(true, 1.f);

		auto test2 = appScene.createEntity("Lollipop 2")
			.addComponent<transform_component>(vec3(20.f, 5.f, -2.f), quat::identity)
			.addComponent<raster_component>(testMesh)
			.addComponent<collider_component>(collider_component::asCapsule({ vec3(0.f, -0.5f, 0.f), vec3(0.f, 0.5f, 0.f), 0.1f }, 0.2f, 0.5f, 4.f))
			.addComponent<collider_component>(collider_component::asSphere({ vec3(0.f, 0.5f + 0.1f + 0.4f, 0.f), 0.4f }, 0.2f, 0.5f, 4.f))
			.addComponent<rigid_body_component>(true, 1.f);

		for (uint32 i = 0; i < 10; ++i)
		{
			appScene.createEntity("Cube")
				.addComponent<transform_component>(vec3(25.f, 10.f + i * 3.f, -5.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(1.f)))
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
				.addComponent<transform_component>(vec3(20.f, 15.f, 0.f), quat::identity)
				.addComponent<raster_component>(loadMeshFromFile("assets/colliders/hull.fbx"))
				.addComponent<collider_component>(collider_component::asHull(hull, 0.1f, 0.5f, 0.1f))
				.addComponent<rigid_body_component>(false, 0.f);
		}

		appScene.createEntity("Test ground")
			.addComponent<transform_component>(vec3(30.f, -4.f, 0.f), quat(vec3(1.f, 0.f, 0.f), deg2rad(0.f)))
			.addComponent<raster_component>(groundMesh)
			.addComponent<collider_component>(collider_component::asAABB(bounding_box::fromCenterRadius(vec3(0.f, 0.f, 0.f), vec3(20.f, 4.f, 20.f)), 0.1f, 1.f, 4.f))
			.addComponent<rigid_body_component>(true);

		/*appScene.createEntity("Test ground")
			.addComponent<transform_component>(vec3(20.f, -5.f, 0.f), quat(vec3(1.f, 0.f, 0.f), deg2rad(0.f)))
			.addComponent<raster_component>(groundMesh)
			.addComponent<collider_component>(collider_component::asAABB(bounding_box::fromCenterRadius(vec3(0.f, 0.f, 0.f), vec3(20.f, 4.f, 20.f)), 0.1f, 1.f, 4.f))
			.addComponent<rigid_body_component>(true);*/


#if 1
		auto chainMesh = make_ref<composite_mesh>();
		chainMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, 2.f, 0.18f, vec3(0.f)), {}, trs::identity, lollipopMaterial });

		auto fixed = appScene.createEntity("Fixed")
			.addComponent<transform_component>(vec3(37.f, 15.f, -2.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(90.f)))
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
				.addComponent<transform_component>(vec3(xCurr, 15.f, -2.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(90.f)))
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
	//ragdoll.initialize(appScene, vec3(20.f, 1.25f, 0.f));

	//initializeLocomotionEval(appScene, ragdoll);

	// Raytracing.
	if (dxContext.featureSupport.raytracing())
	{
		pathTracer.finish();
	}



	setEnvironment("assets/sky/sunset_in_the_chalk_quarry_4k.hdr");



	random_number_generator rng = { 14878213 };

#if 1
	appScene.createEntity("Spot light 0")
		.addComponent<position_rotation_component>(vec3(2.f, 3.f, 0.f), lookAtQuaternion(vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f)))
		.addComponent<spot_light_component>(
			randomRGB(rng),
			5.f,
			25.f,
			deg2rad(20.f),
			deg2rad(30.f),
			true,
			512u
		);
	
	appScene.createEntity("Spot light 1")
		.addComponent<position_rotation_component>(vec3(-2.f, 3.f, 0.f), lookAtQuaternion(vec3(-1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f)))
		.addComponent<spot_light_component>(
			randomRGB(rng),
			5.f,
			25.f,
			deg2rad(20.f),
			deg2rad(30.f),
			true,
			512u
		);

	appScene.createEntity("Point light 0")
		.addComponent<position_component>(vec3(0.f, 8.f, 0.f))
		.addComponent<point_light_component>(
			randomRGB(rng),
			1.f,
			10.f,
			true,
			512u
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
	if (ImGui::BeginTree("Tonemapping"))
	{
		ImGui::PlotLines("",
			[](void* data, int idx)
			{
				float t = idx * 0.01f;
				tonemap_settings& aces = *(tonemap_settings*)data;
				return aces.tonemap(t);
			},
			&tonemap, 100, 0, 0, 0.f, 1.f, ImVec2(250.f, 250.f));

		if (ImGui::BeginProperties())
		{
			result |= ImGui::PropertySlider("Shoulder strength", tonemap.A, 0.f, 1.f);
			result |= ImGui::PropertySlider("Linear strength", tonemap.B, 0.f, 1.f);
			result |= ImGui::PropertySlider("Linear angle", tonemap.C, 0.f, 1.f);
			result |= ImGui::PropertySlider("Toe strength", tonemap.D, 0.f, 1.f);
			result |= ImGui::PropertySlider("Tone numerator", tonemap.E, 0.f, 1.f);
			result |= ImGui::PropertySlider("Toe denominator", tonemap.F, 0.f, 1.f);
			result |= ImGui::PropertySlider("Linear white", tonemap.linearWhite, 0.f, 100.f);
			result |= ImGui::PropertySlider("Exposure", tonemap.exposure, -3.f, 3.f);
			ImGui::EndProperties();
		}

		ImGui::EndTree();
	}
	return result;
}

static bool editSunShadowParameters(directional_light& sun)
{
	bool result = false;
	if (ImGui::BeginTree("Sun"))
	{
		if (ImGui::BeginProperties())
		{
			result |= ImGui::PropertySlider("Intensity", sun.intensity, 0.f, 1000.f);
			result |= ImGui::PropertyColorEdit("Color", sun.color);
			result |= ImGui::PropertySlider("# Cascades", sun.numShadowCascades, 1, 4);

			const float minCascadeDistance = 0.f, maxCascadeDistance = 300.f;
			const float minBias = 0.f, maxBias = 0.005f;
			const float minBlend = 0.f, maxBlend = 10.f;
			if (sun.numShadowCascades == 1)
			{
				result |= ImGui::PropertySlider("Distance", sun.cascadeDistances.x, minCascadeDistance, maxCascadeDistance);
				result |= ImGui::PropertySlider("Bias", sun.bias.x, minBias, maxBias, "%.6f");
				result |= ImGui::PropertySlider("Blend distances", sun.blendDistances.x, minBlend, maxBlend, "%.6f");
			}
			else if (sun.numShadowCascades == 2)
			{
				result |= ImGui::PropertySlider("Distance", sun.cascadeDistances.xy, minCascadeDistance, maxCascadeDistance);
				result |= ImGui::PropertySlider("Bias", sun.bias.xy, minBias, maxBias, "%.6f");
				result |= ImGui::PropertySlider("Blend distances", sun.blendDistances.xy, minBlend, maxBlend, "%.6f");
			}
			else if (sun.numShadowCascades == 3)
			{
				result |= ImGui::PropertySlider("Distance", sun.cascadeDistances.xyz, minCascadeDistance, maxCascadeDistance);
				result |= ImGui::PropertySlider("Bias", sun.bias.xyz, minBias, maxBias, "%.6f");
				result |= ImGui::PropertySlider("Blend distances", sun.blendDistances.xyz, minBlend, maxBlend, "%.6f");
			}
			else if (sun.numShadowCascades == 4)
			{
				result |= ImGui::PropertySlider("Distance", sun.cascadeDistances, minCascadeDistance, maxCascadeDistance);
				result |= ImGui::PropertySlider("Bias", sun.bias, minBias, maxBias, "%.6f");
				result |= ImGui::PropertySlider("Blend distances", sun.blendDistances, minBlend, maxBlend, "%.6f");
			}

			ImGui::EndProperties();
		}

		ImGui::EndTree();
	}
	return result;
}

static bool editSSR(bool& enable, ssr_settings& settings)
{
	bool result = false;
	if (ImGui::BeginProperties())
	{
		result |= ImGui::PropertyCheckbox("Enable SSR", enable);
		if (enable)
		{
			result |= ImGui::PropertySlider("Num iterations", settings.numSteps, 1, 1024);
			result |= ImGui::PropertySlider("Max distance", settings.maxDistance, 5.f, 1000.f);
			result |= ImGui::PropertySlider("Min. stride", settings.minStride, 1.f, 50.f);
			result |= ImGui::PropertySlider("Max. stride", settings.maxStride, settings.minStride, 50.f);
		}
		ImGui::EndProperties();
	}
	return result;
}

static bool editTAA(bool& enable, taa_settings& settings)
{
	bool result = false;
	if (ImGui::BeginProperties())
	{
		result |= ImGui::PropertyCheckbox("Enable TAA", enable);
		if (enable)
		{
			result |= ImGui::PropertySlider("Jitter strength", settings.cameraJitterStrength);
		}
		ImGui::EndProperties();
	}
	return result;
}

static bool editBloom(bool& enable, bloom_settings& settings)
{
	bool result = false;
	if (ImGui::BeginProperties())
	{
		result |= ImGui::PropertyCheckbox("Enable bloom", enable);
		if (enable)
		{
			result |= ImGui::PropertySlider("Bloom threshold", settings.threshold, 0.5f, 100.f);
			result |= ImGui::PropertySlider("Bloom strength", settings.strength);
		}
		ImGui::EndProperties();
	}
	return result;
}

static bool editSharpen(bool& enable, sharpen_settings& settings)
{
	bool result = false;
	if (ImGui::BeginProperties())
	{
		result |= ImGui::PropertyCheckbox("Enable sharpen", enable);
		if (enable)
		{
			result |= ImGui::PropertySlider("Sharpen strength", settings.strength);
		}
		ImGui::EndProperties();
	}
	return result;
}

static bool editFireParticleSystem(fire_particle_system& particleSystem)
{
	bool result = false;
	if (ImGui::BeginTree("Fire particles"))
	{
		if (ImGui::BeginProperties())
		{
			result |= ImGui::PropertySlider("Emit rate", particleSystem.emitRate, 0.f, 1000.f);
			ImGui::EndProperties();
		}

		result |= ImGui::Spline("Size over lifetime", ImVec2(200, 200), particleSystem.settings.sizeOverLifetime);
		ImGui::Separator();
		result |= ImGui::Spline("Atlas progression over lifetime", ImVec2(200, 200), particleSystem.settings.atlasProgressionOverLifetime);
		ImGui::Separator();
		result |= ImGui::Spline("Intensity over lifetime", ImVec2(200, 200), particleSystem.settings.intensityOverLifetime);

		ImGui::EndTree();
	}
	return result;
}

static bool editBoidParticleSystem(boid_particle_system& particleSystem)
{
	bool result = false;
	if (ImGui::BeginTree("Boid particles"))
	{
		if (ImGui::BeginProperties())
		{
			result |= ImGui::PropertySlider("Emit rate", particleSystem.emitRate, 0.f, 5000.f);
			result |= ImGui::PropertySlider("Emit radius", particleSystem.settings.radius, 5.f, 100.f);
			ImGui::EndProperties();
		}

		ImGui::EndTree();
	}
	return result;
}

void application::updateSelectedEntityUIRotation()
{
	if (selectedEntity)
	{
		quat rotation = quat::identity;

		if (transform_component* transform = selectedEntity.getComponentIfExists<transform_component>())
		{
			rotation = transform->rotation;
		}
		else if (position_rotation_component* prc = selectedEntity.getComponentIfExists<position_rotation_component>())
		{
			rotation = prc->rotation;
		}

		selectedEntityEulerRotation = quatToEuler(rotation);
		selectedEntityEulerRotation.x = rad2deg(angleToZeroToTwoPi(selectedEntityEulerRotation.x));
		selectedEntityEulerRotation.y = rad2deg(angleToZeroToTwoPi(selectedEntityEulerRotation.y));
		selectedEntityEulerRotation.z = rad2deg(angleToZeroToTwoPi(selectedEntityEulerRotation.z));
	}
}

void application::setSelectedEntity(scene_entity entity)
{
	if (selectedEntity != entity)
	{
		undoStack.pushAction("selection", selection_undo{ this, selectedEntity, entity });
	}

	setSelectedEntityNoUndo(entity);
}

void application::setSelectedEntityNoUndo(scene_entity entity)
{
	selectedEntity = entity;
	updateSelectedEntityUIRotation();
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
	if (auto* component = entity.getComponentIfExists<component_t>())
	{
		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		float lineHeight = ImGui::GetIO().Fonts->Fonts[0]->FontSize + ImGui::GetStyle().FramePadding.y * 2.f;
		ImGui::Separator();
		bool open = ImGui::TreeNodeEx((void*)typeid(component_t).hash_code(), treeNodeFlags, componentName);
		ImGui::PopStyleVar();

		if (open)
		{
			func(*component);
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
				drawComponent<transform_component>(selectedEntity, "Transform", [this, &objectMovedByWidget](transform_component& transform)
				{
					objectMovedByWidget |= ImGui::DragFloat3("Position", transform.position.data, 0.1f, 0.f, 0.f);

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

				drawComponent<position_component>(selectedEntity, "Transform", [&objectMovedByWidget](position_component& position)
				{
					objectMovedByWidget |= ImGui::DragFloat3("Position", position.position.data, 0.1f, 0.f, 0.f);
				});

				drawComponent<position_rotation_component>(selectedEntity, "Transform", [this, &objectMovedByWidget](position_rotation_component& pr)
				{
					objectMovedByWidget |= ImGui::DragFloat3("Translation", pr.position.data, 0.1f, 0.f, 0.f); 
					if (ImGui::DragFloat3("Rotation", selectedEntityEulerRotation.data, 0.1f, 0.f, 0.f))
					{
						vec3 euler = selectedEntityEulerRotation;
						euler.x = deg2rad(euler.x);
						euler.y = deg2rad(euler.y);
						euler.z = deg2rad(euler.z);
						pr.rotation = eulerToQuat(euler);

						objectMovedByWidget = true;
					}
				});

				drawComponent<dynamic_transform_component>(selectedEntity, "Dynamic", [](dynamic_transform_component& dynamic)
				{
					ImGui::Text("Dynamic");
				});

				drawComponent<animation_component>(selectedEntity, "Animation", [this](animation_component& anim)
				{
					if (raster_component* raster = selectedEntity.getComponentIfExists<raster_component>())
					{
						if (ImGui::BeginProperties())
						{
							uint32 animationIndex = anim.animation.clip ? (uint32)(anim.animation.clip - raster->mesh->skeleton.clips.data()) : -1;

							bool animationChanged = ImGui::PropertyDropdown("Currently playing", [](uint32 index, void* data)
							{
								if (index == -1) { return "---"; }

								animation_skeleton& skeleton = *(animation_skeleton*)data;
								const char* result = 0;
								if (index < (uint32)skeleton.clips.size())
								{
									result = skeleton.clips[index].name.c_str();
								}
								return result;
							}, animationIndex, &raster->mesh->skeleton);

							if (animationChanged)
							{
								anim.animation.set(&raster->mesh->skeleton.clips[animationIndex]);
							}

							ImGui::EndProperties();
						}
					}
				});

				drawComponent<rigid_body_component>(selectedEntity, "Rigid body", [this](rigid_body_component& rb)
				{
					if (ImGui::BeginProperties())
					{
						bool kinematic = rb.invMass == 0;
						if (ImGui::PropertyCheckbox("Kinematic", kinematic))
						{
							if (kinematic)
							{
								rb.invMass = 0.f;
								rb.invInertia = mat3::zero;
								rb.linearVelocity = vec3(0.f);
								rb.angularVelocity = vec3(0.f);
								rb.forceAccumulator = vec3(0.f);
								rb.torqueAccumulator = vec3(0.f);
							}
							else
							{
								rb.invMass = 1.f;
								rb.invInertia = mat3::identity;

								if (physics_reference_component* ref = selectedEntity.getComponentIfExists<physics_reference_component>())
								{
									rb.recalculateProperties(&appScene.registry, *ref);
								}
							}
						}

						ImGui::PropertySlider("Linear velocity damping", rb.linearDamping);
						ImGui::PropertySlider("Angular velocity damping", rb.angularDamping);
						ImGui::PropertySlider("Gravity factor", rb.gravityFactor);

						ImGui::EndProperties();
					}
				});

				drawComponent<cloth_component>(selectedEntity, "Cloth", [](cloth_component& cloth)
				{
					bool dirty = false;
					if (ImGui::BeginProperties())
					{
						dirty |= ImGui::PropertyInput("Total mass", cloth.totalMass);
						dirty |= ImGui::PropertySlider("Stiffness", cloth.stiffness, 0.01f, 0.7f);

						// These two don't need to notify the cloth on change.
						ImGui::PropertySlider("Velocity damping", cloth.damping, 0.f, 1.f);
						ImGui::PropertySlider("Gravity factor", cloth.gravityFactor, 0.f, 1.f);

						ImGui::EndProperties();
					}

					if (dirty)
					{
						cloth.recalculateProperties();
					}
				});

				drawComponent<point_light_component>(selectedEntity, "Point light", [](point_light_component& pl)
				{
					if (ImGui::BeginProperties())
					{
						ImGui::PropertyColorEdit("Color", pl.color);
						ImGui::PropertySlider("Intensity", pl.intensity, 0.f, 10.f);
						ImGui::PropertySlider("Radius", pl.radius, 0.f, 100.f);
						ImGui::PropertyCheckbox("Casts shadow", pl.castsShadow);
						if (pl.castsShadow)
						{
							ImGui::PropertyDropdownPowerOfTwo("Shadow resolution", 128, 2048, pl.shadowMapResolution);
						}

						ImGui::EndProperties();
					}
				});

				drawComponent<spot_light_component>(selectedEntity, "Spot light", [](spot_light_component& sl)
				{
					if (ImGui::BeginProperties())
					{
						float inner = rad2deg(sl.innerAngle);
						float outer = rad2deg(sl.outerAngle);

						ImGui::PropertyColorEdit("Color", sl.color);
						ImGui::PropertySlider("Intensity", sl.intensity, 0.f, 10.f);
						ImGui::PropertySlider("Distance", sl.distance, 0.f, 100.f);
						ImGui::PropertySlider("Inner angle", inner, 0.1f, 80.f);
						ImGui::PropertySlider("Outer angle", outer, 0.2f, 85.f);
						ImGui::PropertyCheckbox("Casts shadow", sl.castsShadow);
						if (sl.castsShadow)
						{
							ImGui::PropertyDropdownPowerOfTwo("Shadow resolution", 128, 2048, sl.shadowMapResolution);
						}

						sl.innerAngle = deg2rad(inner);
						sl.outerAngle = deg2rad(outer);

						ImGui::EndProperties();
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

		ImGui::Text("Video memory usage: %u / %uMB", memoryUsage.currentlyUsed, memoryUsage.available);

		ImGui::Dropdown("Aspect ratio", aspectRatioNames, aspect_ratio_mode_count, (uint32&)renderer->aspectRatioMode);

		plotAndEditTonemapping(renderer->tonemapSettings);
		editSunShadowParameters(sun);

		if (ImGui::BeginTree("Post processing"))
		{
			if (renderer->spec.allowSSR) { editSSR(renderer->enableSSR, renderer->ssrSettings); ImGui::Separator(); }
			if (renderer->spec.allowTAA) { editTAA(renderer->enableTAA, renderer->taaSettings); ImGui::Separator(); }
			if (renderer->spec.allowBloom) { editBloom(renderer->enableBloom, renderer->bloomSettings); ImGui::Separator(); }
			editSharpen(renderer->enableSharpen, renderer->sharpenSettings);

			ImGui::EndTree();
		}

		if (ImGui::BeginTree("Environment"))
		{
			if (ImGui::BeginProperties())
			{
				ImGui::PropertySlider("Environment intensity", renderer->environmentIntensity, 0.f, 2.f);
				ImGui::PropertySlider("Sky intensity", renderer->skyIntensity, 0.f, 2.f);
				ImGui::EndProperties();
			}

			ImGui::EndTree();
		}

		if (renderer->mode == renderer_mode_pathtraced)
		{
			bool pathTracerDirty = false;
			if (ImGui::BeginProperties())
			{
				pathTracerDirty |= ImGui::PropertySlider("Max recursion depth", pathTracer.recursionDepth, 0, pathTracer.maxRecursionDepth - 1);
				pathTracerDirty |= ImGui::PropertySlider("Start russian roulette after", pathTracer.startRussianRouletteAfter, 0, pathTracer.recursionDepth);
				pathTracerDirty |= ImGui::PropertyCheckbox("Use thin lens camera", pathTracer.useThinLensCamera);
				if (pathTracer.useThinLensCamera)
				{
					pathTracerDirty |= ImGui::PropertySlider("Focal length", pathTracer.focalLength, 0.5f, 50.f);
					pathTracerDirty |= ImGui::PropertySlider("F-Number", pathTracer.fNumber, 1.f, 128.f);
				}
				pathTracerDirty |= ImGui::PropertyCheckbox("Use real materials", pathTracer.useRealMaterials);
				pathTracerDirty |= ImGui::PropertyCheckbox("Enable direct lighting", pathTracer.enableDirectLighting);
				if (pathTracer.enableDirectLighting)
				{
					pathTracerDirty |= ImGui::PropertySlider("Light intensity scale", pathTracer.lightIntensityScale, 0.f, 50.f);
					pathTracerDirty |= ImGui::PropertySlider("Point light radius", pathTracer.pointLightRadius, 0.01f, 1.f);

					pathTracerDirty |= ImGui::PropertyCheckbox("Multiple importance sampling", pathTracer.multipleImportanceSampling);
				}

				ImGui::EndProperties();
			}


			if (pathTracerDirty)
			{
				pathTracer.numAveragedFrames = 0;
			}
		}
		else
		{
			if (ImGui::BeginTree("Particle systems"))
			{
				editFireParticleSystem(fireParticleSystem);
				editBoidParticleSystem(boidParticleSystem);

				ImGui::EndTree();
			}

			if (ImGui::BeginTree("Physics"))
			{
				if (ImGui::BeginProperties())
				{
					//ragdoll.edit();
					ImGui::PropertySlider("Rigid solver iterations", physicsSettings.numRigidSolverIterations, 1, 200);

					ImGui::PropertySlider("Cloth velocity iterations", physicsSettings.numClothVelocityIterations, 0, 10);
					ImGui::PropertySlider("Cloth position iterations", physicsSettings.numClothPositionIterations, 0, 10);
					ImGui::PropertySlider("Cloth drift iterations", physicsSettings.numClothDriftIterations, 0, 10);

					ImGui::PropertySlider("Test force", testPhysicsForce, 1.f, 10000.f);

					ImGui::EndProperties();
				}
				ImGui::EndTree();
			}
		}
	}

	ImGui::End();
}

void application::resetRenderPasses()
{
	opaqueRenderPass.reset();
	transparentRenderPass.reset();
	ldrRenderPass.reset();
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
}

void application::submitRenderPasses(uint32 numSpotLightShadowPasses, uint32 numPointLightShadowPasses)
{
	opaqueRenderPass.sort();
	transparentRenderPass.sort();
	ldrRenderPass.sort();

	renderer->submitRenderPass(&opaqueRenderPass);
	renderer->submitRenderPass(&transparentRenderPass);
	renderer->submitRenderPass(&ldrRenderPass);

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

	if (input.keyboard['F'].pressEvent && selectedEntity && selectedEntity.hasComponent<transform_component>())
	{
		auto& transform = selectedEntity.getComponent<transform_component>();

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


	if (selectedEntity)
	{
		if (transform_component* transform = selectedEntity.getComponentIfExists<transform_component>())
		{
			// Saved rigid-body properties. When an RB is dragged, we make it kinematic.
			static bool saved = false;
			static float invMass;

			bool draggingBefore = gizmo.dragging;

			if (gizmo.manipulateTransformation(*transform, camera, input, !inputCaptured, &ldrRenderPass))
			{
				updateSelectedEntityUIRotation();
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
					undoStack.pushAction("transform entity", transform_undo{ selectedEntity, gizmo.originalTransform, *transform });
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
		else if (position_component* pc = selectedEntity.getComponentIfExists<position_component>())
		{
			if (gizmo.manipulatePosition(pc->position, camera, input, !inputCaptured, &ldrRenderPass))
			{
				inputCaptured = true;
			}
		}
		else if (position_rotation_component* prc = selectedEntity.getComponentIfExists<position_rotation_component>())
		{
			if (gizmo.manipulatePositionRotation(prc->position, prc->rotation, camera, input, !inputCaptured, &ldrRenderPass))
			{
				inputCaptured = true;
			}
		}
		else
		{
			gizmo.manipulateNothing(camera, input, !inputCaptured, &ldrRenderPass);
		}

		if (!inputCaptured)
		{
			if (input.keyboard[key_ctrl].down && input.keyboard['D'].pressEvent)
			{
				// Duplicate entity.
				scene_entity newEntity = appScene.createEntity(selectedEntity.getComponent<tag_component>().name);
				appScene.copyComponentsIfExists<transform_component, raster_component, animation_component, raytrace_component>(selectedEntity, newEntity);
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
	else
	{
		gizmo.manipulateNothing(camera, input, !inputCaptured, &ldrRenderPass);
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
		for (auto [entityHandle, anim, raster, transform] : appScene.group(entt::get<animation_component, raster_component, transform_component>).each())
		{
			context.addWork([&anim = anim, mesh = raster.mesh, &transform = transform, dt]()
			{
				anim.update(mesh, dt, &transform);
			});
		}

		context.waitForWorkCompletion();


		// Render shadow maps.
		renderSunShadowMap(sun, &sunShadowRenderPass, appScene, objectDragged);

		uint32 numPointLights = appScene.numberOfComponentsOfType<point_light_component>();
		if (numPointLights)
		{
			auto* plPtr = (point_light_cb*)mapBuffer(pointLightBuffer[dxContext.bufferedFrameID], false);
			auto* siPtr = (point_shadow_info*)mapBuffer(pointLightShadowInfoBuffer[dxContext.bufferedFrameID], false);

			for (auto [entityHandle, position, pl] : appScene.group<position_component, point_light_component>().each())
			{
				point_light_cb cb;
				cb.initialize(position.position, pl.color * pl.intensity, pl.radius);

				if (pl.castsShadow)
				{
					cb.shadowInfoIndex = numPointShadowRenderPasses++;
					*siPtr++ = renderPointShadowMap(cb, (uint32)entityHandle, &pointShadowRenderPasses[cb.shadowInfoIndex], appScene, objectDragged, pl.shadowMapResolution);
				}

				*plPtr++ = cb;
			}

			unmapBuffer(pointLightBuffer[dxContext.bufferedFrameID], true, { 0, numPointLights });
			unmapBuffer(pointLightShadowInfoBuffer[dxContext.bufferedFrameID], true, { 0, numPointShadowRenderPasses });

			renderer->setPointLights(pointLightBuffer[dxContext.bufferedFrameID], numPointLights, pointLightShadowInfoBuffer[dxContext.bufferedFrameID]);
		}

		uint32 numSpotLights = appScene.numberOfComponentsOfType<spot_light_component>();
		if (numSpotLights)
		{
			auto* slPtr = (spot_light_cb*)mapBuffer(spotLightBuffer[dxContext.bufferedFrameID], false);
			auto* siPtr = (spot_shadow_info*)mapBuffer(spotLightShadowInfoBuffer[dxContext.bufferedFrameID], false);

			for (auto [entityHandle, transform, sl] : appScene.group<position_rotation_component, spot_light_component>().each())
			{
				spot_light_cb cb;
				cb.initialize(transform.position, transform.rotation * vec3(0.f, 0.f, -1.f), sl.color * sl.intensity, sl.innerAngle, sl.outerAngle, sl.distance);

				if (sl.castsShadow)
				{
					cb.shadowInfoIndex = numSpotShadowRenderPasses++;
					*siPtr++ = renderSpotShadowMap(cb, (uint32)entityHandle, &spotShadowRenderPasses[cb.shadowInfoIndex], appScene, objectDragged, sl.shadowMapResolution);
				}

				*slPtr++ = cb;
			}

			unmapBuffer(spotLightBuffer[dxContext.bufferedFrameID], true, { 0, numSpotLights });
			unmapBuffer(spotLightShadowInfoBuffer[dxContext.bufferedFrameID], true, { 0, numSpotShadowRenderPasses });

			renderer->setSpotLights(spotLightBuffer[dxContext.bufferedFrameID], numSpotLights, spotLightShadowInfoBuffer[dxContext.bufferedFrameID]);
		}



		// Submit render calls.
		for (auto [entityHandle, raster, transform] : appScene.group(entt::get<raster_component, transform_component>).each())
		{
			const dx_mesh& mesh = raster.mesh->mesh;
			mat4 m = trsToMat4(transform);

			scene_entity entity = { entityHandle, appScene };
			bool outline = selectedEntity == entity;

			dynamic_transform_component* dynamic = entity.getComponentIfExists<dynamic_transform_component>();
			mat4 lastM = dynamic ? trsToMat4(*dynamic) : m;

			if (animation_component* anim = entity.getComponentIfExists<animation_component>())
			{
				uint32 numSubmeshes = (uint32)raster.mesh->submeshes.size();

				for (uint32 i = 0; i < numSubmeshes; ++i)
				{
					submesh_info submesh = raster.mesh->submeshes[i].info;
					submesh.baseVertex -= raster.mesh->submeshes[0].info.baseVertex; // Vertex buffer from skinning already points to first vertex.

					const ref<pbr_material>& material = raster.mesh->submeshes[i].material;

					if (material->albedoTint.a < 1.f)
					{
						transparentRenderPass.renderObject(m, anim->currentVertexBuffer, mesh.indexBuffer, submesh, material);
					}
					else
					{
						opaqueRenderPass.renderAnimatedObject(m, lastM, 
							anim->currentVertexBuffer, anim->prevFrameVertexBuffer, mesh.indexBuffer,
							submesh, material,
							(uint32)entityHandle);
					}

					if (outline)
					{
						ldrRenderPass.renderOutline(m, anim->currentVertexBuffer, mesh.indexBuffer, submesh);
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
						ldrRenderPass.renderOutline(m, mesh.vertexBuffer, mesh.indexBuffer, submesh);
					}
				}
			}
		}

		for (auto [entityHandle, cloth] : appScene.view<cloth_component>().each())
		{
			auto [vb, prevFrameVB, ib, sm] = cloth.getRenderData();
			opaqueRenderPass.renderAnimatedObject(mat4::identity, mat4::identity, vb, prevFrameVB, ib, sm, clothMaterial, (uint32)entityHandle);

			scene_entity entity = { entityHandle, appScene };
			bool outline = selectedEntity == entity;

			if (outline)
			{
				ldrRenderPass.renderOutline(mat4::identity, vb, ib, sm);
			}
		}

		void collisionDebugDraw(ldr_render_pass* renderPass);
		collisionDebugDraw(&ldrRenderPass);


		if (decals.size())
		{
			updateUploadBufferData(decalBuffer[dxContext.bufferedFrameID], decals.data(), (uint32)(sizeof(pbr_decal_cb) * decals.size()));
			renderer->setDecals(decalBuffer[dxContext.bufferedFrameID], (uint32)decals.size(), decalTexture);
		}

		if (selectedEntity)
		{
			if (point_light_component* pl = selectedEntity.getComponentIfExists<point_light_component>())
			{
				position_component& pc = selectedEntity.getComponent<position_component>();

				renderWireSphere(pc.position, pl->radius, vec4(pl->color, 1.f), &ldrRenderPass);
			}
			else if (spot_light_component* sl = selectedEntity.getComponentIfExists<spot_light_component>())
			{
				position_rotation_component& prc = selectedEntity.getComponent<position_rotation_component>();

				renderWireCone(prc.position, prc.rotation * vec3(0.f, 0.f, -1.f), 
					sl->distance, sl->outerAngle * 2.f, vec4(sl->color, 1.f), &ldrRenderPass);
			}
		}

		submitRenderPasses(numSpotShadowRenderPasses, numPointShadowRenderPasses);
	}
	else
	{
		if (dxContext.featureSupport.raytracing())
		{
			raytracingTLAS.reset();

			for (auto [entityHandle, transform, raytrace] : appScene.group(entt::get<transform_component, raytrace_component>).each())
			{
				raytracingTLAS.instantiate(raytrace.type, transform);
			}

			raytracingTLAS.build();

			renderer->setRaytracer(&pathTracer, &raytracingTLAS);
		}
	}
#endif

	for (auto [entityHandle, transform, dynamic] : appScene.group(entt::get<transform_component, dynamic_transform_component>).each())
	{
		dynamic = transform;
	}
}

void application::setEnvironment(const fs::path& filename)
{
	environment = createEnvironment(filename); // Currently synchronous (on render queue).
	pathTracer.numAveragedFrames = 0;

	if (!environment)
	{
		std::cout << "Could not load environment '" << filename << "'. Renderer will use procedural sky box. Procedural sky boxes currently cannot contribute to global illumnation, so expect very dark lighting.\n";
	}
}

void application::handleFileDrop(const fs::path& filename)
{
	fs::path path = filename;
	fs::path relative = fs::relative(path, fs::current_path());

	auto mesh = loadMeshFromFile(relative.string());
	if (mesh)
	{
		fs::path path = filename;
		path = path.stem();

		appScene.createEntity(path.string().c_str())
			.addComponent<transform_component>(vec3(0.f), quat::identity)
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

		if (transform_component* transform = entity.getComponentIfExists<transform_component>())
		{
			out << YAML::Key << "Transform" << YAML::Value
				<< YAML::BeginMap
					<< YAML::Key << "Rotation" << YAML::Value << transform->rotation
					<< YAML::Key << "Position" << YAML::Value << transform->position
					<< YAML::Key << "Scale" << YAML::Value << transform->scale
				<< YAML::EndMap;
		}

		if (raster_component* raster = entity.getComponentIfExists<raster_component>())
		{
			out << YAML::Key << "Raster" << YAML::Value
				<< YAML::BeginMap 
					<< YAML::Key << "Mesh" << YAML::Value << raster->mesh->filepath
					<< YAML::Key << "Flags" << YAML::Value << raster->mesh->flags
					<< YAML::Key << "Animation files" << YAML::Value << YAML::BeginSeq;

			for (const fs::path& s : raster->mesh->skeleton.files)
			{
				out << s;
			}

			out		<< YAML::EndSeq
				<< YAML::EndMap;
		}

		if (animation_component* anim = entity.getComponentIfExists<animation_component>())
		{

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
	setEnvironment(environmentNode["Name"].as<fs::path>());
	renderer->environmentIntensity = environmentNode["Intensity"].as<float>();

	auto entitiesNode = data["Entities"];
	for (auto entityNode : entitiesNode)
	{
		std::string name = entityNode["Tag"].as<std::string>();
		scene_entity entity = appScene.createEntity(name.c_str());

		if (entityNode["Transform"])
		{
			auto transformNode = entityNode["Transform"];
			entity.addComponent<transform_component>(transformNode["Position"].as<vec3>(), transformNode["Rotation"].as<quat>(), transformNode["Scale"].as<vec3>());
		}

		if (entityNode["Raster"])
		{
			auto rasterNode = entityNode["Raster"];
			auto mesh = loadMeshFromFile(rasterNode["Mesh"].as<fs::path>(), rasterNode["Flags"].as<uint32>());

			auto animationsNode = rasterNode["Animation files"];
			for (auto file : animationsNode)
			{
				mesh->skeleton.pushAssimpAnimations(file.as<fs::path>());
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
