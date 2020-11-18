#include "pch.h"
#include "application.h"
#include "geometry.h"
#include "texture.h"
#include "random.h"
#include "color.h"
#include "imgui.h"
#include "dx_context.h"


void application::initialize(dx_renderer* renderer)
{
	this->renderer = renderer;

	camera.position = vec3(0.f, 30.f, 40.f);
	camera.rotation = quat::identity;
	camera.verticalFOV = deg2rad(70.f);
	camera.nearPlane = 0.1f;
	camera.farPlane = 1000.f;



	meshes.push_back(createCompositeMeshFromFile("assets/meshes/cerberus.fbx", 
		mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents)
	);

	composite_mesh mesh = {};
	cpu_mesh m(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents);
	mesh.singleMeshes.push_back({ m.pushQuad(100.f) });
	mesh.mesh = m.createDXMesh();
	meshes.push_back(mesh);

	textures.push_back(loadTextureFromFile("assets/textures/cerberus_a.tga"));
	textures.push_back(loadTextureFromFile("assets/textures/cerberus_n.tga", texture_load_flags_default | texture_load_flags_noncolor));
	textures.push_back(loadTextureFromFile("assets/textures/cerberus_r.tga", texture_load_flags_default | texture_load_flags_noncolor));
	textures.push_back(loadTextureFromFile("assets/textures/cerberus_m.tga", texture_load_flags_default | texture_load_flags_noncolor));

	materials.push_back(
		{
			&textures[0],
			&textures[1],
			&textures[2],
			&textures[3],
			vec4(1.f, 1.f, 1.f, 1.f),
			0.f, 
			0.f,
		}
	);

	gameObjects.push_back(
		{
			{
				vec3(0.f, 10.f, 0.f),
				quat(vec3(1.f, 0.f, 0.f), deg2rad(-90.f)),
				0.4f,
			},
			0,
			0,
		}
	);

	gameObjects.push_back(
		{
			{
				vec3(0.f, 0.f, 0.f),
				quat(vec3(1.f, 0.f, 0.f), deg2rad(-90.f)),
				1.f,
			},
			1,
			0,
		}
	);

	setEnvironment("assets/textures/hdri/leadenhall_market_4k.hdr");

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
				2.f,
				rng.randomFloatBetween(-100.f, 100.f),
			},
			25,
			randomRGB(rng) * 250.f,
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
	sun.radiance = vec3(1.f, 0.93f, 0.76f) * 50.f;

	sun.cascadeDistances.data[0] = 9.f;
	sun.cascadeDistances.data[1] = 39.f;
	sun.cascadeDistances.data[2] = 74.f;
	sun.cascadeDistances.data[3] = 10000.f;

	sun.numShadowCascades = 3;

	sun.bias = vec4(0.001f, 0.0015f, 0.0015f, 0.0035f);
	sun.blendArea = 0.07f;
}

void application::updateCamera(const user_input& input, float dt)
{
	const float CAMERA_MOVEMENT_SPEED = 8.f;
	const float CAMERA_SENSITIVITY = 4.f;
	const float CAMERA_ORBIT_RADIUS = 50.f;

	if (input.mouse.right.down)
	{
		// Fly camera.

		vec3 cameraInputDir = vec3(
			(input.keyboard['D'].down ? 1.f : 0.f) + (input.keyboard['A'].down ? -1.f : 0.f),
			(input.keyboard['E'].down ? 1.f : 0.f) + (input.keyboard['Q'].down ? -1.f : 0.f),
			(input.keyboard['W'].down ? -1.f : 0.f) + (input.keyboard['S'].down ? 1.f : 0.f)
		) * (input.keyboard[key_shift].down ? 3.f : 1.f) * (input.keyboard[key_ctrl].down ? 0.1f : 1.f) * CAMERA_MOVEMENT_SPEED;

		vec2 turnAngle(0.f, 0.f);
		turnAngle = vec2(-input.mouse.reldx, -input.mouse.reldy) * CAMERA_SENSITIVITY;

		quat& cameraRotation = camera.rotation;
		cameraRotation = quat(vec3(0.f, 1.f, 0.f), turnAngle.x) * cameraRotation;
		cameraRotation = cameraRotation * quat(vec3(1.f, 0.f, 0.f), turnAngle.y);

		camera.position += cameraRotation * cameraInputDir * dt;
	}
	else if (input.keyboard[key_alt].down && input.mouse.left.down)
	{
		// Orbit camera.

		vec2 turnAngle(0.f, 0.f);
		turnAngle = vec2(-input.mouse.reldx, -input.mouse.reldy) * CAMERA_SENSITIVITY;

		quat& cameraRotation = camera.rotation;

		vec3 center = camera.position + cameraRotation * vec3(0.f, 0.f, -CAMERA_ORBIT_RADIUS);

		cameraRotation = quat(vec3(0.f, 1.f, 0.f), turnAngle.x) * cameraRotation;
		cameraRotation = cameraRotation * quat(vec3(1.f, 0.f, 0.f), turnAngle.y);

		camera.position = center - cameraRotation * vec3(0.f, 0.f, -CAMERA_ORBIT_RADIUS);
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

void application::update(const user_input& input, float dt)
{
	updateCamera(input, dt);
	camera.recalculateMatrices(renderer->renderWidth, renderer->renderHeight);

	ImGui::Begin("Settings");
	ImGui::Text("%f ms, %u FPS", dt, (uint32)(1.f / dt));

	dx_memory_usage memoryUsage = dxContext.getMemoryUsage();

	ImGui::Text("Video memory available: %uMB", memoryUsage.available);
	ImGui::Text("Video memory used: %uMB", memoryUsage.currentlyUsed);

	ImGui::Dropdown("Aspect ratio", aspectRatioNames, aspect_ratio_mode_count, (uint32&)renderer->settings.aspectRatioMode);
	ImGui::Checkbox("Show light volumes", &renderer->settings.showLightVolumes);
	plotAndEditTonemapping(renderer->settings.tonemap);

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


	geometry_render_pass* geometryPass = renderer->beginGeometryPass();
	sun_shadow_render_pass* shadowPass = renderer->beginSunShadowPass();
	volumetrics_render_pass* volumetricsPass = renderer->beginVolumetricsPass();


	for (const scene_object& go : gameObjects)
	{
		const dx_mesh& mesh = meshes[go.meshIndex].mesh;
		submesh_info submesh = meshes[go.meshIndex].singleMeshes[0].submesh;
		const pbr_material& material = materials[go.materialIndex];

		mat4 m = trsToMat4(go.transform);

		geometryPass->renderObject(&mesh, submesh, &material, m);
		shadowPass->renderObject(0, &mesh, submesh, m);
	}

	volumetricsPass->addVolume(bounding_box::fromCenterRadius(vec3(0.f, 0.f, 0.f), vec3(100.f, 100.f, 100.f)));

}

void application::setEnvironment(const char* filename)
{
	environment = dx_renderer::createEnvironment(filename); // Currently synchronous (on render queue).
}




// Serialization stuff.

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;


void application::serializeToFile(const char* filename)
{
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "Scene" << YAML::Value << "PLACEHOLDER";

	out << YAML::EndMap;

	fs::create_directories(fs::path(filename).parent_path());

	std::ofstream fout(filename);
	fout << out.c_str();
}

void application::unserializeFromFile(const char* filename)
{
}
