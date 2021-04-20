#include "pch.h"
#include "ragdoll.h"
#include "pbr.h"
#include "mesh.h"
#include "geometry.h"
#include "scene.h"
#include "imgui.h"

void humanoid_ragdoll::initialize(scene& appScene)
{
	cpu_mesh primitiveMesh(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents);

	auto ragdollMaterial = createPBRMaterial(
		{}, {}, {}, {}, vec4(0.f), vec4(161.f, 102.f, 94.f, 255.f) / 255.f, 1.f, 0.f);
	//auto ragdollMaterial = lollipopMaterial;

	auto ragdollTorsoMesh = make_ref<composite_mesh>();
	ragdollTorsoMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, 0.4f, 0.25f, vec3(0.f), vec3(1.f, 0.f, 0.f)), {}, trs::identity, ragdollMaterial });
	ragdollTorsoMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, 0.32f, 0.2f, vec3(0.f, 0.32f, 0.f), vec3(1.f, 0.f, 0.f)), {}, trs::identity, ragdollMaterial });
	ragdollTorsoMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, 0.28f, 0.22f, vec3(0.f, 0.62f, 0.f), vec3(1.f, 0.f, 0.f)), {}, trs::identity, ragdollMaterial });
	ragdollTorsoMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, 0.28f, 0.2f, vec3(0.f, 0.92f, 0.f), vec3(1.f, 0.f, 0.f)), {}, trs::identity, ragdollMaterial });

	auto ragdollHeadMesh = make_ref<composite_mesh>();
	ragdollHeadMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, 0.15f, 0.25f, vec3(0.f)), {}, trs::identity, ragdollMaterial });

	auto ragdollArmMesh = make_ref<composite_mesh>();
	ragdollArmMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, 0.4f, 0.15f, vec3(0.f)), {}, trs::identity, ragdollMaterial });

	auto ragdollUpperLegMesh = make_ref<composite_mesh>();
	ragdollUpperLegMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, 0.6f, 0.25f, vec3(0.f)), {}, trs::identity, ragdollMaterial });

	auto ragdollLowerLegMesh = make_ref<composite_mesh>();
	ragdollLowerLegMesh->submeshes.push_back({ primitiveMesh.pushCapsule(15, 15, 0.6f, 0.18f, vec3(0.f)), {}, trs::identity, ragdollMaterial });

	bool ragdollKinematic = false;
	float ragdollGravityFactor = 1.f;
	float ragdollDensity = 998.f;
	float ragdollFriction = 0.9f;

	trs torsoTransform(vec3(30.f, 5.f, -2.f), quat::identity);
	auto torso = appScene.createEntity("Torso")
		.addComponent<trs>(torsoTransform)
		.addComponent<raster_component>(ragdollTorsoMesh)
		.addComponent<collider_component>(bounding_capsule{ vec3(-0.2f, 0.f, 0.f), vec3(0.2f, 0.f, 0.f), 0.25f }, 0.2f, 0.5f, ragdollDensity)
		.addComponent<collider_component>(bounding_capsule{ vec3(-0.16f, 0.32f, 0.f), vec3(0.16f, 0.32f, 0.f), 0.2f }, 0.2f, 0.5f, ragdollDensity)
		.addComponent<collider_component>(bounding_capsule{ vec3(-0.14f, 0.62f, 0.f), vec3(0.14f, 0.62f, 0.f), 0.22f }, 0.2f, 0.5f, ragdollDensity)
		.addComponent<collider_component>(bounding_capsule{ vec3(-0.14f, 0.92f, 0.f), vec3(0.14f, 0.92f, 0.f), 0.2f }, 0.2f, 0.5f, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	auto head = appScene.createEntity("Head")
		.addComponent<trs>(vec3(30.f, 6.45f, -2.f), quat::identity)
		.addComponent<raster_component>(ragdollHeadMesh)
		.addComponent<collider_component>(bounding_capsule{ vec3(0.f, -0.075f, 0.f), vec3(0.f, 0.075f, 0.f), 0.25f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	trs leftUpperArmTransform(vec3(29.4f, 5.75f, -2.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(-30.f)));
	auto leftUpperArm = appScene.createEntity("Left upper arm")
		.addComponent<trs>(leftUpperArmTransform)
		.addComponent<raster_component>(ragdollArmMesh)
		.addComponent<collider_component>(bounding_capsule{ vec3(0.f, -0.2f, 0.f), vec3(0.f, 0.2f, 0.f), 0.15f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	auto leftLowerArm = appScene.createEntity("Left lower arm")
		.addComponent<trs>(vec3(29.116f, 5.044f, -2.043f), quat(vec3(0.f, 0.f, 1.f), deg2rad(-20.f)))
		.addComponent<raster_component>(ragdollArmMesh)
		.addComponent<collider_component>(bounding_capsule{ vec3(0.f, -0.2f, 0.f), vec3(0.f, 0.2f, 0.f), 0.15f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	trs rightUpperArmTransform(vec3(30.6f, 5.75f, -2.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(30.f)));
	auto rightUpperArm = appScene.createEntity("Right upper arm")
		.addComponent<trs>(vec3(30.6f, 5.75f, -2.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(30.f)))
		.addComponent<raster_component>(ragdollArmMesh)
		.addComponent<collider_component>(bounding_capsule{ vec3(0.f, -0.2f, 0.f), vec3(0.f, 0.2f, 0.f), 0.15f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	auto rightLowerArm = appScene.createEntity("Right lower arm")
		.addComponent<trs>(vec3(30.884f, 5.044f, -2.043f), quat(vec3(0.f, 0.f, 1.f), deg2rad(20.f)))
		.addComponent<raster_component>(ragdollArmMesh)
		.addComponent<collider_component>(bounding_capsule{ vec3(0.f, -0.2f, 0.f), vec3(0.f, 0.2f, 0.f), 0.15f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	trs leftUpperLegTransform(vec3(29.629f, 4.188f, -2.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(-10.f)));
	auto leftUpperLeg = appScene.createEntity("Left upper leg")
		.addComponent<trs>(leftUpperLegTransform)
		.addComponent<raster_component>(ragdollUpperLegMesh)
		.addComponent<collider_component>(bounding_capsule{ vec3(0.f, -0.3f, 0.f), vec3(0.f, 0.3f, 0.f), 0.25f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	auto leftLowerLeg = appScene.createEntity("Left lower leg")
		.addComponent<trs>(vec3(29.548f, 3.045f, -2.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(-3.5f)))
		.addComponent<raster_component>(ragdollLowerLegMesh)
		.addComponent<collider_component>(bounding_capsule{ vec3(0.f, -0.3f, 0.f), vec3(0.f, 0.3f, 0.f), 0.18f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	trs rightUpperLegTransform(vec3(30.371f, 4.188f, -2.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(10.f)));
	auto rightUpperLeg = appScene.createEntity("Right upper leg")
		.addComponent<trs>(rightUpperLegTransform)
		.addComponent<raster_component>(ragdollUpperLegMesh)
		.addComponent<collider_component>(bounding_capsule{ vec3(0.f, -0.3f, 0.f), vec3(0.f, 0.3f, 0.f), 0.25f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);

	auto rightLowerLeg = appScene.createEntity("Right lower leg")
		.addComponent<trs>(vec3(30.452f, 3.045f, -2.f), quat(vec3(0.f, 0.f, 1.f), deg2rad(3.5f)))
		.addComponent<raster_component>(ragdollLowerLegMesh)
		.addComponent<collider_component>(bounding_capsule{ vec3(0.f, -0.3f, 0.f), vec3(0.f, 0.3f, 0.f), 0.18f }, 0.2f, ragdollFriction, ragdollDensity)
		.addComponent<rigid_body_component>(ragdollKinematic, ragdollGravityFactor);


	neckConstraint = addConeTwistConstraintFromGlobalPoints(torso, head, transformPosition(torsoTransform, vec3(0.f, 1.2f, 0.f)), vec3(0.f, 1.f, 0.f), deg2rad(50.f), deg2rad(90.f));
	leftShoulderConstraint = addConeTwistConstraintFromGlobalPoints(torso, leftUpperArm, transformPosition(torsoTransform, vec3(-0.4f, 1.f, 0.f)), vec3(-1.f, 0.f, 0.f), deg2rad(130.f), deg2rad(90.f));
	leftElbowConstraint = addHingeJointConstraintFromGlobalPoints(leftUpperArm, leftLowerArm, transformPosition(leftUpperArmTransform, vec3(0.f, -0.42f, 0.f)), normalize(vec3(1.f, 0.f, 1.f)), deg2rad(-5.f), deg2rad(85.f));
	rightShoulderConstraint = addConeTwistConstraintFromGlobalPoints(torso, rightUpperArm, transformPosition(torsoTransform, vec3(0.4f, 1.f, 0.f)), vec3(1.f, 0.f, 0.f), deg2rad(130.f), deg2rad(90.f));
	rightElbowConstraint = addHingeJointConstraintFromGlobalPoints(rightUpperArm, rightLowerArm, transformPosition(rightUpperArmTransform, vec3(0.f, -0.42f, 0.f)), normalize(vec3(1.f, 0.f, -1.f)), deg2rad(-5.f), deg2rad(85.f));
	leftHipConstraint = addConeTwistConstraintFromGlobalPoints(torso, leftUpperLeg, transformPosition(torsoTransform, vec3(-0.3f, -0.25f, 0.f)), transformDirection(leftUpperLegTransform, vec3(0.f, -1.f, 0.f)), -1.f, deg2rad(30.f));
	leftKneeConstraint = addHingeJointConstraintFromGlobalPoints(leftUpperLeg, leftLowerLeg, transformPosition(leftUpperLegTransform, vec3(0.f, -0.6f, 0.f)), vec3(1.f, 0.f, 0.f), deg2rad(-90.f), deg2rad(5.f));
	rightHipConstraint = addConeTwistConstraintFromGlobalPoints(torso, rightUpperLeg, transformPosition(torsoTransform, vec3(0.3f, -0.25f, 0.f)), transformDirection(rightUpperLegTransform, vec3(0.f, -1.f, 0.f)), -1.f, deg2rad(30.f));
	rightKneeConstraint = addHingeJointConstraintFromGlobalPoints(rightUpperLeg, rightLowerLeg, transformPosition(rightUpperLegTransform, vec3(0.f, -0.6f, 0.f)), vec3(1.f, 0.f, 0.f), deg2rad(-90.f), deg2rad(5.f));


	ragdollTorsoMesh->mesh =
		ragdollHeadMesh->mesh =
		ragdollArmMesh->mesh =
		ragdollUpperLegMesh->mesh =
		ragdollLowerLegMesh->mesh =
		primitiveMesh.createDXMesh();
}

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

		bool coneLimitActive = con.coneLimit >= 0.f;
		if (ImGui::Checkbox("Cone limit active", &coneLimitActive))
		{
			result = true;
			con.coneLimit = -con.coneLimit;
		}
		if (coneLimitActive)
		{
			result |= ImGui::SliderAngle("Cone limit", &con.coneLimit, 0.f, 180.f);
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
		result |= editConeTwistConstraint("Right hip", rightHipConstraint);
		result |= editHingeConstraint("Right knee", rightKneeConstraint);

		ImGui::TreePop();
	}
	return result;
}
