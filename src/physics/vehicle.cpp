#include "pch.h"
#include "vehicle.h"

#ifndef PHYSICS_ONLY
#include "rendering/pbr.h"
#include "geometry/mesh.h"
#include "geometry/geometry.h"
#include "core/imgui.h"
#endif

static scene_entity createGear(game_scene& scene, cpu_mesh& primitiveMesh, ref<pbr_material> material,
	vec3 position, float height, float innerRadius, uint32 numTeeth, float toothLength, float toothWidth,
	float friction, float density)
{
	scene_entity gear = scene.createEntity("Gear")
		.addComponent<transform_component>(position, quat::identity);


	auto gearMesh = make_ref<composite_mesh>();
	gearMesh->submeshes.push_back({ primitiveMesh.pushCylinder(21, innerRadius, height), {}, trs::identity, material });
	for (uint32 i = 0; i < numTeeth; ++i)
	{
		float angle = i * M_TAU / numTeeth;
		quat rotation(vec3(0.f, 1.f, 0.f), angle);
		vec3 center = rotation * vec3(innerRadius + toothLength * 0.5f, 0.f, 0.f);
		vec3 radius(toothLength * 0.5f, height * 0.5f, toothWidth * 0.5f);

		//gearMesh->submeshes.push_back({ primitiveMesh.pushCube(radius, false, center, rotation), {}, trs::identity, material });
		gearMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, toothLength, toothWidth * 0.5f, center, rotation * vec3(1.f, 0.f, 0.f)), {}, trs::identity, material });



		//bounding_oriented_box obb;
		//obb.center = center;
		//obb.radius = radius;
		//obb.rotation = rotation;
		//gear.addComponent<collider_component>(collider_component::asOBB(obb, 0.2f, friction, density));

		bounding_capsule capsule;
		capsule.positionA = center - rotation * vec3(toothLength * 0.5f, 0.f, 0.f);
		capsule.positionB = center + rotation * vec3(toothLength * 0.5f, 0.f, 0.f);
		capsule.radius = toothWidth * 0.5f;

		gear.addComponent<collider_component>(collider_component::asCapsule(capsule, 0.2f, friction, density));
	}

	gear.addComponent<raster_component>(gearMesh);
	gear.addComponent<rigid_body_component>(false);

	return gear;
}

void vehicle::initialize(game_scene& scene)
{
	float friction = 0.f;
	float density = 2000.f;

	chassis = scene.createEntity("Chassis")
		.addComponent<transform_component>(vec3(0.f, 1.f, 0.f), quat::identity)
		.addComponent<collider_component>(collider_component::asAABB(bounding_box::fromCenterRadius(vec3(0.f), vec3(1.f, 0.2f, 0.6f)), 0.2f, friction, density))
		.addComponent<rigid_body_component>(true);


	cpu_mesh primitiveMesh(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents);

	auto material = createPBRMaterial(
		{}, {}, {}, {}, vec4(0.f), vec4(0.1f, 0.1f, 0.1f, 1.f), 1.f, 0.f);

	auto chassisMesh = make_ref<composite_mesh>();
	chassisMesh->submeshes.push_back({ primitiveMesh.pushCube(vec3(1.f, 0.2f, 0.6f)), {}, trs::identity, material });

	chassis.addComponent<raster_component>(chassisMesh);


	for (uint32 i = 0; i < arraysize(gears); ++i)
	{
		gears[i] = createGear(scene, primitiveMesh, material, vec3(0.195f + i * 0.505f, 1.45f, 0.f), 0.1f, 0.2f, 8, 0.05f, 0.1f, friction, density);
		auto handle = addWheelConstraintFromGlobalPoints(chassis, gears[i], vec3(0.195f + i * 0.505f, 1.3f, 0.f), vec3(0.f, 1.f, 0.f));

		if (i == 0)
		{
			wheel_constraint& wheelCon = getConstraint(scene, handle);
			wheelCon.maxMotorTorque = 200.f;
			wheelCon.motorVelocity = 1.f;
		}
	}


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
