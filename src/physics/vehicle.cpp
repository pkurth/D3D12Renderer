#include "pch.h"
#include "vehicle.h"


struct gear_description
{
	float height; 
	float cylinderRadius;
	float cylinderInnerRadius;
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
	float innerRadius;

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

static void attach(mesh_builder& builder, ref<pbr_material> material, scene_entity axis, axis_attachment& attachment, float sign)
{
	float rodOffset = attachment.rodLength * sign;

	switch (attachment.type)
	{
		case attachment_type_gear: 
		{
			gear_description desc = attachment.gear;

			if (desc.cylinderInnerRadius > 0.f)
			{
				hollow_cylinder_mesh_desc m;
				m.center = vec3(0.f, rodOffset, 0.f);
				m.height = desc.height;
				m.radius = desc.cylinderRadius;
				m.innerRadius = desc.cylinderInnerRadius;
				m.slices = 21;

				builder.pushHollowCylinder(m);
			}
			else
			{
				cylinder_mesh_desc m;
				m.center = vec3(0.f, rodOffset, 0.f);
				m.height = desc.height;
				m.radius = desc.cylinderRadius;
				m.slices = 21;

				builder.pushCylinder(m);
			}

			for (uint32 i = 0; i < desc.numTeeth; ++i)
			{
				float angle = i * M_TAU / desc.numTeeth;
				quat localRotation(vec3(0.f, 1.f, 0.f), angle);
				vec3 center = localRotation * vec3(desc.cylinderRadius + desc.toothLength * 0.5f, 0.f, 0.f);
				vec3 radius(desc.toothLength * 0.5f, desc.height * 0.5f, desc.toothWidth * 0.5f);
				
				capsule_mesh_desc m;
				m.center = center + vec3(0.f, rodOffset, 0.f);
				m.height = desc.toothLength;
				m.radius = desc.toothWidth * 0.5f;
				m.rotation = localRotation * quat(vec3(0.f, 0.f, 1.f), deg2rad(90.f));
				builder.pushCapsule(m);

				bounding_capsule capsule;
				capsule.positionA = center + vec3(0.f, rodOffset, 0.f) - localRotation * vec3(desc.toothLength * 0.5f, 0.f, 0.f);
				capsule.positionB = center + vec3(0.f, rodOffset, 0.f) + localRotation * vec3(desc.toothLength * 0.5f, 0.f, 0.f);

				capsule.radius = desc.toothWidth * 0.5f;

				axis.addComponent<collider_component>(collider_component::asCapsule(capsule, { physics_material_type_wood, 0.2f, desc.friction, desc.density }));
			}
		} break;

		case attachment_type_wheel:
		{
			wheel_description desc = attachment.wheel;

			hollow_cylinder_mesh_desc m;
			m.center = vec3(0.f, rodOffset, 0.f);
			m.height = desc.height;
			m.radius = desc.radius;
			m.innerRadius = desc.innerRadius;
			m.slices = 21;
			builder.pushHollowCylinder(m);

			bounding_cylinder cylinder;
			cylinder.positionA = vec3(0.f, rodOffset - desc.height * 0.5f, 0.f);
			cylinder.positionB = vec3(0.f, rodOffset + desc.height * 0.5f, 0.f);
			cylinder.radius = desc.radius;
			
			axis.addComponent<collider_component>(collider_component::asCylinder(cylinder, { physics_material_type_wood, 0.2f, desc.friction, desc.density }));
		} break;
	}

	if (attachment.rodLength > 0.f)
	{
		box_mesh_desc m;
		m.radius = vec3(attachment.rodThickness * 0.5f, attachment.rodLength * 0.5f, attachment.rodThickness * 0.5f);
		m.center = vec3(0.f, rodOffset * 0.5f, 0.f);
		builder.pushBox(m);
	}
}

static scene_entity createAxis(game_scene& scene, mesh_builder& builder, ref<pbr_material> material,
	vec3 position, quat rotation, gear_description desc, axis_attachment* firstAttachment = 0, axis_attachment* secondAttachment = 0)
{
	scene_entity axis = scene.createEntity("Axis")
		.addComponent<transform_component>(position, rotation);

	auto mesh = make_ref<multi_mesh>();

	axis_attachment centerGearAttachment(0.f, 0.f, desc);
	attach(builder, material, axis, centerGearAttachment, 1.f);

	if (firstAttachment)
	{
		attach(builder, material, axis, *firstAttachment, 1.f);

	}
	if (secondAttachment)
	{
		attach(builder, material, axis, *secondAttachment, -1.f);
	}

	mesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity, material });

	axis.addComponent<raster_component>(mesh);
	axis.addComponent<rigid_body_component>(false);

	return axis;
}

static scene_entity createGearAxis(game_scene& scene, mesh_builder& builder, ref<pbr_material> material,
	vec3 position, quat rotation, float length, uint32 numTeeth, float toothLength, float toothWidth, 
	float friction, float density)
{
	scene_entity axis = scene.createEntity("Gear Axis")
		.addComponent<transform_component>(position, rotation);

	auto mesh = make_ref<multi_mesh>();

	box_mesh_desc m;
	m.radius = vec3(length * 0.5f, toothWidth * 0.5f, toothWidth * 0.5f);
	builder.pushBox(m);


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

		axis.addComponent<collider_component>(collider_component::asCapsule(capsule, { physics_material_type_wood, 0.2f, friction, density }));

		capsule_mesh_desc m;
		m.center = center;
		m.height = toothLength;
		m.radius = toothWidth * 0.5f;
		builder.pushCapsule(m);
	}

	axis.addComponent<raster_component>(mesh);
	axis.addComponent<rigid_body_component>(false);

	mesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity, material });

	return axis;
}

static scene_entity createWheel(game_scene& scene, mesh_builder& builder, ref<pbr_material> material,
	vec3 position, quat rotation, wheel_description desc)
{
	scene_entity result = scene.createEntity("Wheel")
		.addComponent<transform_component>(position, rotation);

	auto mesh = make_ref<multi_mesh>();

	hollow_cylinder_mesh_desc m;
	m.height = desc.height;
	m.radius = desc.radius;
	m.innerRadius = desc.innerRadius;
	m.slices = 21;
	builder.pushHollowCylinder(m);

	bounding_cylinder cylinder;
	cylinder.positionA = vec3(0.f, -desc.height * 0.5f, 0.f);
	cylinder.positionB = vec3(0.f, desc.height * 0.5f, 0.f);
	cylinder.radius = desc.radius;

	result.addComponent<collider_component>(collider_component::asCylinder(cylinder, { physics_material_type_wood, 0.2f, desc.friction, desc.density }));

	result.addComponent<raster_component>(mesh);
	result.addComponent<rigid_body_component>(false);

	mesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity, material });

	return result;
}

static scene_entity createWheelSuspension(game_scene& scene, mesh_builder& builder, ref<pbr_material> material,
	vec3 position, quat rotation, float axisLength, float thickness, bool right)
{
	scene_entity result = scene.createEntity("Wheel suspension")
		.addComponent<transform_component>(position, rotation);

	auto mesh = make_ref<multi_mesh>();

	float xSign = right ? 1.f : -1.f;

	cylinder_mesh_desc m;
	m.height = axisLength;
	m.center = vec3(axisLength * 0.5f * xSign, 0.f, 0.f);
	m.radius = thickness * 0.5f;
	m.rotation = quat(vec3(0.f, 0.f, 1.f), deg2rad(90.f));
	builder.pushCylinder(m);

	m.center = vec3(0.f, 0.f, axisLength * 0.5f);
	m.rotation = quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f));
	builder.pushCylinder(m);

	// These rigid bodies don't have colliders, since they penetrate the wheels.

	result.addComponent<raster_component>(mesh);
	result.addComponent<rigid_body_component>(false);

	mesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity, material });

	return result;
}

static scene_entity createRod(game_scene& scene, mesh_builder& builder, ref<pbr_material> material, 
	vec3 from, vec3 to, float thickness)
{
	vec3 position = (from + to) * 0.5f;
	vec3 axis = normalize(to - from);
	float len = length(to - from);
	quat rotation = rotateFromTo(vec3(0.f, 1.f, 0.f), axis);

	scene_entity result = scene.createEntity("Rod")
		.addComponent<transform_component>(position, rotation);

	auto mesh = make_ref<multi_mesh>();

	box_mesh_desc m;
	m.radius = vec3(thickness * 0.5f, len * 0.5f, thickness * 0.5f);
	builder.pushBox(m);

	result.addComponent<raster_component>(mesh);
	result.addComponent<rigid_body_component>(false);

	mesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity, material });

	return result;
}

void vehicle::initialize(game_scene& scene, vec3 initialMotorPosition, float initialRotation)
{
	float density = 2000.f;


	mesh_builder builder;


	auto material = createPBRMaterial(
		"assets/desert/textures/WoodenCrate2_Albedo.png",
		"assets/desert/textures/WoodenCrate2_Normal.png",
		{}, {});


	motor = scene.createEntity("Motor")
		.addComponent<transform_component>(vec3(0.f, 0.f, 0.f), quat::identity)
		.addComponent<collider_component>(collider_component::asAABB(bounding_box::fromCenterRadius(vec3(0.f), vec3(0.6f, 0.1f, 1.f)), { physics_material_type_wood, 0.2f, 0.f, density }))
		.addComponent<rigid_body_component>(false);

	{	
		auto motorMesh = make_ref<multi_mesh>();

		box_mesh_desc m;
		m.radius = vec3(0.6f, 0.1f, 1.f);
		builder.pushBox(m);

		motorMesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity, material });

		motor.addComponent<raster_component>(motorMesh);
	}

	gear_description motorGearDesc;
	motorGearDesc.height = 0.1f;
	motorGearDesc.cylinderRadius = 0.2f;
	motorGearDesc.cylinderInnerRadius = 0.f;
	motorGearDesc.numTeeth = 8;
	motorGearDesc.toothLength = 0.07f;
	motorGearDesc.toothWidth = 0.1f;
	motorGearDesc.friction = 0.f;
	motorGearDesc.density = density;

	float rodThickness = 0.05f;

	gear_description steeringWheelDesc;
	steeringWheelDesc.height = 0.1f;
	steeringWheelDesc.cylinderRadius = 0.4f;
	steeringWheelDesc.cylinderInnerRadius = 0.2f;
	steeringWheelDesc.numTeeth = 0;
	steeringWheelDesc.toothLength = 0.07f;
	steeringWheelDesc.toothWidth = 0.1f;
	steeringWheelDesc.friction = 0.f;
	steeringWheelDesc.density = density;

	wheel_description wheelDesc;
	wheelDesc.height = 0.3f;
	wheelDesc.radius = 0.7f;
	wheelDesc.innerRadius = wheelDesc.radius * 0.4f;
	wheelDesc.friction = 1.f;
	wheelDesc.density = 50.f;


	float motorGearY = 0.25f;
	float gearOffset = 0.26f;

	// Motor gear.
	motorGear = createAxis(scene, builder, material, vec3(0.f, motorGearY, 0.f), quat::identity, motorGearDesc);
	auto motorConstraintHandle = addHingeConstraintFromGlobalPoints(motor, motorGear, vec3(0.f, motorGearY, 0.f), vec3(0.f, 1.f, 0.f));

	auto& motorConstraint = getConstraint(scene, motorConstraintHandle);
	motorConstraint.maxMotorTorque = 500.f;
	motorConstraint.motorVelocity = 0.f;

	// Drive axis.
	float driveAxisLength = 4.5f;
	axis_attachment driveAxisAttachment(driveAxisLength * 0.57f - 1.1f, rodThickness, motorGearDesc);
	driveAxis = createAxis(scene, builder, material, vec3(0.f, motorGearY + gearOffset, gearOffset), quat(vec3(-1.f, 0.f, 0.f), deg2rad(90.f)),
		motorGearDesc, 0, &driveAxisAttachment);
	addHingeConstraintFromGlobalPoints(motor, driveAxis, vec3(0.f, motorGearY + gearOffset, gearOffset), vec3(0.f, 0.f, 1.f));

	// Front axis.
	float axisLength = 1.5f;
	float suspensionLength = 0.4f;

	float frontAxisOffsetZ = -driveAxisLength * 0.5f + gearOffset * 2.f;
	vec3 frontAxisPos(0.f, motorGearY + gearOffset, frontAxisOffsetZ);
	frontAxis = createRod(scene, builder, material, frontAxisPos + vec3(axisLength, 0.f, 0.f), frontAxisPos - vec3(axisLength, 0.f, 0.f), 0.05f);
	addFixedConstraintFromGlobalPoints(motor, frontAxis, frontAxisPos);

	// Steering wheel.
	axis_attachment steeringWheelAttachment(2.f, rodThickness, motorGearDesc);
	quat steeringWheelRot(vec3(-1.f, 0.f, 0.f), deg2rad(-80.f));
	vec3 steeringWheelPos(0.f, 1.12f, 0.81f);
	steeringWheel = createAxis(scene, builder, material, steeringWheelPos,
		steeringWheelRot, steeringWheelDesc, 0, &steeringWheelAttachment);
	auto steeringWheelConstraintHandle = addHingeConstraintFromGlobalPoints(motor, steeringWheel, steeringWheelPos, steeringWheelRot * vec3(0.f, -1.f, 0.f));

	auto& steeringWheelConstraint = getConstraint(scene, steeringWheelConstraintHandle);
	steeringWheelConstraint.motorType = constraint_position_motor;
	steeringWheelConstraint.maxMotorTorque = 1000.f;
	steeringWheelConstraint.motorTargetAngle = 0.f;

	// Steering axis.
	vec3 steeringAxisPos(0.f, motorGearY + gearOffset + 0.06f, frontAxisOffsetZ + 0.49f);
	float steeringAxisLength = axisLength * 1.05f;
	steeringAxis = createGearAxis(scene, builder, material, steeringAxisPos, steeringWheelRot, steeringAxisLength, 8,
		motorGearDesc.toothLength, motorGearDesc.toothWidth, motorGearDesc.friction, motorGearDesc.density);
	addSliderConstraintFromGlobalPoints(motor, steeringAxis, steeringAxisPos, vec3(1.f, 0.f, 0.f), -4.f, 4.f);

	vec3 leftSteeringAxisAttachmentPos = steeringAxisPos - vec3(steeringAxisLength * 0.5f, 0.f, 0.f);
	vec3 rightSteeringAxisAttachmentPos = steeringAxisPos + vec3(steeringAxisLength * 0.5f, 0.f, 0.f);

	// Left wheel suspension.
	vec3 leftWheelSuspensionPos = frontAxisPos - vec3(axisLength, 0.f, 0.f);
	vec3 leftWheelSuspensionAttachmentPos = leftWheelSuspensionPos + vec3(0.f, 0.f, suspensionLength);
	leftWheelSuspension = createWheelSuspension(scene, builder, material, leftWheelSuspensionPos, quat::identity, suspensionLength, 0.1f, false);
	addHingeConstraintFromGlobalPoints(motor, leftWheelSuspension, leftWheelSuspensionPos, vec3(0.f, 1.f, 0.f), deg2rad(-45.f), deg2rad(45.f));

	// Right wheel suspension.
	vec3 rightWheelSuspensionPos = frontAxisPos + vec3(axisLength, 0.f, 0.f);
	vec3 rightWheelSuspensionAttachmentPos = rightWheelSuspensionPos + vec3(0.f, 0.f, suspensionLength);
	rightWheelSuspension = createWheelSuspension(scene, builder, material, rightWheelSuspensionPos, quat::identity, suspensionLength, 0.1f, true);
	addHingeConstraintFromGlobalPoints(motor, rightWheelSuspension, rightWheelSuspensionPos, vec3(0.f, 1.f, 0.f), deg2rad(-45.f), deg2rad(45.f));


	// Left front wheel.
	vec3 leftFrontWheelPos = leftWheelSuspensionPos - vec3(suspensionLength * 0.5f, 0.f, 0.f);
	leftFrontWheel = createWheel(scene, builder, material, leftFrontWheelPos, quat(vec3(0.f, 0.f, 1.f), deg2rad(90.f)), wheelDesc);

	// Right front wheel.
	vec3 rightFrontWheelPos = rightWheelSuspensionPos + vec3(suspensionLength * 0.5f, 0.f, 0.f);
	rightFrontWheel = createWheel(scene, builder, material, rightFrontWheelPos, quat(vec3(0.f, 0.f, 1.f), deg2rad(90.f)), wheelDesc);

	addHingeConstraintFromGlobalPoints(leftFrontWheel, leftWheelSuspension, leftFrontWheelPos, vec3(1.f, 0.f, 0.f));
	addHingeConstraintFromGlobalPoints(rightFrontWheel, rightWheelSuspension, rightFrontWheelPos, vec3(1.f, 0.f, 0.f));


	leftWheelArm = createRod(scene, builder, material, leftSteeringAxisAttachmentPos, leftWheelSuspensionAttachmentPos, 0.05f);
	rightWheelArm = createRod(scene, builder, material, rightSteeringAxisAttachmentPos, rightWheelSuspensionAttachmentPos, 0.05f);

	addBallConstraintFromGlobalPoints(leftWheelSuspension, leftWheelArm, leftWheelSuspensionAttachmentPos);
	addBallConstraintFromGlobalPoints(steeringAxis, leftWheelArm, leftSteeringAxisAttachmentPos);

	addBallConstraintFromGlobalPoints(rightWheelSuspension, rightWheelArm, rightWheelSuspensionAttachmentPos);
	addBallConstraintFromGlobalPoints(steeringAxis, rightWheelArm, rightSteeringAxisAttachmentPos);





	gear_description rearAxisGearDesc = motorGearDesc;
	rearAxisGearDesc.cylinderRadius = 0.5f;
	rearAxisGearDesc.cylinderInnerRadius = 0.4f;
	rearAxisGearDesc.numTeeth = 17;


	// Rear axis.
	float rearAxisOffsetZ = driveAxisLength * 0.505f;
	float rearAxisOffsetX = -gearOffset;
	differentialSunGear = createAxis(scene, builder, material, vec3(rearAxisOffsetX, motorGearY + gearOffset, rearAxisOffsetZ), quat(vec3(0.f, 0.f, -1.f), deg2rad(90.f)), rearAxisGearDesc);
	addHingeConstraintFromGlobalPoints(motor, differentialSunGear, vec3(rearAxisOffsetX, motorGearY + gearOffset, rearAxisOffsetZ), vec3(1.f, 0.f, 0.f));

	{
		box_mesh_desc m;
		m.radius = vec3(0.01f, gearOffset * 0.5f + 0.05f, 0.01f);
		m.center = vec3(-rearAxisGearDesc.cylinderRadius * 0.9f, gearOffset * 0.5f + 0.05f, 0.f);
		builder.pushBox(m);
		differentialSunGear.getComponent<raster_component>().mesh->submeshes.push_back({ builder.endSubmesh(), {}, trs::identity, material });
	}

	// Differential.
	vec3 differentialSpiderGearPos(0.11f, motorGearY + gearOffset * 2.f, rearAxisOffsetZ);
	axis_attachment spiderGearAttachment(0.2f, 0.02f);
	differentialSpiderGear = createAxis(scene, builder, material, differentialSpiderGearPos, quat::identity, motorGearDesc, &spiderGearAttachment);
	addHingeConstraintFromGlobalPoints(differentialSunGear, differentialSpiderGear, differentialSpiderGearPos, vec3(0.f, 1.f, 0.f));

	vec3 leftRearWheelPos = differentialSpiderGearPos + vec3(-gearOffset, -gearOffset, 0.f);
	vec3 rightRearWheelPos = differentialSpiderGearPos + vec3(gearOffset, -gearOffset, 0.f);

	axis_attachment leftWheelAttachment(axisLength + differentialSpiderGearPos.x, rodThickness, wheelDesc);
	axis_attachment rightWheelAttachment(axisLength - differentialSpiderGearPos.x, rodThickness, wheelDesc);

	leftRearWheel = createAxis(scene, builder, material, leftRearWheelPos, quat(vec3(0.f, 0.f, -1.f), deg2rad(90.f)), motorGearDesc, 0, &leftWheelAttachment);
	rightRearWheel = createAxis(scene, builder, material, rightRearWheelPos, quat(vec3(0.f, 0.f, -1.f), deg2rad(90.f)), motorGearDesc, &rightWheelAttachment, 0);

	addHingeConstraintFromGlobalPoints(motor, leftRearWheel, leftRearWheelPos, vec3(1.f, 0.f, 0.f));
	addHingeConstraintFromGlobalPoints(motor, rightRearWheel, rightRearWheelPos, vec3(1.f, 0.f, 0.f));


	quat rotation(vec3(0.f, 1.f, 0.f), initialRotation);

	auto mesh = builder.createDXMesh();
	for (uint32 i = 0; i < arraysize(parts); ++i)
	{
		parts[i].getComponent<raster_component>().mesh->mesh = mesh;

		auto& transform = parts[i].getComponent<transform_component>();
		transform.position = rotation * transform.position + initialMotorPosition;
		transform.rotation = rotation * transform.rotation;
	}
}

vehicle vehicle::create(game_scene& scene, vec3 initialMotorPosition, float initialRotation)
{
	vehicle v;
	v.initialize(scene, initialMotorPosition, initialRotation);
	return v;
}
