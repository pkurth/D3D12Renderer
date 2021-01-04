#include "pch.h"
#include "application.h"
#include "geometry.h"
#include "dx_texture.h"
#include "random.h"
#include "color.h"
#include "imgui.h"
#include "dx_context.h"
#include "skinning.h"


struct raster_component
{
	ref<composite_mesh> mesh;
};

struct animation_component
{
	float time;

	ref<dx_vertex_buffer> vb;
	submesh_info sm;

	ref<dx_vertex_buffer> prevFrameVB;
	submesh_info prevFrameSM;
};


void application::initialize(dx_renderer* renderer)
{
	this->renderer = renderer;

	camera.initializeIngame(vec3(0.f, 30.f, 40.f), quat::identity, deg2rad(70.f), 0.1f);
	cameraController.initialize(&camera);

	
	// Sponza.
	scene.createEntity("Sponza")
		.addComponent<trs>(vec3(0.f, 0.f, 0.f), quat::identity, 0.01f)
		.addComponent<raster_component>(loadMeshFromFile("assets/meshes/sponza.obj"));


	// Stormtroopers.
	auto stormtrooperMesh = loadMeshFromFile("assets/meshes/stormtrooper.fbx",
		mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents | mesh_creation_flags_with_skin);

	stormtrooperMesh->submeshes[0].material = createPBRMaterial(
		"assets/textures/stormtrooper/Stormtrooper_D.png",
		0, 0, 0,
		vec4(1.f, 1.f, 1.f, 1.f),
		0.f,
		0.f
	);

	scene.createEntity("Stormtrooper 1")
		.addComponent<trs>(vec3(0.f), quat::identity)
		.addComponent<raster_component>(stormtrooperMesh)
		.addComponent<animation_component>(0.f);

	scene.createEntity("Stormtrooper 2")
		.addComponent<trs>(vec3(5.f, 0.f, 0.f), quat::identity)
		.addComponent<raster_component>(stormtrooperMesh)
		.addComponent<animation_component>(1.5f);



	// Raytracing.
	/*if (dxContext.raytracingSupported)
	{
		raytracingBindingTable.initialize();
		raytracingTLAS.initialize(acceleration_structure_refit);

		std::vector<raytracing_object_handle> types;
		for (auto& m : meshes)
		{
			raytracing_blas_builder blasBuilder;
			std::vector<ref<pbr_material>> raytracingMaterials;

			for (auto& sm : m.submeshes)
			{
				blasBuilder.push(m.mesh.vertexBuffer, m.mesh.indexBuffer, sm.info);
				raytracingMaterials.push_back(sm.material);
			}

			blas.push_back(blasBuilder.finish());
			types.push_back(raytracingBindingTable.defineObjectType(blas.back(), raytracingMaterials));
		}

		raytracingTLAS.instantiate(types[0], sponzaTransform);


		raytracingBindingTable.build();
		raytracingTLAS.build();
	}*/






	setEnvironment("assets/textures/hdri/sunset_in_the_chalk_quarry_4k.hdr");

	pointLights = new point_light_cb[numPointLights];
	lightVelocities = new vec3[numPointLights];
	spotLights = new spot_light_cb[numSpotLights];
	


	random_number_generator rng = { 14878213 };
	for (uint32 i = 0; i < numPointLights; ++i)
	{
		pointLights[i] =
		{
			{
				rng.randomFloatBetween(-100.f, 100.f),
				10.f,
				rng.randomFloatBetween(-100.f, 100.f),
			},
			25,
			randomRGB(rng),
		};

		lightVelocities[i] =
		{
			rng.randomFloatBetween(-1.f, 1.f),
			0.f,
			rng.randomFloatBetween(-1.f, 1.f),
		};
	}

	for (uint32 i = 0; i < numSpotLights; ++i)
	{
		spotLights[i] =
		{
			{
				rng.randomFloatBetween(-100.f, 100.f),
				10.f,
				rng.randomFloatBetween(-100.f, 100.f),
			},
			0.f,
			{
				0.f, -1.f, 0.f
			},
			0.f,
			randomRGB(rng) * 250.f,
			25
		};
	}

	sun.direction = normalize(vec3(-0.6f, -1.f, -0.3f));
	sun.color = vec3(1.f, 0.93f, 0.76f);
	sun.intensity = 50.f;

	sun.numShadowCascades = 3;
	sun.cascadeDistances.data[0] = 9.f;
	sun.cascadeDistances.data[1] = 39.f;
	sun.cascadeDistances.data[2] = 74.f;
	sun.cascadeDistances.data[3] = 10000.f;

	sun.bias = vec4(0.000163f, 0.000389f, 0.000477f, 0.0035f);
	sun.blendArea = 0.07f;
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
		for (uint32 i = 0; i < sun.numShadowCascades; ++i)
		{
			ImGui::PushID(i);
			if (ImGui::TreeNode("Cascade"))
			{
				result |= ImGui::SliderFloat("Distance", &sun.cascadeDistances.data[i], 0.f, 300.f);
				result |= ImGui::SliderFloat("Bias", &sun.bias.data[i], 0.f, 0.005f, "%.6f");
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		result |= ImGui::SliderFloat("Blend area", &sun.blendArea, 0.f, 0.1f, "%.6f");

		ImGui::TreePop();
	}
	return result;
}

void application::update(const user_input& input, float dt)
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

	if (selectedEntity && selectedEntity.hasComponent<trs>())
	{
		trs& transform = selectedEntity.getComponent<trs>();

		static transformation_type type = transformation_type_translation;
		static transformation_space space = transformation_global;
		inputCaptured = manipulateTransformation(transform, type, space, camera, input, !inputCaptured, renderer);
	}

	if (!inputCaptured && input.mouse.left.clickEvent)
	{
		if (renderer->hoveredObjectID != 0xFFFF)
		{
			selectedEntity = { renderer->hoveredObjectID, scene };
		}
		else
		{
			selectedEntity = {};
		}
		inputCaptured = true;
	}


	ImGui::Begin("Settings");
	ImGui::Text("%.3f ms, %u FPS", dt * 1000.f, (uint32)(1.f / dt));

	dx_memory_usage memoryUsage = dxContext.getMemoryUsage();

	ImGui::Text("Video memory available: %uMB", memoryUsage.available);
	ImGui::Text("Video memory used: %uMB", memoryUsage.currentlyUsed);

	ImGui::Dropdown("Aspect ratio", aspectRatioNames, aspect_ratio_mode_count, (uint32&)renderer->settings.aspectRatioMode);
	ImGui::Checkbox("Show light volumes", &renderer->settings.showLightVolumes);

	//ImGui::Image(renderer->screenVelocitiesTexture, 512, 512); // TODO: When you remove this, make renderer attributes private again.

	plotAndEditTonemapping(renderer->settings.tonemap);
	editSunShadowParameters(sun);

	ImGui::SliderFloat("Environment intensity", &renderer->settings.environmentIntensity, 0.f, 2.f);
	ImGui::SliderFloat("Sky intensity", &renderer->settings.skyIntensity, 0.f, 2.f);
	ImGui::SliderInt("Raytracing bounces", (int*)&renderer->settings.numRaytracingBounces, 1, MAX_PBR_RAYTRACING_RECURSION_DEPTH - 1);
	ImGui::SliderInt("Raytracing downsampling", (int*)&renderer->settings.raytracingDownsampleFactor, 1, 4);
	ImGui::SliderInt("Raytracing blur iteations", (int*)&renderer->settings.blurRaytracingResultIterations, 0, 4);

	ImGui::End();

	// Update light positions.
	const vec3 vortexCenter(0.f, 0.f, 0.f);
	const float vortexSpeed = 0.75f;
	const float vortexSize = 60.f;
	for (uint32 i = 0; i < numPointLights; ++i)
	{
		lightVelocities[i] *= (1.f - 0.005f);
		pointLights[i].position += lightVelocities[i] * dt;

		vec3 d = pointLights[i].position - vortexCenter;
		vec3 v = vec3(-d.z, 0.f, d.x) * vortexSpeed;
		float factor = 1.f / (1.f + (d.x * d.x + d.z * d.z) / vortexSize);

		lightVelocities[i] += (v - lightVelocities[i]) * factor;
	}

	sun.updateMatrices(camera);

	renderer->setCamera(camera);
	renderer->setSun(sun);
	renderer->setEnvironment(environment);
	renderer->setPointLights(pointLights, numPointLights);
	renderer->setSpotLights(spotLights, numSpotLights);



	scene.group<animation_component>(entt::get<raster_component>)
		.each([dt](auto& anim, auto& raster)
	{
		anim.time += dt;
		const dx_mesh& mesh = raster.mesh->mesh;
		const animation_skeleton& skeleton = raster.mesh->skeleton;
		submesh_info info = raster.mesh->submeshes[0].info;

		trs localTransforms[128];
		auto [vb, sm, skinningMatrices] = skinObject(mesh.vertexBuffer, info, (uint32)skeleton.joints.size());
		skeleton.sampleAnimation(skeleton.clips[0].name, anim.time, localTransforms);
		skeleton.getSkinningMatricesFromLocalTransforms(localTransforms, skinningMatrices);

		anim.prevFrameVB = anim.vb;
		anim.prevFrameSM = anim.sm;

		anim.vb = vb;
		anim.sm = sm;
	});


	scene.group<raster_component>(entt::get<trs>)
		.each([this](entt::entity entityHandle, auto& raster, auto& transform)
	{
		const dx_mesh& mesh = raster.mesh->mesh;
		mat4 m = trsToMat4(transform);

		scene_entity entity = { entityHandle, scene };
		bool outline = selectedEntity == entity;

		if (entity.hasComponent<animation_component>())
		{
			// Currently only one submesh in animated meshes.

			auto& anim = entity.getComponent<animation_component>();

			renderer->opaqueRenderPass.renderAnimatedObject(anim.vb, anim.prevFrameVB, mesh.indexBuffer, anim.sm, anim.prevFrameSM, raster.mesh->submeshes[0].material, m, m, 
				(uint32)entityHandle, outline);
			renderer->sunShadowRenderPass.renderObject(0, anim.vb, mesh.indexBuffer, anim.sm, m);
		}
		else
		{
			for (auto& sm : raster.mesh->submeshes)
			{
				submesh_info submesh = sm.info;
				const ref<pbr_material>& material = sm.material;

				renderer->opaqueRenderPass.renderStaticObject(mesh.vertexBuffer, mesh.indexBuffer, submesh, material, m, (uint32)entityHandle, outline);
				renderer->sunShadowRenderPass.renderObject(0, mesh.vertexBuffer, mesh.indexBuffer, submesh, m);
			}
		}
	});

	//renderer->giRenderPass.specularReflection(reflectionsRaytracingPipeline, raytracingTLAS);
}

void application::setEnvironment(const char* filename)
{
	environment = createEnvironment(filename); // Currently synchronous (on render queue).
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
				<< YAML::Key << "Blend Area" << YAML::Value << sun.blendArea
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
	sun.color = sunNode["Color"].as<vec3>();
	sun.intensity = sunNode["Intensity"].as<float>();
	sun.direction = sunNode["Direction"].as<vec3>();
	sun.numShadowCascades = sunNode["Cascades"].as<uint32>();
	sun.cascadeDistances = sunNode["Distances"].as<vec4>();
	sun.bias = sunNode["Bias"].as<vec4>();
	sun.blendArea = sunNode["Blend Area"].as<float>();

	auto lightingNode = data["Lighting"];
	renderer->settings.environmentIntensity = lightingNode["Environment Intensity"].as<float>();

	return true;
}
