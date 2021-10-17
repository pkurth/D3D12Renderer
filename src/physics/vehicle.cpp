#include "pch.h"
#include "vehicle.h"

#ifndef PHYSICS_ONLY
#include "rendering/pbr.h"
#include "geometry/mesh.h"
#include "geometry/geometry.h"
#include "core/imgui.h"
#endif

static scene_entity createGear(game_scene& scene, cpu_mesh& primitiveMesh, ref<pbr_material> material,
	vec3 position, quat rotation, float height, float innerRadius, uint32 numTeeth, float toothLength, float toothWidth, float rodLength,
	float friction, float density)
{
	scene_entity gear = scene.createEntity("Gear")
		.addComponent<transform_component>(position, rotation);


	auto gearMesh = make_ref<composite_mesh>();
	gearMesh->submeshes.push_back({ primitiveMesh.pushCylinder(21, innerRadius, height), {}, trs::identity, material });
	for (uint32 i = 0; i < numTeeth; ++i)
	{
		float angle = i * M_TAU / numTeeth;
		quat localRotation(vec3(0.f, 1.f, 0.f), angle);
		vec3 center = localRotation * vec3(innerRadius + toothLength * 0.5f, 0.f, 0.f);
		vec3 radius(toothLength * 0.5f, height * 0.5f, toothWidth * 0.5f);

		gearMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, toothLength, toothWidth * 0.5f, center, localRotation * vec3(1.f, 0.f, 0.f)), {}, trs::identity, material });

		bounding_capsule capsule;
		capsule.positionA = center - localRotation * vec3(toothLength * 0.5f, 0.f, 0.f);
		capsule.positionB = center + localRotation * vec3(toothLength * 0.5f, 0.f, 0.f);
		capsule.radius = toothWidth * 0.5f;

		gear.addComponent<collider_component>(collider_component::asCapsule(capsule, 0.2f, friction, density));
	}

	gearMesh->submeshes.push_back({ primitiveMesh.pushCube(vec3(innerRadius * 0.1f, rodLength * 0.5f, innerRadius * 0.1f)), {}, trs::identity, material });

	gear.addComponent<raster_component>(gearMesh);
	gear.addComponent<rigid_body_component>(false);

	return gear;
}

void vehicle::initialize(game_scene& scene)
{
	float friction = 0.f;
	float density = 2000.f;

	motor = scene.createEntity("Motor")
		.addComponent<transform_component>(vec3(0.f, 1.f, 0.f), quat::identity)
		.addComponent<collider_component>(collider_component::asAABB(bounding_box::fromCenterRadius(vec3(0.f), vec3(1.f, 0.2f, 0.6f)), 0.2f, friction, density))
		.addComponent<rigid_body_component>(true);


	cpu_mesh primitiveMesh(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents);

	auto material = createPBRMaterial(
		{}, {}, {}, {}, vec4(0.f), vec4(0.1f, 0.1f, 0.1f, 1.f), 1.f, 0.f);

	auto motorMesh = make_ref<composite_mesh>();
	motorMesh->submeshes.push_back({ primitiveMesh.pushCube(vec3(1.f, 0.2f, 0.6f)), {}, trs::identity, material });

	motor.addComponent<raster_component>(motorMesh);

	gears[0] = createGear(scene, primitiveMesh, material, vec3(0.f, 1.35f, 0.f), quat::identity, 
		0.1f, 0.2f, 8, 0.05f, 0.1f, 0.25f, friction, density);
	auto handle = addWheelConstraintFromGlobalPoints(motor, gears[0], vec3(0.f, 1.35f, 0.f), vec3(0.f, 1.f, 0.f));

	wheel_constraint& wheelCon = getConstraint(scene, handle);
	wheelCon.maxMotorTorque = 200.f;
	wheelCon.motorVelocity = 1.f;

	float offset = 0.26f;
	gears[1] = createGear(scene, primitiveMesh, material, vec3(offset, 1.35f + offset, 0.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(90.f)),
		0.1f, 0.2f, 8, 0.05f, 0.1f, 4.5f, friction, density);
	addWheelConstraintFromGlobalPoints(motor, gears[1], vec3(offset, 1.35f + offset, 0.f), vec3(1.f, 0.f, 0.f));


	auto mesh = primitiveMesh.createDXMesh();
	for (uint32 i = 0; i < arraysize(parts); ++i)
	{
		parts[i].getComponent<raster_component>().mesh->mesh = mesh;
	}
}

vehicle vehicle::create(game_scene& scene)
{
	vehicle v;
	v.initialize(scene);
	return v;
}
