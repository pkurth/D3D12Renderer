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
	attachment_type_none,
	attachment_type_gear,
	attachment_type_wheel,
};

struct axis_attachment
{
	attachment_type type;
	float rodLength;
	float rodThickness;

	union
	{
		gear_description gear;
		wheel_description wheel;
	};

	axis_attachment(float rodLength, float rodThickness)
		: type(attachment_type_none), rodLength(rodLength), rodThickness(rodThickness) {}
	axis_attachment(float rodLength, float rodThickness, gear_description gear)
		: type(attachment_type_gear), rodLength(rodLength), rodThickness(rodThickness), gear(gear) {}
	axis_attachment(float rodLength, float rodThickness, wheel_description wheel)
		: type(attachment_type_wheel), rodLength(rodLength), rodThickness(rodThickness), wheel(wheel) {}
};

static void attach(ref<composite_mesh> mesh, cpu_mesh& primitiveMesh, ref<pbr_material> material, scene_entity axis, axis_attachment& attachment, float sign)
{
	float rodOffset = attachment.rodLength * sign;

	switch (attachment.type)
	{
		case attachment_type_gear: 
		{
			gear_description desc = attachment.gear;

			mesh->submeshes.push_back({ primitiveMesh.pushCylinder(21, desc.innerRadius, desc.height, vec3(0.f, rodOffset, 0.f)), {}, trs::identity, material });

			for (uint32 i = 0; i < desc.numTeeth; ++i)
			{
				float angle = i * M_TAU / desc.numTeeth;
				quat localRotation(vec3(0.f, 1.f, 0.f), angle);
				vec3 center = localRotation * vec3(desc.innerRadius + desc.toothLength * 0.5f, 0.f, 0.f);
				vec3 radius(desc.toothLength * 0.5f, desc.height * 0.5f, desc.toothWidth * 0.5f);

				mesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, desc.toothLength, desc.toothWidth * 0.5f, center + vec3(0.f, rodOffset, 0.f), localRotation * vec3(1.f, 0.f, 0.f)), {}, trs::identity, material });

				bounding_capsule capsule;
				capsule.positionA = center + vec3(0.f, rodOffset, 0.f) - localRotation * vec3(desc.toothLength * 0.5f, 0.f, 0.f);
				capsule.positionB = center + vec3(0.f, rodOffset, 0.f) + localRotation * vec3(desc.toothLength * 0.5f, 0.f, 0.f);

				capsule.radius = desc.toothWidth * 0.5f;

				axis.addComponent<collider_component>(collider_component::asCapsule(capsule, 0.2f, desc.friction, desc.density));
			}
		} break;

		case attachment_type_wheel:
		{
			wheel_description desc = attachment.wheel;

			mesh->submeshes.push_back({ primitiveMesh.pushCylinder(21, desc.radius, desc.height, vec3(0.f, rodOffset, 0.f)), {}, trs::identity, material });
			//mesh->submeshes.push_back({ primitiveMesh.pushSphere(21, 21, desc.radius, vec3(0.f, rodOffset, 0.f)), {}, trs::identity, material });

			bounding_cylinder cylinder;
			cylinder.positionA = vec3(0.f, rodOffset - desc.height * 0.5f, 0.f);
			cylinder.positionB = vec3(0.f, rodOffset + desc.height * 0.5f, 0.f);
			cylinder.radius = desc.radius;
			
			axis.addComponent<collider_component>(collider_component::asCylinder(cylinder, 0.2f, desc.friction, desc.density));
		} break;
	}

	if (attachment.rodLength > 0.f)
	{
		mesh->submeshes.push_back({ primitiveMesh.pushCube(vec3(attachment.rodThickness * 0.5f, attachment.rodLength * 0.5f, attachment.rodThickness * 0.5f), false, vec3(0.f, rodOffset * 0.5f, 0.f)), {}, trs::identity, material });
	}
}

static scene_entity createAxis(game_scene& scene, cpu_mesh& primitiveMesh, ref<pbr_material> material,
	vec3 position, quat rotation, gear_description desc, axis_attachment* firstAttachment = 0, axis_attachment* secondAttachment = 0)
{
	scene_entity axis = scene.createEntity("Axis")
		.addComponent<transform_component>(position, rotation);

	auto mesh = make_ref<composite_mesh>();

	axis_attachment centerGearAttachment(0.f, 0.f, desc);
	attach(mesh, primitiveMesh, material, axis, centerGearAttachment, 1.f);

	if (firstAttachment)
	{
		attach(mesh, primitiveMesh, material, axis, *firstAttachment, 1.f);

	}
	if (secondAttachment)
	{
		attach(mesh, primitiveMesh, material, axis, *secondAttachment, -1.f);
	}


	axis.addComponent<raster_component>(mesh);
	axis.addComponent<rigid_body_component>(false);

	return axis;
}

static scene_entity createGearAxis(game_scene& scene, cpu_mesh& primitiveMesh, ref<pbr_material> material,
	vec3 position, quat rotation, float length, uint32 numTeeth, float toothLength, float toothWidth, 
	float friction, float density)
{
	scene_entity axis = scene.createEntity("Gear Axis")
		.addComponent<transform_component>(position, rotation);

	auto mesh = make_ref<composite_mesh>();

	mesh->submeshes.push_back({ primitiveMesh.pushCube(vec3(length * 0.5f, toothWidth * 0.5f, toothWidth * 0.5f)), {}, trs::identity, material });


	float distance = length - toothWidth;
	float stride = distance / (numTeeth - 1);

	float leftOffset = -0.5f * length + 0.5f * toothWidth;

	for (uint32 i = 0; i < numTeeth; ++i)
	{
		float x = leftOffset + i * stride;

		vec3 center(x, toothWidth * 0.5f, 0.f);

		bounding_capsule capsule;
		capsule.positionA = center + vec3(0.f, toothLength * 0.5f, 0.f);
		capsule.positionB = center - vec3(0.f, toothLength * 0.5f, 0.f);
		capsule.radius = toothWidth * 0.5f;

		axis.addComponent<collider_component>(collider_component::asCapsule(capsule, 0.2f, friction, density));

		mesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, toothLength, toothWidth * 0.5f, center), {}, trs::identity, material });
	}

	axis.addComponent<raster_component>(mesh);
	axis.addComponent<rigid_body_component>(false);

	return axis;
}

static scene_entity createWheel(game_scene& scene, cpu_mesh& primitiveMesh, ref<pbr_material> material,
	vec3 position, quat rotation, wheel_description desc)
{
	scene_entity result = scene.createEntity("Wheel")
		.addComponent<transform_component>(position, rotation);

	auto mesh = make_ref<composite_mesh>();

	mesh->submeshes.push_back({ primitiveMesh.pushCylinder(21, desc.radius, desc.height), {}, trs::identity, material });

	bounding_cylinder cylinder;
	cylinder.positionA = vec3(0.f, -desc.height * 0.5f, 0.f);
	cylinder.positionB = vec3(0.f, desc.height * 0.5f, 0.f);
	cylinder.radius = desc.radius;

	result.addComponent<collider_component>(collider_component::asCylinder(cylinder, 0.2f, desc.friction, desc.density));

	result.addComponent<raster_component>(mesh);
	result.addComponent<rigid_body_component>(false);

	return result;
}

static scene_entity createWheelSuspension(game_scene& scene, cpu_mesh& primitiveMesh, ref<pbr_material> material,
	vec3 position, quat rotation, float axisLength, float thickness, bool right)
{
	scene_entity result = scene.createEntity("Wheel suspension")
		.addComponent<transform_component>(position, rotation);

	auto mesh = make_ref<composite_mesh>();

	float xSign = right ? 1.f : -1.f;
	//mesh->submeshes.push_back({ primitiveMesh.pushCube(vec3(axisLength * 0.5f, thickness * 0.5f, thickness * 0.5f), false, vec3(axisLength * 0.5f * xSign, 0.f, 0.f)), {}, trs::identity, material });
	//mesh->submeshes.push_back({ primitiveMesh.pushCube(vec3(thickness * 0.5f, thickness * 0.5f, axisLength * 0.5f), false, vec3(0.f, 0.f, axisLength * 0.5f)), {}, trs::identity, material });
	mesh->submeshes.push_back({ primitiveMesh.pushCylinder(15, thickness * 0.5f, axisLength, vec3(axisLength * 0.5f * xSign, 0.f, 0.f), vec3(1.f, 0.f, 0.f)), {}, trs::identity, material });
	mesh->submeshes.push_back({ primitiveMesh.pushCylinder(15, thickness * 0.5f, axisLength, vec3(0.f, 0.f, axisLength * 0.5f), vec3(0.f, 0.f, 1.f)), {}, trs::identity, material });

	//bounding_capsule capsule;
	//capsule.positionA = vec3(0.f);
	//capsule.positionB = vec3(axisLength * xSign, 0.f, 0.f);
	//capsule.radius = thickness * 0.5f;
	//result.addComponent<collider_component>(collider_component::asCapsule(capsule, 0.2f, 0.f, 4000.f));
	//
	//capsule.positionB = vec3(0.f, 0.f, axisLength);
	//result.addComponent<collider_component>(collider_component::asCapsule(capsule, 0.2f, 0.f, 4000.f));

#if 0
	// This last collider is a hack. This just balances the COG out, because the solver currently seems to have problems, if this is not the case.
	capsule.positionB = vec3(0.f, 0.f, -axisLength);
	result.addComponent<collider_component>(collider_component::asCapsule(capsule, 0.2f, 0.f, 500.f));
#endif

	result.addComponent<raster_component>(mesh);
	result.addComponent<rigid_body_component>(false);

	//float mass = 30.f;
	//mat3 inertia = mat3::identity * (2.f / 5.f * mass * axisLength * axisLength);
	//
	//auto& rb = result.getComponent<rigid_body_component>();
	//rb.invMass = 1.f / mass;
	//rb.invInertia = invert(inertia);

	return result;
}

void vehicle::initialize(game_scene& scene)
{
	float density = 2000.f;


	cpu_mesh primitiveMesh(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents);


	auto material = createPBRMaterial(
		"assets/desert/textures/WoodenCrate2_Albedo.png",
		"assets/desert/textures/WoodenCrate2_Normal.png",
		{}, {});


	motor = scene.createEntity("Motor")
		.addComponent<transform_component>(vec3(0.f, 1.1f, 0.f), quat::identity)
		.addComponent<collider_component>(collider_component::asAABB(bounding_box::fromCenterRadius(vec3(0.f), vec3(1.f, 0.1f, 0.6f)), 0.2f, 0.f, density))
		.addComponent<rigid_body_component>(true);

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

	gear_description steeringWheelDesc;
	steeringWheelDesc.height = 0.1f;
	steeringWheelDesc.innerRadius = 0.4f;
	steeringWheelDesc.numTeeth = 0;
	steeringWheelDesc.toothLength = 0.07f;
	steeringWheelDesc.toothWidth = 0.1f;
	steeringWheelDesc.friction = 0.f;
	steeringWheelDesc.density = density;

	wheel_description wheelDesc;
	wheelDesc.height = 0.3f;
	wheelDesc.radius = 0.7f;
	wheelDesc.friction = 1.f;
	wheelDesc.density = 50.f;


	// Motor gear.
	motorGear = createAxis(scene, primitiveMesh, material, vec3(0.f, 1.35f, 0.f), quat::identity, gearDesc);
	auto motorConstraintHandle = addHingeConstraintFromGlobalPoints(motor, motorGear, vec3(0.f, 1.35f, 0.f), vec3(0.f, 1.f, 0.f));

	auto& motorConstraint = getConstraint(scene, motorConstraintHandle);
	motorConstraint.maxMotorTorque = 500.f;
	motorConstraint.motorVelocity = 0.f;

	// Drive axis.
	float driveAxisLength = 4.5f;
	float driveAxisOffset = 0.26f;
	axis_attachment driveAxisAttachment(driveAxisLength * 0.5f, gearDesc.innerRadius * 0.2f, gearDesc);
	driveAxis = createAxis(scene, primitiveMesh, material, vec3(driveAxisOffset, 1.35f + driveAxisOffset, 0.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(90.f)),
		gearDesc, 0, &driveAxisAttachment);
	addHingeConstraintFromGlobalPoints(motor, driveAxis, vec3(driveAxisOffset, 1.35f + driveAxisOffset, 0.f), vec3(1.f, 0.f, 0.f));

	// Front axis.
	float frontAxisOffsetX = -driveAxisLength * 0.5f + driveAxisOffset * 2.f;
	float frontAxisOffsetZ = driveAxisOffset;
	float axisLength = 1.5f;
	float suspensionLength = 0.4f;

	float frontAxisLength = axisLength/* - suspensionLength * 2.f*/;
	axis_attachment firstFrontAxisAttachment(axisLength - frontAxisOffsetZ, gearDesc.innerRadius * 0.2f);
	axis_attachment secondFrontAxisAttachment(axisLength + frontAxisOffsetZ, gearDesc.innerRadius * 0.2f);
	vec3 frontAxisPos(frontAxisOffsetX, 1.35f + driveAxisOffset, frontAxisOffsetZ);
	frontAxis = createAxis(scene, primitiveMesh, material, frontAxisPos, quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f)), gearDesc, &firstFrontAxisAttachment, &secondFrontAxisAttachment);
	addFixedConstraintFromGlobalPoints(motor, frontAxis, frontAxisPos);
	addFixedConstraintFromGlobalPoints(motor, frontAxis, frontAxisPos + vec3(0.f, 0.f, axisLength));
	addFixedConstraintFromGlobalPoints(motor, frontAxis, frontAxisPos + vec3(0.f, 0.f, -axisLength));

	// Rear axis.
	float rearAxisOffsetX = driveAxisLength * 0.5f;
	float rearAxisOffsetZ = -driveAxisOffset;
	axis_attachment firstRearAxisAttachment(axisLength - rearAxisOffsetZ, gearDesc.innerRadius * 0.2f, wheelDesc);
	axis_attachment secondRearAxisAttachment(axisLength + rearAxisOffsetZ, gearDesc.innerRadius * 0.2f, wheelDesc);
	rearAxis = createAxis(scene, primitiveMesh, material, vec3(rearAxisOffsetX, 1.35f + driveAxisOffset, rearAxisOffsetZ), quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f)), gearDesc,
		&firstRearAxisAttachment, &secondRearAxisAttachment);
	addHingeConstraintFromGlobalPoints(motor, rearAxis, vec3(rearAxisOffsetX, 1.35f + driveAxisOffset, rearAxisOffsetZ), vec3(0.f, 0.f, 1.f));

	// Steering wheel.
	axis_attachment steeringWheelAttachment(2.f, steeringWheelDesc.innerRadius * 0.2f, gearDesc);
	quat steeringWheelRot(vec3(0.f, 0.f, 1.f), deg2rad(-80.f));
	steeringWheel = createAxis(scene, primitiveMesh, material, vec3(0.8f, 2.3f, 0.f),
		steeringWheelRot, steeringWheelDesc, 0, &steeringWheelAttachment);
	auto steeringWheelConstraintHandle = addHingeConstraintFromGlobalPoints(motor, steeringWheel, vec3(0.8f, 2.3f, 0.f), steeringWheelRot * vec3(0.f, -1.f, 0.f));

	auto& steeringWheelConstraint = getConstraint(scene, steeringWheelConstraintHandle);
	steeringWheelConstraint.motorType = constraint_position_motor;
	steeringWheelConstraint.maxMotorTorque = 300.f;
	steeringWheelConstraint.motorTargetAngle = 0.f;

	// Steering axis.
	vec3 steeringAxisPos(frontAxisOffsetX + 0.49f, 1.35f + driveAxisOffset + 0.06f, 0.f);
	float steeringAxisLength = axisLength * 1.05f;
	steeringAxis = createGearAxis(scene, primitiveMesh, material, steeringAxisPos, steeringWheelRot * quat(vec3(0.f, 1.f, 0.f), deg2rad(90.f)), steeringAxisLength, 8,
		gearDesc.toothLength, gearDesc.toothWidth, gearDesc.friction, gearDesc.density);
	addSliderConstraintFromGlobalPoints(motor, steeringAxis, steeringAxisPos, vec3(0.f, 0.f, 1.f), -4.f, 4.f);

	vec3 leftSteeringAxisAttachmentPos = steeringAxisPos + vec3(0.f, 0.f, steeringAxisLength * 0.5f);
	vec3 rightSteeringAxisAttachmentPos = steeringAxisPos - vec3(0.f, 0.f, steeringAxisLength * 0.5f);

	// Left wheel suspension.
	vec3 leftWheelSuspensionPos = frontAxisPos + vec3(0.f, 0.f, axisLength - frontAxisOffsetZ);
	vec3 leftWheelSuspensionAttachmentPos = leftWheelSuspensionPos + vec3(suspensionLength, 0.f, 0.f);
	leftWheelSuspension = createWheelSuspension(scene, primitiveMesh, material, leftWheelSuspensionPos, quat(vec3(0.f, 1.f, 0.f), deg2rad(90.f)), suspensionLength, 0.1f, false);
	addHingeConstraintFromGlobalPoints(motor, leftWheelSuspension, leftWheelSuspensionPos, vec3(0.f, 1.f, 0.f), deg2rad(-45.f), deg2rad(45.f));
	//addDistanceConstraintFromGlobalPoints(steeringAxis, leftWheelSuspension, leftSteeringAxisAttachmentPos, leftWheelSuspensionAttachmentPos);

	// Right wheel suspension.
	vec3 rightWheelSuspensionPos = frontAxisPos + vec3(0.f, 0.f, -axisLength - frontAxisOffsetZ);
	vec3 rightWheelSuspensionAttachmentPos = rightWheelSuspensionPos + vec3(suspensionLength, 0.f, 0.f);
	rightWheelSuspension = createWheelSuspension(scene, primitiveMesh, material, rightWheelSuspensionPos, quat(vec3(0.f, 1.f, 0.f), deg2rad(90.f)), suspensionLength, 0.1f, true);
	addHingeConstraintFromGlobalPoints(motor, rightWheelSuspension, rightWheelSuspensionPos, vec3(0.f, 1.f, 0.f), deg2rad(-45.f), deg2rad(45.f));
	//addDistanceConstraintFromGlobalPoints(steeringAxis, rightWheelSuspension, rightSteeringAxisAttachmentPos, rightWheelSuspensionAttachmentPos);


	// Left front wheel.
	vec3 leftFrontWheelPos = leftWheelSuspensionPos + vec3(0.f, 0.f, suspensionLength * 0.5f);
	leftFrontWheel = createWheel(scene, primitiveMesh, material, leftFrontWheelPos, quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f)), wheelDesc);

	// Right front wheel.
	vec3 rightFrontWheelPos = rightWheelSuspensionPos - vec3(0.f, 0.f, suspensionLength * 0.5f);
	rightFrontWheel = createWheel(scene, primitiveMesh, material, rightFrontWheelPos, quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f)), wheelDesc);

	addHingeConstraintFromGlobalPoints(leftFrontWheel, leftWheelSuspension, leftFrontWheelPos, vec3(0.f, 0.f, 1.f));
	addHingeConstraintFromGlobalPoints(rightFrontWheel, rightWheelSuspension, rightFrontWheelPos, vec3(0.f, 0.f, 1.f));


	leftWheelArm = scene.createEntity("Left wheel arm")
		.addComponent<transform_component>((leftSteeringAxisAttachmentPos + leftWheelSuspensionAttachmentPos) * 0.5f, quat::identity)
		.addComponent<rigid_body_component>(false);
	rightWheelArm = scene.createEntity("Right wheel arm")
		.addComponent<transform_component>((rightSteeringAxisAttachmentPos + rightWheelSuspensionAttachmentPos) * 0.5f, quat::identity)
		.addComponent<rigid_body_component>(false);

	addBallConstraintFromGlobalPoints(leftWheelSuspension, leftWheelArm, leftWheelSuspensionAttachmentPos);
	addBallConstraintFromGlobalPoints(steeringAxis, leftWheelArm, leftSteeringAxisAttachmentPos);

	addBallConstraintFromGlobalPoints(rightWheelSuspension, rightWheelArm, rightWheelSuspensionAttachmentPos);
	addBallConstraintFromGlobalPoints(steeringAxis, rightWheelArm, rightSteeringAxisAttachmentPos);



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
