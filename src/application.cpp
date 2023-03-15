#include "pch.h"
#include "application.h"
#include "geometry/mesh_builder.h"
#include "dx/dx_texture.h"
#include "core/random.h"
#include "core/color.h"
#include "core/imgui.h"
#include "core/log.h"
#include "core/assimp.h"
#include "dx/dx_context.h"
#include "dx/dx_profiling.h"
#include "physics/physics.h"
#include "physics/ragdoll.h"
#include "physics/vehicle.h"
#include "core/threading.h"
#include "rendering/mesh_shader.h"
#include "rendering/shadow_map.h"
#include "rendering/debug_visualization.h"
#include "audio/audio.h"
#include "terrain/terrain.h"
#include "terrain/heightmap_collider.h"
#include "terrain/proc_placement.h"
#include "terrain/grass.h"
#include "terrain/water.h"
#include "terrain/tree.h"
#include "animation/skinning.h"


static raytracing_object_type defineBlasFromMesh(const ref<multi_mesh>& mesh)
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
		raytracing_object_type type = pbr_raytracer::defineObjectType(blas, raytracingMaterials);
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

void application::initialize(main_renderer* renderer, editor_panels* editorPanels)
{
	this->renderer = renderer;

	if (dxContext.featureSupport.raytracing())
	{
		raytracingTLAS.initialize();
	}

	scene.camera.initializeIngame(vec3(0.f, 1.f, 5.f), quat::identity, deg2rad(70.f), 0.1f);
	scene.environment.setFromTexture("assets/sky/sunset_in_the_chalk_quarry_4k.hdr");
	scene.environment.lightProbeGrid.initialize(vec3(-20.f, -1.f, -20.f), vec3(40.f, 20.f, 40.f), 1.5f);

	editor.initialize(&this->scene, renderer, editorPanels);

	game_scene& scene = this->scene.getCurrentScene();

#if 1
	if (auto mesh = loadMeshFromFile("assets/sponza/sponza.obj"))
	{
		auto blas = defineBlasFromMesh(mesh);

		auto sponza = scene.createEntity("Sponza")
			.addComponent<transform_component>(vec3(0.f, 0.f, 0.f), quat::identity, 0.01f)
			.addComponent<mesh_component>(mesh);

		if (blas.blas)
		{
			sponza.addComponent<raytrace_component>(blas);
		}
	}
#endif


	if (auto stormtrooperMesh = loadAnimatedMeshFromFile("assets/stormtrooper/stormtrooper.fbx"))
	{
		auto stormtrooper = scene.createEntity("Stormtrooper 1")
			.addComponent<transform_component>(vec3(-5.f, 0.f, -1.f), quat::identity)
			.addComponent<mesh_component>(stormtrooperMesh)
			.addComponent<animation_component>()
			.addComponent<dynamic_transform_component>();


		stormtrooper.getComponent<animation_component>().animation.set(&stormtrooperMesh->skeleton.clips[0]);
	}

	if (auto pilotMesh = loadAnimatedMeshFromFile("assets/pilot/pilot.fbx"))
	{
		auto pilot = scene.createEntity("Pilot")
			.addComponent<transform_component>(vec3(2.5f, 0.f, -1.f), quat::identity, 0.2f)
			.addComponent<mesh_component>(pilotMesh)
			.addComponent<animation_component>()
			.addComponent<dynamic_transform_component>();

		pilot.getComponent<animation_component>().animation.set(&pilotMesh->skeleton.clips[0]);
	}

#if 1
	if (auto treeMesh = loadTreeMeshFromFile("assets/tree/chestnut/chestnut.fbx"))
	{
		auto tree = scene.createEntity("Chestnut")
			.addComponent<transform_component>(vec3(2.5f, 0.f, -1.f), quat::identity, 0.2f)
			.addComponent<mesh_component>(treeMesh)
			.addComponent<tree_component>();
	}
#endif


#if 1
	{
		auto woodMaterial = createPBRMaterial(
			"assets/desert/textures/WoodenCrate2_Albedo.png",
			"assets/desert/textures/WoodenCrate2_Normal.png",
			{}, {});


		mesh_builder builder;

		auto sphereMesh = make_ref<multi_mesh>();
		builder.pushSphere({ });
		sphereMesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity, woodMaterial });

		auto boxMesh = make_ref<multi_mesh>();
		builder.pushBox({ vec3(0.f), vec3(1.f, 1.f, 2.f) });
		boxMesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity, woodMaterial });



		auto lollipopMaterial = createPBRMaterial(
			"assets/cc0/sphere/Tiles074_2K_Color.jpg",
			"assets/cc0/sphere/Tiles074_2K_Normal.jpg",
			"assets/cc0/sphere/Tiles074_2K_Roughness.jpg",
			{}, vec4(0.f), vec4(1.f), 0.2f, 0.5f, pbr_material_shader_default, 3.f);

		auto groundMesh = make_ref<multi_mesh>();
		builder.pushBox({ vec3(0.f), vec3(30.f, 4.f, 30.f) });
		groundMesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity, lollipopMaterial });

		random_number_generator rng = { 15681923 };
		for (uint32 i = 0; i < 3; ++i)
		{
			//float x = rng.randomFloatBetween(-90.f, 90.f);
			//float z = rng.randomFloatBetween(-90.f, 90.f);
			//float y = rng.randomFloatBetween(20.f, 60.f);

			//if (i % 2 == 0)
			//{
			//	scene.createEntity("Cube")
			//		.addComponent<transform_component>(vec3(25.f, 10.f + i * 3.f, -5.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(1.f)))
			//		.addComponent<mesh_component>(boxMesh)
			//		.addComponent<collider_component>(collider_component::asAABB(bounding_box::fromCenterRadius(vec3(0.f, 0.f, 0.f), vec3(1.f, 1.f, 2.f)), { physics_material_type_wood, 0.1f, 0.5f, 1.f }))
			//		.addComponent<rigid_body_component>(false, 1.f);
			//}
			//else
			{
				scene.createEntity("Sphere")
					.addComponent<transform_component>(vec3(25.f, 10.f + i * 3.f, -5.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(1.f)), vec3(1.f))
					.addComponent<mesh_component>(sphereMesh)
					.addComponent<collider_component>(collider_component::asSphere({ vec3(0.f, 0.f, 0.f), 1.f }, { physics_material_type_wood, 0.1f, 0.5f, 1.f }))
					.addComponent<rigid_body_component>(false, 1.f);
			}
		}

		auto triggerCallback = [scene = &scene](trigger_event e)
		{
			//scene->deleteEntity(e.other);
			//std::cout << ((e.type == trigger_event_enter) ? "Enter" : "Leave") << '\n';
		};

		scene.createEntity("Trigger")
			.addComponent<collider_component>(collider_component::asAABB(bounding_box::fromCenterRadius(vec3(25.f, 1.f, -5.f), vec3(5.f, 1.f, 5.f)), { physics_material_type_none, 0, 0, 0 }))
			.addComponent<trigger_component>(triggerCallback);

#if 1
		editor.physicsSettings.collisionBeginCallback = [rng = random_number_generator{ 519431 }](const collision_begin_event& e) mutable
		{
			float speed = length(e.relativeVelocity);

			sound_settings settings;
			settings.pitch = rng.randomFloatBetween(0.5f, 1.5f);
			settings.volume = saturate(remap(speed, 0.2f, 20.f, 0.f, 1.f));

			play3DSound(SOUND_ID("Collision"), e.position, settings);
		};

		//editor.physicsSettings.collisionEndCallback = [](const collision_end_event& e) mutable
		//{
		//};
#endif


		scene.createEntity("Platform")
			.addComponent<transform_component>(vec3(10, -4.f, 0.f), quat(vec3(1.f, 0.f, 0.f), deg2rad(0.f)))
			.addComponent<mesh_component>(groundMesh)
			.addComponent<collider_component>(collider_component::asAABB(bounding_box::fromCenterRadius(vec3(0.f, 0.f, 0.f), vec3(30.f, 4.f, 30.f)), { physics_material_type_metal, 0.1f, 1.f, 4.f }));


		auto chainMesh = make_ref<multi_mesh>();
#if 0
		builder.pushCapsule(capsule_mesh_desc(vec3(0.f, -1.f, 0.f), vec3(0.f, 1.f, 0.f), 0.18f));
		chainMesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity, lollipopMaterial });

		auto fixed = scene.createEntity("Fixed")
			.addComponent<transform_component>(vec3(37.f, 15.f, -2.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(90.f)))
			.addComponent<mesh_component>(chainMesh)
			.addComponent<collider_component>(collider_component::asCapsule({ vec3(0.f, -1.f, 0.f), vec3(0.f, 1.f, 0.f), 0.18f }, { 0.2f, 0.5f, 1.f }))
			.addComponent<rigid_body_component>(true, 1.f);

		//fixed.getComponent<rigid_body_component>().angularVelocity = vec3(0.f, 0.1f, 0.f);
		//fixed.getComponent<rigid_body_component>().angularDamping = 0.f;

		auto prev = fixed;

		for (uint32 i = 0; i < 10; ++i)
		{
			float xPrev = 37.f + 2.5f * i;
			float xCurr = 37.f + 2.5f * (i + 1);

			auto chain = scene.createEntity("Chain")
				.addComponent<transform_component>(vec3(xCurr, 15.f, -2.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(90.f)))
				.addComponent<mesh_component>(chainMesh)
				.addComponent<collider_component>(collider_component::asCapsule({ vec3(0.f, -1.f, 0.f), vec3(0.f, 1.f, 0.f), 0.18f }, { 0.2f, 0.5f, 1.f }))
				.addComponent<rigid_body_component>(false, 1.f);

			//addHingeConstraintFromGlobalPoints(prev, chain, vec3(xPrev + 1.18f, 15.f, -2.f), vec3(0.f, 0.f, 1.f), deg2rad(5.f), deg2rad(20.f));
			addConeTwistConstraintFromGlobalPoints(prev, chain, vec3(xPrev + 1.18f, 15.f, -2.f), vec3(1.f, 0.f, 0.f), deg2rad(20.f), deg2rad(30.f));

			prev = chain;
		}
#endif


		groundMesh->mesh =
			boxMesh->mesh =
			sphereMesh->mesh =
			chainMesh->mesh =
			builder.createDXMesh();
	}
#endif




#if 1
	{
		auto sphereMesh = make_ref<multi_mesh>();
		auto boxMesh = make_ref<multi_mesh>();

		{
			mesh_builder builder;
			builder.pushSphere({ });
			sphereMesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity });
			sphereMesh->mesh = builder.createDXMesh();
		}
		{
			mesh_builder builder;
			builder.pushBox({ vec3(0.f), vec3(1.f, 1.f, 2.f) });
			boxMesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity });
			boxMesh->mesh = builder.createDXMesh();
		}

		uint32 numTerrainChunks = 10;
		float terrainChunkSize = 64.f;

		auto terrainGroundMaterial = createPBRMaterial("assets/terrain/ground/Grass002_2K_Color.png", "assets/terrain/ground/Grass002_2K_NormalDX.png", "assets/terrain/ground/Grass002_2K_Roughness.png", {},
			vec4(0.f), vec4(1.f), 1.f, 0.f, pbr_material_shader_default, 1.f, true);
		auto terrainRockMaterial = createPBRMaterial("assets/terrain/rock/Rock034_2K_Color.png", "assets/terrain/rock/Rock034_2K_NormalDX.png", "assets/terrain/rock/Rock034_2K_Roughness.png", {});
		auto terrainMudMaterial = createPBRMaterial("assets/terrain/mud/Ground049B_2K_Color.png", "assets/terrain/mud/Ground049B_2K_NormalDX.png", "assets/terrain/mud/Ground049B_2K_Roughness.png", {});

		std::vector<proc_placement_layer_desc> layers =
		{
			proc_placement_layer_desc {
				"Trees and rocks",
				5.f,
				{ 
					loadMeshFromFile("assets/hoewa/hoewa1.fbx"), 
					loadMeshFromFile("assets/hoewa/hoewa2.fbx"),
					loadMeshFromFile("assets/desert/rock1.fbx"),
					loadMeshFromFile("assets/desert/rock4.fbx"),
				}
			}
		};

		auto terrain = scene.createEntity("Terrain")
			.addComponent<position_component>(vec3(0.f, -64.f, 0.f))
			.addComponent<terrain_component>(numTerrainChunks, terrainChunkSize, 50.f, terrainGroundMaterial, terrainRockMaterial, terrainMudMaterial)
			.addComponent<heightmap_collider_component>(numTerrainChunks, terrainChunkSize, physics_material{ physics_material_type_metal, 0.1f, 1.f, 4.f })
			//.addComponent<proc_placement_component>(layers)
			.addComponent<grass_component>()
			;

		auto water = scene.createEntity("Water")
			.addComponent<position_scale_component>(vec3(-3.920f, -48.689f, -85.580f), vec3(90.f))
			.addComponent<water_component>();
	}
#endif




	random_number_generator rng = { 14878213 };

	this->scene.sun.direction = normalize(vec3(-0.6f, -1.f, -0.3f));
	this->scene.sun.color = vec3(1.f, 0.93f, 0.76f);
	this->scene.sun.intensity = 6.f;

	this->scene.sun.numShadowCascades = 3;
	this->scene.sun.shadowDimensions = 2048;
	this->scene.sun.cascadeDistances = vec4(9.f, 25.f, 50.f, 10000.f);
	this->scene.sun.bias = vec4(0.000588f, 0.000784f, 0.000824f, 0.0035f);
	this->scene.sun.blendDistances = vec4(5.f, 10.f, 10.f, 10.f);
	this->scene.sun.stabilize = true;

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

#if 0
	fireParticleSystem.initialize(10000, 50.f, "assets/particles/fire_explosion.tif", 6, 6);
	smokeParticleSystem.initialize(10000, 500.f, "assets/particles/smoke1.tif", 5, 5);
	boidParticleSystem.initialize(10000, 2000.f);
	debrisParticleSystem.initialize(10000);
#endif

	stackArena.initialize();
}

#if 0
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
#endif

void application::resetRenderPasses()
{
	opaqueRenderPass.reset();
	transparentRenderPass.reset();
	ldrRenderPass.reset();
	sunShadowRenderPass.reset();
	computePass.reset();

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

void application::submitRendererParams(uint32 numSpotLightShadowPasses, uint32 numPointLightShadowPasses)
{
	{
		CPU_PROFILE_BLOCK("Sort render passes");

		opaqueRenderPass.sort();
		transparentRenderPass.sort();
		ldrRenderPass.sort();

		for (uint32 i = 0; i < sunShadowRenderPass.numCascades; ++i)
		{
			sunShadowRenderPass.cascades[i].sort();
		}

		for (uint32 i = 0; i < numSpotLightShadowPasses; ++i)
		{
			spotShadowRenderPasses[i].sort();
		}

		for (uint32 i = 0; i < numPointLightShadowPasses; ++i)
		{
			pointShadowRenderPasses[i].sort();
		}
	}

	renderer->submitRenderPass(&opaqueRenderPass);
	renderer->submitRenderPass(&transparentRenderPass);
	renderer->submitRenderPass(&ldrRenderPass);
	renderer->submitComputePass(&computePass);

	renderer->submitShadowRenderPass(&sunShadowRenderPass);

	for (uint32 i = 0; i < numSpotLightShadowPasses; ++i)
	{
		renderer->submitShadowRenderPass(&spotShadowRenderPasses[i]);
	}

	for (uint32 i = 0; i < numPointLightShadowPasses; ++i)
	{
		renderer->submitShadowRenderPass(&pointShadowRenderPasses[i]);
	}
}

void application::update(const user_input& input, float dt)
{
	resetRenderPasses();

	stackArena.reset();

	//learnedLocomotion.update(scene);

	bool objectDragged = editor.update(input, &ldrRenderPass, dt);



	render_camera& camera = scene.camera;
	directional_light& sun = scene.sun;
	pbr_environment& environment = scene.environment;

	environment.update(sun.direction);
	sun.updateMatrices(camera);
	setAudioListener(camera.position, camera.rotation, vec3(0.f));
	environment.lightProbeGrid.visualize(&opaqueRenderPass);


	game_scene& scene = this->scene.getCurrentScene();
	float unscaledDt = dt;
	dt *= this->scene.getTimestepScale();


	// Must happen before physics update.
	for (auto [entityHandle, terrain, position] : scene.group(component_group<terrain_component, position_component>).each())
	{
		scene_entity entity = { entityHandle, scene };
		heightmap_collider_component* collider = entity.getComponentIfExists<heightmap_collider_component>();

		terrain.update(position.position, collider);
	}


	static float physicsTimer = 0.f;
	physicsStep(scene, stackArena, physicsTimer, editor.physicsSettings, dt);


	// Particles.

#if 0
	if (input.keyboard['T'].pressEvent)
	{
		debrisParticleSystem.burst(camera.position + camera.rotation * vec3(0.f, 0.f, -3.f));
	}

	computePass.dt = dt;
	computePass.updateParticleSystem(&boidParticleSystem);
	computePass.updateParticleSystem(&fireParticleSystem);
	computePass.updateParticleSystem(&smokeParticleSystem);
	computePass.updateParticleSystem(&debrisParticleSystem);

	boidParticleSystem.render(&transparentRenderPass);
	fireParticleSystem.render(&transparentRenderPass);
	smokeParticleSystem.render(&transparentRenderPass);
	debrisParticleSystem.render(&transparentRenderPass);

#endif


	// Set global rendering stuff.






	if (dxContext.featureSupport.raytracing())
	{
		raytracingTLAS.reset();

		for (auto [entityHandle, transform, raytrace] : scene.group(component_group<transform_component, raytrace_component>).each())
		{
			raytracingTLAS.instantiate(raytrace.type, transform);
		}

		renderer->setRaytracingScene(&raytracingTLAS);
	}



	renderer->setRaytracingScene(&raytracingTLAS);
	renderer->setEnvironment(environment);
	renderer->setSun(sun);
	renderer->setCamera(camera);


	scene_entity selectedEntity = editor.selectedEntity;

	if (renderer->mode != renderer_mode_pathtraced)
	{
		if (dxContext.featureSupport.meshShaders())
		{
			testRenderMeshShader(&transparentRenderPass, dt);
		}

		thread_job_context context;

		// Update animated meshes.
		for (auto [entityHandle, anim, mesh, transform] : scene.group(component_group<animation_component, mesh_component, transform_component>).each())
		{
			context.addWork([&anim = anim, mesh = mesh.mesh, &transform = transform, &arena = stackArena, dt]()
			{
				anim.update(mesh, arena, dt, &transform);
			});
		}

		context.waitForWorkCompletion();

		//for (auto [entityHandle, anim, raster, transform] : scene.group(component_group<animation_component, mesh_component, transform_component>).each())
		//{
		//	anim.drawCurrentSkeleton(raster.mesh, transform, &ldrRenderPass);
		//}


		// Render shadow maps.
		renderSunShadowMap(sun, &sunShadowRenderPass, scene, objectDragged);

		uint32 numPointLights = scene.numberOfComponentsOfType<point_light_component>();
		if (numPointLights)
		{
			auto* plPtr = (point_light_cb*)mapBuffer(pointLightBuffer[dxContext.bufferedFrameID], false);
			auto* siPtr = (point_shadow_info*)mapBuffer(pointLightShadowInfoBuffer[dxContext.bufferedFrameID], false);

			for (auto [entityHandle, position, pl] : scene.group<position_component, point_light_component>().each())
			{
				point_light_cb cb;
				cb.initialize(position.position, pl.color * pl.intensity, pl.radius);

				if (pl.castsShadow)
				{
					cb.shadowInfoIndex = numPointShadowRenderPasses++;
					*siPtr++ = renderPointShadowMap(cb, (uint32)entityHandle, &pointShadowRenderPasses[cb.shadowInfoIndex], scene, objectDragged, pl.shadowMapResolution);
				}

				*plPtr++ = cb;
			}

			unmapBuffer(pointLightBuffer[dxContext.bufferedFrameID], true, { 0, numPointLights });
			unmapBuffer(pointLightShadowInfoBuffer[dxContext.bufferedFrameID], true, { 0, numPointShadowRenderPasses });

			renderer->setPointLights(pointLightBuffer[dxContext.bufferedFrameID], numPointLights, pointLightShadowInfoBuffer[dxContext.bufferedFrameID]);
		}

		uint32 numSpotLights = scene.numberOfComponentsOfType<spot_light_component>();
		if (numSpotLights)
		{
			auto* slPtr = (spot_light_cb*)mapBuffer(spotLightBuffer[dxContext.bufferedFrameID], false);
			auto* siPtr = (spot_shadow_info*)mapBuffer(spotLightShadowInfoBuffer[dxContext.bufferedFrameID], false);

			for (auto [entityHandle, transform, sl] : scene.group<position_rotation_component, spot_light_component>().each())
			{
				spot_light_cb cb;
				cb.initialize(transform.position, transform.rotation * vec3(0.f, 0.f, -1.f), sl.color * sl.intensity, sl.innerAngle, sl.outerAngle, sl.distance);

				if (sl.castsShadow)
				{
					cb.shadowInfoIndex = numSpotShadowRenderPasses++;
					*siPtr++ = renderSpotShadowMap(cb, (uint32)entityHandle, &spotShadowRenderPasses[cb.shadowInfoIndex], scene, objectDragged, sl.shadowMapResolution);
				}

				*slPtr++ = cb;
			}

			unmapBuffer(spotLightBuffer[dxContext.bufferedFrameID], true, { 0, numSpotLights });
			unmapBuffer(spotLightShadowInfoBuffer[dxContext.bufferedFrameID], true, { 0, numSpotShadowRenderPasses });

			renderer->setSpotLights(spotLightBuffer[dxContext.bufferedFrameID], numSpotLights, spotLightShadowInfoBuffer[dxContext.bufferedFrameID]);
		}



		for (auto [entityHandle, terrain, position, placement] : scene.group(component_group<terrain_component, position_component, proc_placement_component>).each())
		{
			placement.generate(this->scene.camera, terrain, position.position);
			placement.render(&ldrRenderPass);
		}

		for (auto [entityHandle, terrain, position, grass] : scene.group(component_group<terrain_component, position_component, grass_component>).each())
		{
			grass.generate(&computePass, this->scene.camera, terrain, position.position, unscaledDt);
			grass.render(&opaqueRenderPass, (uint32)entityHandle);
		}

		position_scale_component waterPlaneTransforms[4];
		uint32 numWaterPlanes = 0;

		for (auto [entityHandle, water, transform] : scene.group(component_group<water_component, position_scale_component>).each())
		{
			water.update(unscaledDt);
			water.render(this->scene.camera, &transparentRenderPass, transform.position, vec2(transform.scale.x, transform.scale.z), (uint32)entityHandle);

			waterPlaneTransforms[numWaterPlanes++] = transform;
		}

		for (auto [entityHandle, terrain, position] : scene.group(component_group<terrain_component, position_component>).each())
		{
			terrain.render(this->scene.camera, &opaqueRenderPass, sunShadowRenderPass.copyFromStaticCache ? 0 : &sunShadowRenderPass, position.position, (uint32)entityHandle,
				waterPlaneTransforms, numWaterPlanes);
		}




		{
			CPU_PROFILE_BLOCK("Submit render commands");


			// Animated meshes.
			for (auto [entityHandle, transform, dynamicTransform, mesh, anim] : scene.group(
				component_group<transform_component, dynamic_transform_component, mesh_component, animation_component>)
				.each())
			{
				if (!mesh.mesh)
				{
					continue;
				}

				const dx_mesh& dxMesh = mesh.mesh->mesh;
				mat4 m = trsToMat4(transform);

				scene_entity entity = { entityHandle, scene };
				bool outline = selectedEntity == entity;

				mat4 lastM = trsToMat4(dynamicTransform);

				for (auto& sm : mesh.mesh->submeshes)
				{
					submesh_info submesh = sm.info;
					submesh.baseVertex -= mesh.mesh->submeshes[0].info.baseVertex; // Vertex buffer from skinning already points to first vertex.

					const ref<pbr_material>& material = sm.material;

					if (material->albedoTint.a < 1.f)
					{
						transparentRenderPass.renderObject(m, anim.currentVertexBuffer, dxMesh.indexBuffer, submesh, material);
					}
					else
					{
						opaqueRenderPass.renderAnimatedObject(m, lastM,
							anim.currentVertexBuffer, anim.prevFrameVertexBuffer, dxMesh.indexBuffer,
							submesh, material,
							(uint32)entityHandle);
					}

					if (outline)
					{
						ldrRenderPass.renderOutline(m, anim.currentVertexBuffer, dxMesh.indexBuffer, submesh);
					}
				}
			};


			// Dynamic meshes.
			for (auto [entityHandle, transform, dynamicTransform, mesh] : scene.group(
				component_group<transform_component, dynamic_transform_component, mesh_component>,
				component_group<animation_component>) // Exclude animated meshes, since we already rendered these.
				.each())
			{
				if (!mesh.mesh)
				{
					continue;
				}

				const dx_mesh& dxMesh = mesh.mesh->mesh;
				mat4 m = trsToMat4(transform);

				scene_entity entity = { entityHandle, scene };
				bool outline = selectedEntity == entity;

				mat4 lastM = trsToMat4(dynamicTransform);

				for (auto& sm : mesh.mesh->submeshes)
				{
					if (sm.material->albedoTint.a < 1.f)
					{
						transparentRenderPass.renderObject(m, dxMesh.vertexBuffer, dxMesh.indexBuffer, sm.info, sm.material);
					}
					else
					{
						opaqueRenderPass.renderDynamicObject(m, lastM, dxMesh.vertexBuffer, dxMesh.indexBuffer, sm.info, sm.material, (uint32)entityHandle);
					}

					if (outline)
					{
						ldrRenderPass.renderOutline(m, dxMesh.vertexBuffer, dxMesh.indexBuffer, sm.info);
					}
				}
			}


			// Trees.
			for (auto [entityHandle, transform, mesh, tree] : scene.group(
				component_group<transform_component, mesh_component, tree_component>)
				.each())
			{
				if (!mesh.mesh)
				{
					continue;
				}

				scene_entity entity = { entityHandle, scene };
				bool outline = selectedEntity == entity;

				tree.render(&opaqueRenderPass, transform, mesh.mesh, unscaledDt);

				if (outline)
				{
					for (auto& sm : mesh.mesh->submeshes)
					{
						ldrRenderPass.renderOutline(trsToMat4(transform), mesh.mesh->mesh.vertexBuffer, mesh.mesh->mesh.indexBuffer, sm.info);
					}
				}
			}




			using specialized_components = component_group_t<
				animation_component,
				dynamic_transform_component,
				tree_component
			>;

			// Render all other objects as default.
			for (auto [entityHandle, transform, mesh] : scene.group(
				component_group<transform_component, mesh_component>,
				specialized_components{}) // Exclude all already handled components.
				.each())
			{
				if (!mesh.mesh)
				{
					continue;
				}

				const dx_mesh& dxMesh = mesh.mesh->mesh;
				mat4 m = trsToMat4(transform);

				scene_entity entity = { entityHandle, scene };
				bool outline = selectedEntity == entity;

				for (auto& sm : mesh.mesh->submeshes)
				{
					if (sm.material->albedoTint.a < 1.f)
					{
						transparentRenderPass.renderObject(m, dxMesh.vertexBuffer, dxMesh.indexBuffer, sm.info, sm.material);
					}
					else
					{
						opaqueRenderPass.renderStaticObject(m, dxMesh.vertexBuffer, dxMesh.indexBuffer, sm.info, sm.material, (uint32)entityHandle);
					}

					if (outline)
					{
						ldrRenderPass.renderOutline(m, dxMesh.vertexBuffer, dxMesh.indexBuffer, sm.info);
					}
				}
			}


			for (auto [entityHandle, cloth, render] : scene.group<cloth_component, cloth_render_component>().each())
			{
				static auto clothMaterial = createPBRMaterial(
					"assets/sponza/textures/Sponza_Curtain_Red_diffuse.tga",
					"assets/sponza/textures/Sponza_Curtain_Red_normal.tga",
					"assets/sponza/textures/Sponza_Curtain_roughness.tga",
					"assets/sponza/textures/Sponza_Curtain_metallic.tga",
					vec4(0.f), vec4(1.f), 1.f, 1.f, pbr_material_shader_double_sided);

				auto [vb, prevFrameVB, ib, sm] = render.getRenderData(cloth);
				opaqueRenderPass.renderAnimatedObject(mat4::identity, mat4::identity, vb, prevFrameVB, ib, sm, clothMaterial, (uint32)entityHandle);

				scene_entity entity = { entityHandle, scene };
				bool outline = selectedEntity == entity;

				if (outline)
				{
					ldrRenderPass.renderOutline(mat4::identity, vb, ib, sm);
				}
			}
		}


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

		submitRendererParams(numSpotShadowRenderPasses, numPointShadowRenderPasses);
	}

	for (auto [entityHandle, transform, dynamic] : scene.group(component_group<transform_component, dynamic_transform_component>).each())
	{
		dynamic = transform;
	}

	performSkinning(&computePass);
}

void application::handleFileDrop(const fs::path& filename)
{
	fs::path path = filename;
	fs::path relative = fs::relative(path, fs::current_path());
	fs::path ext = relative.extension();

	if (isMeshExtension(ext))
	{
		if (auto mesh = loadMeshFromFile(relative.string()))
		{
			fs::path path = filename;
			path = path.stem();

			scene.getCurrentScene().createEntity(path.string().c_str())
				.addComponent<transform_component>(vec3(0.f), quat::identity)
				.addComponent<mesh_component>(mesh);
		}
	}
	else if (ext == ".hdr")
	{
		scene.environment.setFromTexture(relative);
	}
}
