#include "pch.h"
#include "ragdoll.h"

#ifndef PHYSICS_ONLY
#include "pbr.h"
#include "mesh.h"
#include "geometry.h"
#include "imgui.h"
#endif

void humanoid_ragdoll::initialize(scene& appScene, vec3 initialHipPosition)
{
	float scale = 0.42f; // This file is completely hardcoded. I initially screwed up the scaling a bit, so this factor brings everything to the correct scale (and therefore weight).

	bool ragdollKinematic = false;
	float ragdollGravityFactor = 1.f;
	float ragdollDensity = 985.f; // Average density of human body in kg/m3.
	float ragdollFriction = 0.9f;

	trs torsoTransform(initialHipPosition + scale * vec3(0.f, 0.f, 0.f), quat::identity);
	trs headTransform(initialHipPosition + scale * vec3(0.f, 1.45f, 0.f), quat::identity);
	trs leftUpperArmTransform(initialHipPosition + scale * vec3(-0.6f, 0.75f, 0.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(-30.f)));
	trs leftLowerArmTransform(initialHipPosition + scale * vec3(-0.884f, 0.044f, -0.043f), quat(vec3(0.f, 0.f, 1.f), deg2rad(-20.f)));
	trs rightUpperArmTransform(initialHipPosition + scale * vec3(0.6f, 0.75f, 0.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(30.f)));
	trs rightLowerArmTransform(initialHipPosition + scale * vec3(0.884f, 0.044f, -0.043f), quat(vec3(0.f, 0.f, 1.f), deg2rad(20.f)));
	trs leftUpperLegTransform(initialHipPosition + scale * vec3(-0.371f, -0.812f, 0.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(-10.f)));
	trs leftLowerLegTransform(initialHipPosition + scale * vec3(-0.452f, -1.955f, 0.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(-3.5f)));
	trs leftFootTransform(initialHipPosition + scale * vec3(-0.498f, -2.585f, -0.18f), quat::identity);
	trs leftToesTransform(initialHipPosition + scale * vec3(-0.498f, -2.585f, -0.637f), quat::identity);
	trs rightUpperLegTransform(initialHipPosition + scale * vec3(0.371f, -0.812f, 0.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(10.f)));
	trs rightLowerLegTransform(initialHipPosition + scale * vec3(0.452f, -1.955f, 0.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(3.5f)));
	trs rightFootTransform(initialHipPosition + scale * vec3(0.498f, -2.585f, -0.18f), quat::identity);
	trs rightToesTransform(initialHipPosition + scale * vec3(0.498f, -2.585f, -0.637f), quat::identity);

	torso = appScene.createEntity("Torso")
		.addComponent<trs>(torsoTransform)
		.addComponent<collider_component>(bounding_capsule{ scale * vec3(-0.2f, 0.f, 0.f),    scale *  vec3(0.2f, 0.f, 0.f),    scale * 0.25f }, 0.2f, 0.5f, ragdollDensity)
		.addComponent<collider_component>(bounding_capsule{ scale * vec3(-0.16f, 0.32f, 0.f), scale *  vec3(0.16f, 0.32f, 0.f), scale * 0.2f }, 0.2f, 0.5f, ragdollDensity)
		.addComponent<collider_component>(bounding_capsule{ scale * vec3(-0.14f, 0.62f, 0.f), scale *  vec3(0.14f, 0.62f, 0.f), scale * 0.22f }, 0.2f, 0.5f, ragdollDensity)
		.addComponent<collider_component>(bounding_capsule{ scale * vec3(-0.14f, 0.92f, 0.f), scale *  vec3(0.14f, 0.92f, 0.f), scale * 0.2f }, 0.2f, 0.5f, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	head = appScene.createEntity("Head")
		.addComponent<trs>(headTransform)
		.addComponent<collider_component>(bounding_capsule{ scale * vec3(0.f, -0.075f, 0.f), scale * vec3(0.f, 0.075f, 0.f), scale * 0.25f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	leftUpperArm = appScene.createEntity("Left upper arm")
		.addComponent<trs>(leftUpperArmTransform)
		.addComponent<collider_component>(bounding_capsule{ scale * vec3(0.f, -0.2f, 0.f), scale * vec3(0.f, 0.2f, 0.f), scale * 0.15f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	leftLowerArm = appScene.createEntity("Left lower arm")
		.addComponent<trs>(leftLowerArmTransform)
		.addComponent<collider_component>(bounding_capsule{ scale * vec3(0.f, -0.2f, 0.f), scale * vec3(0.f, 0.2f, 0.f), scale * 0.15f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	rightUpperArm = appScene.createEntity("Right upper arm")
		.addComponent<trs>(rightUpperArmTransform)
		.addComponent<collider_component>(bounding_capsule{ scale * vec3(0.f, -0.2f, 0.f), scale * vec3(0.f, 0.2f, 0.f), scale * 0.15f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	rightLowerArm = appScene.createEntity("Right lower arm")
		.addComponent<trs>(rightLowerArmTransform)
		.addComponent<collider_component>(bounding_capsule{ scale * vec3(0.f, -0.2f, 0.f), scale * vec3(0.f, 0.2f, 0.f), scale * 0.15f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	leftUpperLeg = appScene.createEntity("Left upper leg")
		.addComponent<trs>(leftUpperLegTransform)
		.addComponent<collider_component>(bounding_capsule{ scale * vec3(0.f, -0.3f, 0.f), scale * vec3(0.f, 0.3f, 0.f), scale * 0.25f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	leftLowerLeg = appScene.createEntity("Left lower leg")
		.addComponent<trs>(leftLowerLegTransform)
		.addComponent<collider_component>(bounding_capsule{ scale * vec3(0.f, -0.3f, 0.f), scale * vec3(0.f, 0.3f, 0.f), scale * 0.18f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	leftFoot = appScene.createEntity("Left foot")
		.addComponent<trs>(leftFootTransform)
		.addComponent<collider_component>(bounding_box::fromCenterRadius(scale * vec3(0.f), scale * vec3(0.1587f, 0.1f, 0.3424f)), 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	leftToes = appScene.createEntity("Left toes")
		.addComponent<trs>(leftToesTransform)
		.addComponent<collider_component>(bounding_capsule{ scale * vec3(-0.0587f, 0.f, 0.f), scale * vec3(0.0587f, 0.f, 0.f), scale * 0.1f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	rightUpperLeg = appScene.createEntity("Right upper leg")
		.addComponent<trs>(rightUpperLegTransform)
		.addComponent<collider_component>(bounding_capsule{ scale * vec3(0.f, -0.3f, 0.f), scale * vec3(0.f, 0.3f, 0.f), scale * 0.25f }, scale * 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	rightLowerLeg = appScene.createEntity("Right lower leg")
		.addComponent<trs>(rightLowerLegTransform)
		.addComponent<collider_component>(bounding_capsule{ scale * vec3(0.f, -0.3f, 0.f), scale * vec3(0.f, 0.3f, 0.f), scale * 0.18f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	rightFoot = appScene.createEntity("Right foot")
		.addComponent<trs>(rightFootTransform)
		.addComponent<collider_component>(bounding_box::fromCenterRadius(scale * vec3(0.f), scale * vec3(0.1587f, 0.1f, 0.3424f)), 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	rightToes = appScene.createEntity("Right toes")
		.addComponent<trs>(rightToesTransform)
		.addComponent<collider_component>(bounding_capsule{ scale * vec3(-0.0587f, 0.f, 0.f), scale * vec3(0.0587f, 0.f, 0.f), scale * 0.1f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	neckConstraint = addConeTwistConstraintFromGlobalPoints(torso, head, transformPosition(torsoTransform, scale * vec3(0.f, 1.2f, 0.f)), vec3(0.f, 1.f, 0.f), deg2rad(50.f), deg2rad(90.f));
	leftShoulderConstraint = addConeTwistConstraintFromGlobalPoints(torso, leftUpperArm, transformPosition(torsoTransform, scale * vec3(-0.4f, 1.f, 0.f)), vec3(-1.f, 0.f, 0.f), deg2rad(130.f), deg2rad(90.f));
	leftElbowConstraint = addHingeJointConstraintFromGlobalPoints(leftUpperArm, leftLowerArm, transformPosition(leftUpperArmTransform, scale * vec3(0.f, -0.42f, 0.f)), normalize(vec3(1.f, 0.f, 1.f)), deg2rad(-5.f), deg2rad(85.f));
	rightShoulderConstraint = addConeTwistConstraintFromGlobalPoints(torso, rightUpperArm, transformPosition(torsoTransform, scale * vec3(0.4f, 1.f, 0.f)), vec3(1.f, 0.f, 0.f), deg2rad(130.f), deg2rad(90.f));
	rightElbowConstraint = addHingeJointConstraintFromGlobalPoints(rightUpperArm, rightLowerArm, transformPosition(rightUpperArmTransform, scale * vec3(0.f, -0.42f, 0.f)), normalize(vec3(1.f, 0.f, -1.f)), deg2rad(-5.f), deg2rad(85.f));
	
	leftHipConstraint = addConeTwistConstraintFromGlobalPoints(torso, leftUpperLeg, transformPosition(torsoTransform, scale * vec3(-0.3f, -0.25f, 0.f)), transformDirection(leftUpperLegTransform, vec3(0.f, -1.f, 0.f)), -1.f, deg2rad(30.f));
	leftKneeConstraint = addHingeJointConstraintFromGlobalPoints(leftUpperLeg, leftLowerLeg, transformPosition(leftUpperLegTransform, scale * vec3(0.f, -0.6f, 0.f)), vec3(1.f, 0.f, 0.f), deg2rad(-90.f), deg2rad(5.f));
	leftAnkleConstraint = addConeTwistConstraintFromGlobalPoints(leftLowerLeg, leftFoot, transformPosition(leftLowerLegTransform, scale * vec3(0.f, -0.52f, 0.f)), transformDirection(leftLowerLegTransform, vec3(0.f, -1.f, 0.f)), deg2rad(75.f), deg2rad(20.f));
	leftToesConstraint = addHingeJointConstraintFromGlobalPoints(leftFoot, leftToes, transformPosition(leftFootTransform, scale * vec3(0.f, 0.f, -0.36f)), vec3(1.f, 0.f, 0.f), deg2rad(-45.f), deg2rad(45.f));

	rightHipConstraint = addConeTwistConstraintFromGlobalPoints(torso, rightUpperLeg, transformPosition(torsoTransform, scale * vec3(0.3f, -0.25f, 0.f)), transformDirection(rightUpperLegTransform, vec3(0.f, -1.f, 0.f)), -1.f, deg2rad(30.f));
	rightKneeConstraint = addHingeJointConstraintFromGlobalPoints(rightUpperLeg, rightLowerLeg, transformPosition(rightUpperLegTransform, scale * vec3(0.f, -0.6f, 0.f)), vec3(1.f, 0.f, 0.f), deg2rad(-90.f), deg2rad(5.f));
	rightAnkleConstraint = addConeTwistConstraintFromGlobalPoints(rightLowerLeg, rightFoot, transformPosition(rightLowerLegTransform, scale * vec3(0.f, -0.52f, 0.f)), transformDirection(rightLowerLegTransform, vec3(0.f, -1.f, 0.f)), deg2rad(75.f), deg2rad(20.f));
	rightToesConstraint = addHingeJointConstraintFromGlobalPoints(rightFoot, rightToes, transformPosition(rightFootTransform, scale * vec3(0.f, 0.f, -0.36f)), vec3(1.f, 0.f, 0.f), deg2rad(-45.f), deg2rad(45.f));

#if 0
	float totalMass = 1.f / torso.getComponent<rigid_body_component>().invMass +
		1.f / head.getComponent<rigid_body_component>().invMass +
		1.f / leftUpperArm.getComponent<rigid_body_component>().invMass +
		1.f / leftLowerArm.getComponent<rigid_body_component>().invMass +
		1.f / rightUpperArm.getComponent<rigid_body_component>().invMass +
		1.f / rightLowerArm.getComponent<rigid_body_component>().invMass +
		1.f / leftUpperLeg.getComponent<rigid_body_component>().invMass +
		1.f / leftLowerLeg.getComponent<rigid_body_component>().invMass +
		1.f / leftFoot.getComponent<rigid_body_component>().invMass +
		1.f / leftToes.getComponent<rigid_body_component>().invMass +
		1.f / rightUpperLeg.getComponent<rigid_body_component>().invMass +
		1.f / rightLowerLeg.getComponent<rigid_body_component>().invMass +
		1.f / rightFoot.getComponent<rigid_body_component>().invMass +
		1.f / rightToes.getComponent<rigid_body_component>().invMass;

	std::cout << totalMass << '\n';
#endif


	torsoParent = {};
	headParent = torso;
	leftUpperArmParent = torso;
	leftLowerArmParent = leftUpperArm;
	rightUpperArmParent = torso;
	rightLowerArmParent = rightUpperArm;
	leftUpperLegParent = torso;
	leftLowerLegParent = leftUpperLeg;
	leftFootParent = leftLowerLeg;
	leftToesParent = leftFoot;
	rightUpperLegParent = torso;
	rightLowerLegParent = rightUpperLeg;
	rightFootParent = rightLowerLeg;
	rightToesParent = rightFoot;


	// Graphics.


#ifndef PHYSICS_ONLY

	cpu_mesh primitiveMesh(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents);

	auto ragdollMaterial = createPBRMaterial(
		{}, {}, {}, {}, vec4(0.f), vec4(161.f, 102.f, 94.f, 255.f) / 255.f, 1.f, 0.f);
	//auto ragdollMaterial = lollipopMaterial;

	auto ragdollTorsoMesh = make_ref<composite_mesh>();
	ragdollTorsoMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, scale * 0.4f,  scale * 0.25f, scale * vec3(0.f), vec3(1.f, 0.f, 0.f)), {}, trs::identity, ragdollMaterial });
	ragdollTorsoMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, scale * 0.32f, scale * 0.2f, scale * vec3(0.f, 0.32f, 0.f), vec3(1.f, 0.f, 0.f)), {}, trs::identity, ragdollMaterial });
	ragdollTorsoMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, scale * 0.28f, scale * 0.22f, scale * vec3(0.f, 0.62f, 0.f), vec3(1.f, 0.f, 0.f)), {}, trs::identity, ragdollMaterial });
	ragdollTorsoMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, scale * 0.28f, scale * 0.2f, scale * vec3(0.f, 0.92f, 0.f), vec3(1.f, 0.f, 0.f)), {}, trs::identity, ragdollMaterial });

	auto ragdollHeadMesh = make_ref<composite_mesh>();
	ragdollHeadMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, scale * 0.15f, scale * 0.25f, scale * vec3(0.f)), {}, trs::identity, ragdollMaterial });

	auto ragdollArmMesh = make_ref<composite_mesh>();
	ragdollArmMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, scale * 0.4f, scale * 0.15f, scale * vec3(0.f)), {}, trs::identity, ragdollMaterial });

	auto ragdollUpperLegMesh = make_ref<composite_mesh>();
	ragdollUpperLegMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, scale * 0.6f, scale * 0.25f, scale * vec3(0.f)), {}, trs::identity, ragdollMaterial });

	auto ragdollLowerLegMesh = make_ref<composite_mesh>();
	ragdollLowerLegMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, scale * 0.6f, scale * 0.18f, scale * vec3(0.f)), {}, trs::identity, ragdollMaterial });

	auto ragdollFootMesh = make_ref<composite_mesh>();
	ragdollFootMesh->submeshes.push_back({ primitiveMesh.pushCube(scale * vec3(0.1587f, 0.1f, 0.3424f)), {}, trs::identity, ragdollMaterial });

	auto ragdollToesMesh = make_ref<composite_mesh>();
	ragdollToesMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, scale * 0.1174f,  scale * 0.1f, scale * vec3(0.f), vec3(1.f, 0.f, 0.f)), {}, trs::identity, ragdollMaterial });

	torso.addComponent<raster_component>(ragdollTorsoMesh);
	head.addComponent<raster_component>(ragdollHeadMesh);
	leftUpperArm.addComponent<raster_component>(ragdollArmMesh);
	leftLowerArm.addComponent<raster_component>(ragdollArmMesh);
	rightUpperArm.addComponent<raster_component>(ragdollArmMesh);
	rightLowerArm.addComponent<raster_component>(ragdollArmMesh);
	leftUpperLeg.addComponent<raster_component>(ragdollUpperLegMesh);
	leftLowerLeg.addComponent<raster_component>(ragdollLowerLegMesh);
	leftFoot.addComponent<raster_component>(ragdollFootMesh);
	leftToes.addComponent<raster_component>(ragdollToesMesh);
	rightUpperLeg.addComponent<raster_component>(ragdollUpperLegMesh);
	rightLowerLeg.addComponent<raster_component>(ragdollLowerLegMesh);
	rightFoot.addComponent<raster_component>(ragdollFootMesh);
	rightToes.addComponent<raster_component>(ragdollToesMesh);

	ragdollTorsoMesh->mesh =
		ragdollHeadMesh->mesh =
		ragdollArmMesh->mesh =
		ragdollUpperLegMesh->mesh =
		ragdollLowerLegMesh->mesh =
		ragdollFootMesh->mesh =
		ragdollToesMesh->mesh =
		primitiveMesh.createDXMesh();

#endif
}

#ifndef PHYSICS_ONLY
static bool editHingeConstraint(const char* label, hinge_joint_constraint_handle handle)
{
	bool result = false;

	if (ImGui::TreeNode(label))
	{
		hinge_joint_constraint& con = getConstraint(handle);

		bool minLimitActive = con.minRotationLimit <= 0.f;
		if (ImGui::Checkbox("Lower limit active", &minLimitActive))
		{
			result = true;
			con.minRotationLimit = -con.minRotationLimit;
		}
		if (minLimitActive)
		{
			float minLimit = -con.minRotationLimit;
			result |= ImGui::SliderAngle("Lower limit", &minLimit, 0.f, 180.f, "-%.0f deg");
			con.minRotationLimit = -minLimit;
		}

		bool maxLimitActive = con.maxRotationLimit >= 0.f;
		if (ImGui::Checkbox("Upper limit active", &maxLimitActive))
		{
			result = true;
			con.maxRotationLimit = -con.maxRotationLimit;
		}
		if (maxLimitActive)
		{
			result |= ImGui::SliderAngle("Upper limit", &con.maxRotationLimit, 0.f, 180.f);
		}

		bool motorActive = con.maxMotorTorque > 0.f;
		if (ImGui::Checkbox("Motor active", &motorActive))
		{
			result = true;
			con.maxMotorTorque = -con.maxMotorTorque;
		}
		if (motorActive)
		{
			result |= ImGui::Dropdown("Motor type", constraintMotorTypeNames, arraysize(constraintMotorTypeNames), (uint32&)con.motorType);

			if (con.motorType == constraint_velocity_motor)
			{
				result |= ImGui::SliderAngle("Motor velocity", &con.motorVelocity, -360.f, 360.f);
			}
			else
			{
				float lo = minLimitActive ? con.minRotationLimit : -M_PI;
				float hi = maxLimitActive ? con.maxRotationLimit : M_PI;
				result |= ImGui::SliderAngle("Motor target angle", &con.motorTargetAngle, rad2deg(lo), rad2deg(hi));
			}

			result |= ImGui::SliderFloat("Max motor torque", &con.maxMotorTorque, 0.001f, 1000.f);
		}

		ImGui::TreePop();
	}

	return result;
}

static bool editConeTwistConstraint(const char* label, cone_twist_constraint_handle handle)
{
	bool result = false;

	if (ImGui::TreeNode(label))
	{
		cone_twist_constraint& con = getConstraint(handle);

		bool swingLimitActive = con.swingLimit >= 0.f;
		if (ImGui::Checkbox("Swing limit active", &swingLimitActive))
		{
			result = true;
			con.swingLimit = -con.swingLimit;
		}
		if (swingLimitActive)
		{
			result |= ImGui::SliderAngle("Swing limit", &con.swingLimit, 0.f, 180.f);
		}

		bool twistLimitActive = con.twistLimit >= 0.f;
		if (ImGui::Checkbox("Twist limit active", &twistLimitActive))
		{
			result = true;
			con.twistLimit = -con.twistLimit;
		}
		if (twistLimitActive)
		{
			result |= ImGui::SliderAngle("Twist limit", &con.twistLimit, 0.f, 180.f);
		}

		bool twistMotorActive = con.maxTwistMotorTorque > 0.f;
		if (ImGui::Checkbox("Twist motor active", &twistMotorActive))
		{
			result = true;
			con.maxTwistMotorTorque = -con.maxTwistMotorTorque;
		}
		if (twistMotorActive)
		{
			result |= ImGui::Dropdown("Twist motor type", constraintMotorTypeNames, arraysize(constraintMotorTypeNames), (uint32&)con.twistMotorType);

			if (con.twistMotorType == constraint_velocity_motor)
			{
				result |= ImGui::SliderAngle("Twist motor velocity", &con.twistMotorVelocity, -360.f, 360.f);
			}
			else
			{
				float li = twistLimitActive ? con.twistLimit : -M_PI;
				result |= ImGui::SliderAngle("Twist motor target angle", &con.twistMotorTargetAngle, rad2deg(-li), rad2deg(li));
			}

			result |= ImGui::SliderFloat("Max twist motor torque", &con.maxTwistMotorTorque, 0.001f, 1000.f);
		}

		bool swingMotorActive = con.maxSwingMotorTorque > 0.f;
		if (ImGui::Checkbox("Swing motor active", &swingMotorActive))
		{
			result = true;
			con.maxSwingMotorTorque = -con.maxSwingMotorTorque;
		}
		if (swingMotorActive)
		{
			result |= ImGui::Dropdown("Swing motor type", constraintMotorTypeNames, arraysize(constraintMotorTypeNames), (uint32&)con.swingMotorType);

			if (con.swingMotorType == constraint_velocity_motor)
			{
				result |= ImGui::SliderAngle("Swing motor velocity", &con.swingMotorVelocity, -360.f, 360.f);
			}
			else
			{
				float li = swingLimitActive ? con.swingLimit : -M_PI;
				result |= ImGui::SliderAngle("Swing motor target angle", &con.swingMotorTargetAngle, rad2deg(-li), rad2deg(li));
			}

			result |= ImGui::SliderAngle("Swing motor axis angle", &con.swingMotorAxis, -180.f, 180.f);
			result |= ImGui::SliderFloat("Max swing motor torque", &con.maxSwingMotorTorque, 0.001f, 1000.f);
		}
		
		ImGui::TreePop();
	}

	return result;
}


bool humanoid_ragdoll::edit()
{
	bool result = false;
	if (ImGui::TreeNode("Ragdoll"))
	{
		result |= editConeTwistConstraint("Neck", neckConstraint);
		result |= editConeTwistConstraint("Left shoulder", leftShoulderConstraint);
		result |= editHingeConstraint("Left Elbow", leftElbowConstraint);
		result |= editConeTwistConstraint("Right shoulder", rightShoulderConstraint);
		result |= editHingeConstraint("Right Elbow", rightElbowConstraint);

		result |= editConeTwistConstraint("Left hip", leftHipConstraint);
		result |= editHingeConstraint("Left knee", leftKneeConstraint);
		result |= editConeTwistConstraint("Left ankle", leftAnkleConstraint);
		result |= editHingeConstraint("Left toes", leftToesConstraint);

		result |= editConeTwistConstraint("Right hip", rightHipConstraint);
		result |= editHingeConstraint("Right knee", rightKneeConstraint);
		result |= editConeTwistConstraint("Right ankle", rightAnkleConstraint);
		result |= editHingeConstraint("Right toes", rightToesConstraint);

		ImGui::TreePop();
	}
	return result;
}

#else
bool humanoid_ragdoll::edit()
{
	return false;
}
#endif
