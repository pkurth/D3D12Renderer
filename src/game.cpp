#include "pch.h"
#include "game.h"
#include "geometry.h"
#include "texture.h"
#include "random.h"
#include "color.h"

void game_scene::initialize()
{
	camera.position = vec3(0.f, 30.f, 40.f);
	camera.rotation = quat(vec3(1.f, 0.f, 0.f), deg2rad(-20.f));
	camera.verticalFOV = deg2rad(70.f);
	camera.nearPlane = 0.1f;


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

	environment = dx_renderer::createEnvironment("assets/textures/hdri/leadenhall_market_4k.hdr");

	pointLights = new point_light_cb[MAX_NUM_POINT_LIGHTS_PER_FRAME];
	lightVelocities = new vec3[MAX_NUM_POINT_LIGHTS_PER_FRAME];


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

	spotLights = new spot_light_cb[MAX_NUM_SPOT_LIGHTS_PER_FRAME];
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
}

void game_scene::update(const user_input& input, float dt)
{
	const float CAMERA_MOVEMENT_SPEED = 4.f;
	const float CAMERA_SENSITIVITY = 4.f;

	if (input.mouse.right.down)
	{
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

	camera.recalculateMatrices(dx_renderer::renderWidth, dx_renderer::renderHeight);



	const vec3 vortexCenter(0.f, 0.f, 0.f);
	const float vortexSpeed = 1.f;
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

	dx_renderer::setCamera(camera);
	dx_renderer::setSun(normalize(-vec3(1.f, 0.8f, 0.3f)), vec3(1.f, 0.9f, 0.8f) * 50.f);
	dx_renderer::setEnvironment(environment);
	dx_renderer::setPointLights(pointLights, numPointLights);
	dx_renderer::setSpotLights(spotLights, numSpotLights);



	for (const game_object& go : gameObjects)
	{
		const dx_mesh& mesh = meshes[go.meshIndex].mesh;
		submesh_info submesh = meshes[go.meshIndex].singleMeshes[0].submesh;
		const pbr_material& material = materials[go.materialIndex];

		dx_renderer::renderObject(&mesh, submesh, &material, go.transform);
	}


}
