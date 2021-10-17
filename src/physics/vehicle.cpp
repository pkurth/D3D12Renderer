#include "pch.h"
#include "vehicle.h"

#ifndef PHYSICS_ONLY
#include "rendering/pbr.h"
#include "geometry/mesh.h"
#include "geometry/geometry.h"
#include "core/imgui.h"
#endif

static scene_entity createGear(game_scene& scene, cpu_mesh& primitiveMesh, ref<pbr_material> material,
	vec3 position, quat rotation, float height, float innerRadius, uint32 numTeeth, float toothLength, float toothWidth, 
	float firstRodLength, float secondRodLength,
	float friction, float density, bool placeAnotherAtFirstEnd = false, bool placeAnotherAtSecondEnd = false)
{
	scene_entity gear = scene.createEntity("Gear")
		.addComponent<transform_component>(position, rotation);


	auto gearMesh = make_ref<composite_mesh>();
	gearMesh->submeshes.push_back({ primitiveMesh.pushCylinder(21, innerRadius, height), {}, trs::identity, material });

	if (placeAnotherAtFirstEnd)
	{
		gearMesh->submeshes.push_back({ primitiveMesh.pushCylinder(21, innerRadius, height, vec3(0.f, firstRodLength, 0.f)), {}, trs::identity, material });
	}
	if (placeAnotherAtSecondEnd)
	{
		gearMesh->submeshes.push_back({ primitiveMesh.pushCylinder(21, innerRadius, height, vec3(0.f, -secondRodLength, 0.f)), {}, trs::identity, material });
	}

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

		if (placeAnotherAtFirstEnd)
		{
			gearMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, toothLength, toothWidth * 0.5f, center + vec3(0.f, firstRodLength, 0.f), localRotation * vec3(1.f, 0.f, 0.f)), {}, trs::identity, material });

			bounding_capsule c = capsule;
			c.positionA += vec3(0.f, firstRodLength, 0.f);
			c.positionB += vec3(0.f, firstRodLength, 0.f);

			gear.addComponent<collider_component>(collider_component::asCapsule(c, 0.2f, friction, density));
		}
		if (placeAnotherAtSecondEnd)
		{
			gearMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, toothLength, toothWidth * 0.5f, center + vec3(0.f, -secondRodLength, 0.f), localRotation * vec3(1.f, 0.f, 0.f)), {}, trs::identity, material });

			bounding_capsule c = capsule;
			c.positionA += vec3(0.f, -secondRodLength, 0.f);
			c.positionB += vec3(0.f, -secondRodLength, 0.f);

			gear.addComponent<collider_component>(collider_component::asCapsule(c, 0.2f, friction, density));
		}
	}

	gearMesh->submeshes.push_back({ primitiveMesh.pushCube(vec3(innerRadius * 0.1f, firstRodLength * 0.5f, innerRadius * 0.1f), false, vec3(0.f, firstRodLength * 0.5f, 0.f)), {}, trs::identity, material });
	gearMesh->submeshes.push_back({ primitiveMesh.pushCube(vec3(innerRadius * 0.1f, secondRodLength * 0.5f, innerRadius * 0.1f), false, vec3(0.f, -secondRodLength * 0.5f, 0.f)), {}, trs::identity, material });

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

	// Motor gear.
	gears[0] = createGear(scene, primitiveMesh, material, vec3(0.f, 1.35f, 0.f), quat::identity, 
		0.1f, 0.2f, 8, 0.05f, 0.1f, 0.25f, 0.25f, friction, density);
	auto handle = addWheelConstraintFromGlobalPoints(motor, gears[0], vec3(0.f, 1.35f, 0.f), vec3(0.f, 1.f, 0.f));

	wheel_constraint& wheelCon = getConstraint(scene, handle);
	wheelCon.maxMotorTorque = 200.f;
	wheelCon.motorVelocity = 1.f;

	// Drive axis.
	float driveAxisLength = 4.5f;
	float driveAxisOffset = 0.26f;
	gears[1] = createGear(scene, primitiveMesh, material, vec3(driveAxisOffset, 1.35f + driveAxisOffset, 0.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(90.f)),
		0.1f, 0.2f, 8, 0.05f, 0.1f, driveAxisLength * 0.5f, driveAxisLength * 0.5f, friction, density, true, true);
	addWheelConstraintFromGlobalPoints(motor, gears[1], vec3(driveAxisOffset, 1.35f + driveAxisOffset, 0.f), vec3(1.f, 0.f, 0.f));
	// Stabilization. Not really necessary.
	addWheelConstraintFromGlobalPoints(motor, gears[1], vec3(driveAxisOffset + driveAxisLength * 0.5f, 1.35f + driveAxisOffset, 0.f), vec3(1.f, 0.f, 0.f));
	addWheelConstraintFromGlobalPoints(motor, gears[1], vec3(driveAxisOffset - driveAxisLength * 0.5f, 1.35f + driveAxisOffset, 0.f), vec3(1.f, 0.f, 0.f));

	// Front axis.
	float frontAxisOffsetX = -driveAxisLength * 0.5f + driveAxisOffset * 2.f;
	float frontAxisOffsetZ = driveAxisOffset;
	float axisLength = 1.5f;
	gears[2] = createGear(scene, primitiveMesh, material, vec3(frontAxisOffsetX, 1.35f + driveAxisOffset, frontAxisOffsetZ), quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f)),
		0.1f, 0.2f, 8, 0.05f, 0.1f, axisLength - frontAxisOffsetZ, axisLength + frontAxisOffsetZ, friction, density, true, true);
	addWheelConstraintFromGlobalPoints(motor, gears[2], vec3(frontAxisOffsetX, 1.35f + driveAxisOffset, frontAxisOffsetZ), vec3(0.f, 0.f, 1.f));

	// Rear axis.
	float rearAxisOffsetX = driveAxisLength * 0.5f;
	float rearAxisOffsetZ = -driveAxisOffset;
	gears[3] = createGear(scene, primitiveMesh, material, vec3(rearAxisOffsetX, 1.35f + driveAxisOffset, rearAxisOffsetZ), quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f)),
		0.1f, 0.2f, 8, 0.05f, 0.1f, axisLength - rearAxisOffsetZ, axisLength + rearAxisOffsetZ, friction, density, true, true);
	addWheelConstraintFromGlobalPoints(motor, gears[3], vec3(rearAxisOffsetX, 1.35f + driveAxisOffset, rearAxisOffsetZ), vec3(0.f, 0.f, 1.f));


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
