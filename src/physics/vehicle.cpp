#include "pch.h"
#include "vehicle.h"

#ifndef PHYSICS_ONLY
#include "rendering/pbr.h"
#include "geometry/mesh.h"
#include "geometry/geometry.h"
#include "core/imgui.h"
#endif

struct gear_description
{
	float height; 
	float innerRadius; 
	uint32 numTeeth; 
	float toothLength; 
	float toothWidth;

	float friction; 
	float density;
};

struct wheel_description
{
	float height;
	float radius;

	float friction;
	float density;
};

enum attachment_type
{
	attachment_type_gear,
	attachment_type_wheel,
};

struct axis_attachment
{
	attachment_type type;
	float rodLength;

	union
	{
		gear_description gear;
		wheel_description wheel;
	};

	axis_attachment(float rodLength, gear_description gear)
		: type(attachment_type_gear), rodLength(rodLength), gear(gear) {}
	axis_attachment(float rodLength, wheel_description wheel)
		: type(attachment_type_wheel), rodLength(rodLength), wheel(wheel) {}
};

static void attach(ref<composite_mesh> mesh, cpu_mesh& primitiveMesh, ref<pbr_material> material, scene_entity axis, axis_attachment& attachment, float sign)
{
	float rodLength = attachment.rodLength * sign;

	switch (attachment.type)
	{
		case attachment_type_gear: 
		{
			gear_description desc = attachment.gear;

			mesh->submeshes.push_back({ primitiveMesh.pushCylinder(21, desc.innerRadius, desc.height, vec3(0.f, rodLength, 0.f)), {}, trs::identity, material });

			for (uint32 i = 0; i < desc.numTeeth; ++i)
			{
				float angle = i * M_TAU / desc.numTeeth;
				quat localRotation(vec3(0.f, 1.f, 0.f), angle);
				vec3 center = localRotation * vec3(desc.innerRadius + desc.toothLength * 0.5f, 0.f, 0.f);
				vec3 radius(desc.toothLength * 0.5f, desc.height * 0.5f, desc.toothWidth * 0.5f);

				mesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, desc.toothLength, desc.toothWidth * 0.5f, center + vec3(0.f, rodLength, 0.f), localRotation * vec3(1.f, 0.f, 0.f)), {}, trs::identity, material });

				bounding_capsule capsule;
				capsule.positionA = center + vec3(0.f, rodLength, 0.f) - localRotation * vec3(desc.toothLength * 0.5f, 0.f, 0.f);
				capsule.positionB = center + vec3(0.f, rodLength, 0.f) + localRotation * vec3(desc.toothLength * 0.5f, 0.f, 0.f);

				capsule.radius = desc.toothWidth * 0.5f;

				axis.addComponent<collider_component>(collider_component::asCapsule(capsule, 0.2f, desc.friction, desc.density));
			}
		} break;

		case attachment_type_wheel:
		{
			wheel_description desc = attachment.wheel;

			//mesh->submeshes.push_back({ primitiveMesh.pushCylinder(21, desc.radius, desc.height, vec3(0.f, rodLength, 0.f)), {}, trs::identity, material });
			mesh->submeshes.push_back({ primitiveMesh.pushSphere(21, 21, desc.radius, vec3(0.f, rodLength, 0.f)), {}, trs::identity, material });

			bounding_sphere sphere;
			sphere.center = vec3(0.f, rodLength, 0.f);
			sphere.radius = desc.radius;

			axis.addComponent<collider_component>(collider_component::asSphere(sphere, 0.2f, desc.friction, desc.density));
		} break;
	}
}

static scene_entity createAxis(game_scene& scene, cpu_mesh& primitiveMesh, ref<pbr_material> material,
	vec3 position, quat rotation, gear_description desc, axis_attachment* firstAttachment = 0, axis_attachment* secondAttachment = 0)
{
	scene_entity axis = scene.createEntity("Axis")
		.addComponent<transform_component>(position, rotation);


	auto mesh = make_ref<composite_mesh>();

	axis_attachment centerGearAttachment(0.f, desc);
	attach(mesh, primitiveMesh, material, axis, centerGearAttachment, 1.f);

	if (firstAttachment)
	{
		attach(mesh, primitiveMesh, material, axis, *firstAttachment, 1.f);
		mesh->submeshes.push_back({ primitiveMesh.pushCube(vec3(desc.innerRadius * 0.1f, firstAttachment->rodLength * 0.5f, desc.innerRadius * 0.1f), false, vec3(0.f, firstAttachment->rodLength * 0.5f, 0.f)), {}, trs::identity, material });
	}
	if (secondAttachment)
	{
		attach(mesh, primitiveMesh, material, axis, *secondAttachment, -1.f);
		mesh->submeshes.push_back({ primitiveMesh.pushCube(vec3(desc.innerRadius * 0.1f, secondAttachment->rodLength * 0.5f, desc.innerRadius * 0.1f), false, vec3(0.f, -secondAttachment->rodLength * 0.5f, 0.f)), {}, trs::identity, material });
	}


	axis.addComponent<raster_component>(mesh);
	axis.addComponent<rigid_body_component>(false);

	return axis;
}

void vehicle::initialize(game_scene& scene)
{
	float density = 2000.f;


	cpu_mesh primitiveMesh(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents);

	auto material = createPBRMaterial(
		{}, {}, {}, {}, vec4(0.f), vec4(0.1f, 0.1f, 0.1f, 1.f), 1.f, 0.f);


	motor = scene.createEntity("Motor")
		.addComponent<transform_component>(vec3(0.f, 1.1f, 0.f), quat::identity)
		.addComponent<collider_component>(collider_component::asAABB(bounding_box::fromCenterRadius(vec3(0.f), vec3(1.f, 0.1f, 0.6f)), 0.2f, 0.f, density))
		.addComponent<rigid_body_component>(false);

	auto motorMesh = make_ref<composite_mesh>();
	motorMesh->submeshes.push_back({ primitiveMesh.pushCube(vec3(1.f, 0.1f, 0.6f)), {}, trs::identity, material });

	motor.addComponent<raster_component>(motorMesh);


	gear_description gearDesc;
	gearDesc.height = 0.1f;
	gearDesc.innerRadius = 0.2f;
	gearDesc.numTeeth = 8;
	gearDesc.toothLength = 0.07f;
	gearDesc.toothWidth = 0.1f;
	gearDesc.friction = 0.f;
	gearDesc.density = density;

	wheel_description wheelDesc;
	wheelDesc.height = 0.3f;
	wheelDesc.radius = 0.7f;
	wheelDesc.friction = 1.f;
	wheelDesc.density = 100.f;


	// Motor gear.
	motorGear = createAxis(scene, primitiveMesh, material, vec3(0.f, 1.35f, 0.f), quat::identity, gearDesc);
	auto handle = addHingeConstraintFromGlobalPoints(motor, motorGear, vec3(0.f, 1.35f, 0.f), vec3(0.f, 1.f, 0.f));

	auto& constraint = getConstraint(scene, handle);
	//constraint.maxMotorTorque = 200.f;
	constraint.motorVelocity = 1.f;

	// Drive axis.
	float driveAxisLength = 4.5f;
	float driveAxisOffset = 0.26f;
	axis_attachment driveAxisAttachment(driveAxisLength * 0.5f, gearDesc);
	driveAxis = createAxis(scene, primitiveMesh, material, vec3(driveAxisOffset, 1.35f + driveAxisOffset, 0.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(90.f)),
		gearDesc, &driveAxisAttachment, &driveAxisAttachment);
	addHingeConstraintFromGlobalPoints(motor, driveAxis, vec3(driveAxisOffset, 1.35f + driveAxisOffset, 0.f), vec3(1.f, 0.f, 0.f));
	// Stabilization. Not really necessary.
	addHingeConstraintFromGlobalPoints(motor, driveAxis, vec3(driveAxisOffset + driveAxisLength * 0.5f, 1.35f + driveAxisOffset, 0.f), vec3(1.f, 0.f, 0.f));
	addHingeConstraintFromGlobalPoints(motor, driveAxis, vec3(driveAxisOffset - driveAxisLength * 0.5f, 1.35f + driveAxisOffset, 0.f), vec3(1.f, 0.f, 0.f));

	// Front axis.
	float frontAxisOffsetX = -driveAxisLength * 0.5f + driveAxisOffset * 2.f;
	float frontAxisOffsetZ = driveAxisOffset;
	float axisLength = 1.5f;
	axis_attachment firstFrontAxisAttachment(axisLength - frontAxisOffsetZ, wheelDesc);
	axis_attachment secondFrontAxisAttachment(axisLength + frontAxisOffsetZ, wheelDesc);
	frontAxis = createAxis(scene, primitiveMesh, material, vec3(frontAxisOffsetX, 1.35f + driveAxisOffset, frontAxisOffsetZ), quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f)), gearDesc, &firstFrontAxisAttachment, &secondFrontAxisAttachment);
	addHingeConstraintFromGlobalPoints(motor, frontAxis, vec3(frontAxisOffsetX, 1.35f + driveAxisOffset, frontAxisOffsetZ), vec3(0.f, 0.f, 1.f));

	// Rear axis.
	float rearAxisOffsetX = driveAxisLength * 0.5f;
	float rearAxisOffsetZ = -driveAxisOffset;
	axis_attachment firstRearAxisAttachment(axisLength - rearAxisOffsetZ, wheelDesc);
	axis_attachment secondRearAxisAttachment(axisLength + rearAxisOffsetZ, wheelDesc);
	rearAxis = createAxis(scene, primitiveMesh, material, vec3(rearAxisOffsetX, 1.35f + driveAxisOffset, rearAxisOffsetZ), quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f)), gearDesc, &firstRearAxisAttachment, &secondRearAxisAttachment);
	addHingeConstraintFromGlobalPoints(motor, rearAxis, vec3(rearAxisOffsetX, 1.35f + driveAxisOffset, rearAxisOffsetZ), vec3(0.f, 0.f, 1.f));

	// Steering axis.
	steeringAxis = createAxis(scene, primitiveMesh, material, vec3(frontAxisOffsetX, 1.35f + driveAxisOffset + 2.f, frontAxisOffsetZ), quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f)), gearDesc, &firstFrontAxisAttachment, &secondFrontAxisAttachment);
	addSliderConstraintFromGlobalPoints(motor, steeringAxis, vec3(frontAxisOffsetX, 1.35f + driveAxisOffset + 2.f, frontAxisOffsetZ), vec3(0.f, 0.f, 1.f));

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
