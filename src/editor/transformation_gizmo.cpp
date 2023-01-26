#include "pch.h"
#include "transformation_gizmo.h"
#include "dx/dx_command_list.h"
#include "dx/dx_pipeline.h"
#include "geometry/mesh_builder.h"
#include "physics/bounding_volumes.h"
#include "rendering/render_utils.h"
#include "rendering/debug_visualization.h"
#include "core/imgui.h"


enum gizmo_axis
{
	gizmo_axis_x,
	gizmo_axis_y,
	gizmo_axis_z,
};

static union
{
	struct
	{
		submesh_info translationSubmesh;
		submesh_info rotationSubmesh;
		submesh_info scaleSubmesh;
		submesh_info planeSubmesh;
		submesh_info boxSubmesh;
	};

	submesh_info submeshes[5];
};

static bounding_cylinder cylinders[3] =
{
	bounding_cylinder{ vec3(0.f), vec3(1.f, 0.f, 0.f), 1.f },
	bounding_cylinder{ vec3(0.f), vec3(0.f, 1.f, 0.f), 1.f },
	bounding_cylinder{ vec3(0.f), vec3(0.f, 0.f, 1.f), 1.f },
};

static bounding_torus tori[3] =
{
	bounding_torus{ vec3(0.f), vec3(1.f, 0.f, 0.f), 1.f, 1.f },
	bounding_torus{ vec3(0.f), vec3(0.f, 1.f, 0.f), 1.f, 1.f },
	bounding_torus{ vec3(0.f), vec3(0.f, 0.f, 1.f), 1.f, 1.f },
};

struct gizmo_rectangle
{
	vec3 position;
	vec3 tangent;
	vec3 bitangent;
	vec2 radius;
};

static gizmo_rectangle rectangles[] =
{
	gizmo_rectangle{ vec3(1.f, 0.f, 1.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 0.f, 1.f), vec2(1.f) },
	gizmo_rectangle{ vec3(1.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), vec2(1.f) },
	gizmo_rectangle{ vec3(0.f, 1.f, 1.f), vec3(0.f, 1.f, 0.f), vec3(0.f, 0.f, 1.f), vec2(1.f) },
};

static dx_mesh mesh;


void initializeTransformationGizmos()
{
	mesh_builder mesh(mesh_creation_flags_with_positions | mesh_creation_flags_with_normals);
	float shaftLength = 1.f;
	float headLength = 0.2f;
	float radius = 0.03f;
	float headRadius = 0.065f;

	arrow_mesh_desc arrowMesh;
	arrowMesh.slices = 6;
	arrowMesh.shaftRadius = radius;
	arrowMesh.headRadius = headRadius;
	arrowMesh.shaftLength = shaftLength;
	arrowMesh.headLength = headLength;
	mesh.pushArrow(arrowMesh);
	translationSubmesh = mesh.endSubmesh();

	torus_mesh_desc torusMesh;
	torusMesh.slices = 6;
	torusMesh.segments = 64;
	torusMesh.torusRadius = shaftLength;
	torusMesh.tubeRadius = radius;
	mesh.pushTorus(torusMesh);
	rotationSubmesh = mesh.endSubmesh();

	mace_mesh_desc maceMesh;
	maceMesh.slices = 6;
	maceMesh.shaftRadius = radius;
	maceMesh.headRadius = headRadius;
	maceMesh.shaftLength = shaftLength;
	maceMesh.headLength = headLength;
	mesh.pushMace(maceMesh);
	scaleSubmesh = mesh.endSubmesh();

	box_mesh_desc planeMesh;
	planeMesh.radius = vec3(shaftLength, 0.01f, shaftLength) * 0.2f;
	planeMesh.center = vec3(shaftLength, 0.f, shaftLength) * 0.35f;
	mesh.pushBox(planeMesh);
	planeSubmesh = mesh.endSubmesh();

	box_mesh_desc boxMesh;
	boxMesh.radius = (shaftLength + headLength) * 0.3f;
	mesh.pushBox(boxMesh);
	boxSubmesh = mesh.endSubmesh();

	::mesh = mesh.createDXMesh();

	for (uint32 i = 0; i < 3; ++i)
	{
		cylinders[i].positionB *= shaftLength + headLength;
		cylinders[i].radius *= radius * 1.1f;

		tori[i].majorRadius *= shaftLength;
		tori[i].tubeRadius *= radius * 1.1f;

		rectangles[i].position *= shaftLength * 0.35f;
		rectangles[i].radius *= shaftLength * 0.2f;
	}
}


uint32 transformation_gizmo::handleTranslation(trs& transform, ray r, const user_input& input, float snapping, float scaling)
{
	quat rot = (space == transformation_global) ? quat::identity : transform.rotation;

	uint32 hoverAxisIndex = -1;
	float minT = FLT_MAX;
	for (uint32 i = 0; i < 3; ++i)
	{
		float t;
		bounding_cylinder cylinder = { rot * cylinders[i].positionA * scaling + transform.position, rot * cylinders[i].positionB * scaling + transform.position, cylinders[i].radius * scaling };
		if (r.intersectCylinder(cylinder, t) && t < minT)
		{
			hoverAxisIndex = i;
			minT = t;
		}

		if (r.intersectRectangle(rot * rectangles[i].position * scaling + transform.position, rot * rectangles[i].tangent, rot * rectangles[i].bitangent, rectangles[i].radius * scaling, t) && t < minT)
		{
			hoverAxisIndex = i + 3;
			minT = t;
		}
	}

	if (input.mouse.left.clickEvent && hoverAxisIndex != -1)
	{
		dragging = true;
		axisIndex = hoverAxisIndex;

		if (hoverAxisIndex < 3)
		{
			vec3 candidate0(0.f); candidate0.data[(axisIndex + 1) % 3] = 1.f;
			vec3 candidate1(0.f); candidate1.data[(axisIndex + 2) % 3] = 1.f;

			const vec4 axisPlanes[] =
			{
				createPlane(transform.position, rot * candidate0),
				createPlane(transform.position, rot * candidate1),
			};

			float a = abs(dot(r.direction, axisPlanes[0].xyz));
			float b = abs(dot(r.direction, axisPlanes[1].xyz));

			plane = (a > b) ? axisPlanes[0] : axisPlanes[1];

			vec3 axis(0.f); axis.data[axisIndex] = 1.f;
			axis = rot * axis;

			float t;
			r.intersectPlane(plane.xyz, plane.w, t);
			anchor = dot(r.origin + t * r.direction - transform.position, axis);
		}
		else
		{
			plane = createPlane(rot * rectangles[axisIndex - 3].position + transform.position, rot * cross(rectangles[axisIndex - 3].tangent, rectangles[axisIndex - 3].bitangent));
			anchor3 = r.origin + minT * r.direction - transform.position;
		}
		originalPosition = transform.position;

		originalTransform = transform;
	}

	if (dragging)
	{
		float t;
		r.intersectPlane(plane.xyz, plane.w, t);

		if (axisIndex < 3)
		{
			vec3 axis(0.f); axis.data[axisIndex] = 1.f;
			axis = rot * axis;

			float amount = (dot(r.origin + t * r.direction - originalPosition, axis) - anchor);

			if (snapping > 0.f)
			{
				amount = round(amount / snapping) * snapping;
			}

			transform.position = originalPosition + amount * axis;
		}
		else
		{
			vec3 amount = r.origin + t * r.direction - originalPosition - anchor3;

			if (snapping > 0.f)
			{
				vec3 tangent = rot * rectangles[axisIndex - 3].tangent;
				vec3 bitangent = rot * rectangles[axisIndex - 3].bitangent;

				float tangentAmount = round(dot(tangent, amount) / snapping) * snapping;
				float bitangentAmount = round(dot(bitangent, amount) / snapping)* snapping;

				amount = tangent * tangentAmount + bitangent * bitangentAmount;
			}

			transform.position = originalPosition + amount;
		}
	}

	return dragging ? axisIndex : hoverAxisIndex;
}

uint32 transformation_gizmo::handleRotation(trs& transform, ray r, const user_input& input, float snapping, float scaling)
{
	quat rot = (space == transformation_global) ? quat::identity : transform.rotation;

	uint32 hoverAxisIndex = -1;
	float minT = FLT_MAX;
	for (uint32 i = 0; i < 3; ++i)
	{
		float t;
		bounding_torus torus = { rot * tori[i].position * scaling + transform.position, rot * tori[i].upAxis, tori[i].majorRadius * scaling, tori[i].tubeRadius * scaling };
		if (r.intersectTorus(torus, t) && t < minT)
		{
			hoverAxisIndex = i;
			minT = t;
		}
	}

	if (input.mouse.left.clickEvent && hoverAxisIndex != -1)
	{
		dragging = true;
		axisIndex = hoverAxisIndex;

		vec3 planeNormal(0.f); planeNormal.data[axisIndex] = 1.f;
		planeNormal = rot * planeNormal;
		plane = createPlane(transform.position, planeNormal);

		float t;
		r.intersectPlane(plane.xyz, plane.w, t);

		vec3 p = r.origin + t * r.direction - transform.position;
		p = conjugate(rot) * p;
		float x = p.data[(axisIndex + 1) % 3];
		float y = p.data[(axisIndex + 2) % 3];

		anchor = atan2(y, x);
		originalRotation = quat(planeNormal, anchor);

		originalTransform = transform;
	}

	if (dragging)
	{
		float t;
		r.intersectPlane(plane.xyz, plane.w, t);

		vec3 p = r.origin + t * r.direction - transform.position;
		p = conjugate(rot) * p;
		float x = p.data[(axisIndex + 1) % 3];
		float y = p.data[(axisIndex + 2) % 3];

		float angle = atan2(y, x);

		if (snapping > 0.f)
		{
			angle = round((angle - anchor) / snapping) * snapping + anchor;
		}

		quat currentRotation(plane.xyz, angle);
		quat deltaRotation = currentRotation * conjugate(originalRotation);
		transform.rotation = normalize(deltaRotation * transform.rotation);
		if (space == transformation_global)
		{
			originalRotation = currentRotation;
		}
	}

	return dragging ? axisIndex : hoverAxisIndex;
}

uint32 transformation_gizmo::handleScaling(trs& transform, ray r, const user_input& input, float scaling, const render_camera& camera, bool allowNonUniform)
{
	// We only allow scaling in local space.
	quat rot = transform.rotation;

	const float cylinderHeight = cylinders[0].positionB.x;
	const float uniformRadius = cylinderHeight * 0.3f * scaling;

	bounding_box uniformBox = bounding_box::fromCenterRadius(vec3(0.f), uniformRadius);

	ray localRay = r;
	localRay.origin = inverseTransformPosition(transform, r.origin) * transform.scale; // inverseTransformPosition applies scale, but we don't want this.
	localRay.direction = inverseTransformDirection(transform, r.direction);

	uint32 hoverAxisIndex = -1;
	float minT = FLT_MAX;
	if (localRay.intersectAABB(uniformBox, minT))
	{
		hoverAxisIndex = 3;
	}
	else if (allowNonUniform)
	{
		float minT = FLT_MAX;
		for (uint32 i = 0; i < 3; ++i)
		{
			float t;
			bounding_cylinder cylinder = { rot * cylinders[i].positionA * scaling + transform.position, rot * cylinders[i].positionB * scaling + transform.position, cylinders[i].radius * scaling };
			if (r.intersectCylinder(cylinder, t) && t < minT)
			{
				hoverAxisIndex = i;
				minT = t;
			}
		}
	}


	if (input.mouse.left.clickEvent && hoverAxisIndex != -1)
	{
		if (hoverAxisIndex < 3)
		{
			// Non-uniform scaling.
			vec3 candidate0(0.f); candidate0.data[(hoverAxisIndex + 1) % 3] = 1.f;
			vec3 candidate1(0.f); candidate1.data[(hoverAxisIndex + 2) % 3] = 1.f;

			const vec4 axisPlanes[] =
			{
				createPlane(transform.position, rot * candidate0),
				createPlane(transform.position, rot * candidate1),
			};

			dragging = true;
			axisIndex = hoverAxisIndex;

			float a = abs(dot(r.direction, axisPlanes[0].xyz));
			float b = abs(dot(r.direction, axisPlanes[1].xyz));

			plane = (a > b) ? axisPlanes[0] : axisPlanes[1];

			vec3 axis(0.f); axis.data[axisIndex] = 1.f;
			axis = rot * axis;

			float t;
			r.intersectPlane(plane.xyz, plane.w, t);
			anchor = dot(r.origin + t * r.direction - transform.position, axis);
			anchor = max(anchor, 0.0001f);
			originalScale = transform.scale;

			originalTransform = transform;
		}
		else
		{
			// Uniform scaling.
			assert(hoverAxisIndex == 3);

			dragging = true;
			axisIndex = hoverAxisIndex;

			plane = createPlane(transform.position, camera.rotation * vec3(0.f, 0.f, 1.f)); // Backward axis.

			float t;
			r.intersectPlane(plane.xyz, plane.w, t);
			anchor = dot(r.origin + t * r.direction - transform.position, camera.rotation * vec3(1.f, 1.f, 0.f));
			anchor = max(anchor, 0.0001f);
			originalScale = transform.scale;
		}
	}

	if (dragging)
	{
		if (axisIndex < 3)
		{
			float t;
			r.intersectPlane(plane.xyz, plane.w, t);

			vec3 axis(0.f); axis.data[axisIndex] = 1.f;
			axis = rot * axis;

			vec3 d = (dot(r.origin + t * r.direction - transform.position, axis) / anchor) * axis;

			d = conjugate(rot) * d;
			transform.scale.data[axisIndex] = originalScale.data[axisIndex] * d.data[axisIndex];
		}
		else
		{
			assert(axisIndex == 3);

			float t;
			r.intersectPlane(plane.xyz, plane.w, t);

			float d = (dot(r.origin + t * r.direction - transform.position, camera.rotation * vec3(1.f, 1.f, 0.f)) / anchor);
			
			transform.scale = originalScale * d;
		}
	}

	return dragging ? axisIndex : hoverAxisIndex;
}

bool transformation_gizmo::handleUserInput(bool allowKeyboardInput, 
	bool allowTranslation, bool allowRotation, bool allowScaling, bool allowSpaceChange)
{
	bool keyboardInteraction = false;
	bool uiInteraction = false;

	if (allowKeyboardInput && !ImGui::IsAnyItemActive() && !ImGui::AnyModifiersDown())
	{
		if (type != transformation_type_scale)
		{
			if (ImGui::IsKeyPressed('G') && allowSpaceChange)
			{
				space = (transformation_space)(1 - space);
				dragging = false;
				keyboardInteraction = true;
			}
		}
		if (ImGui::IsKeyPressed('Q'))
		{
			type = transformation_type_none;
			dragging = false;
			keyboardInteraction = true;
		}
		if (ImGui::IsKeyPressed('W') && allowTranslation)
		{
			type = transformation_type_translation;
			dragging = false;
			keyboardInteraction = true;
		}
		if (ImGui::IsKeyPressed('E') && allowRotation)
		{
			type = transformation_type_rotation;
			dragging = false;
			keyboardInteraction = true;
		}
		if (ImGui::IsKeyPressed('R') && allowScaling)
		{
			type = transformation_type_scale;
			dragging = false;
			keyboardInteraction = true;
		}
	}

	if (ImGui::BeginControlsWindow("##GizmoControls", ImVec2(0.f, 0.f), ImVec2(20.f, 20.f)))
	{
		transformation_space constantLocal = transformation_local;
		transformation_space& space = (type == transformation_type_scale) ? constantLocal : this->space;

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		bool allowGlobal = space == transformation_global || allowSpaceChange;
		bool allowLocal = space == transformation_local || allowSpaceChange;
		if (type == transformation_type_scale)
		{
			allowGlobal = false;
		}

		ImGui::PushID(&this->space);
		uiInteraction |= ImGui::IconRadioButton(imgui_icon_global, (int*)&space, transformation_global, IMGUI_ICON_DEFAULT_SIZE, allowGlobal);
		ImGui::SameLine(0.f, IMGUI_ICON_DEFAULT_SPACING);
		uiInteraction |= ImGui::IconRadioButton(imgui_icon_local, (int*)&space, transformation_local, IMGUI_ICON_DEFAULT_SIZE, allowLocal);
		ImGui::PopID();

		ImGui::SameLine(0.f, (float)IMGUI_ICON_DEFAULT_SIZE);


		ImGui::PushID(&type);
		uiInteraction |= ImGui::IconRadioButton(imgui_icon_cross, (int*)&type, transformation_type_none, IMGUI_ICON_DEFAULT_SIZE, true);
		ImGui::SameLine(0.f, IMGUI_ICON_DEFAULT_SPACING);
		uiInteraction |= ImGui::IconRadioButton(imgui_icon_translate, (int*)&type, transformation_type_translation, IMGUI_ICON_DEFAULT_SIZE, allowTranslation);
		ImGui::SameLine(0.f, IMGUI_ICON_DEFAULT_SPACING);
		uiInteraction |= ImGui::IconRadioButton(imgui_icon_rotate, (int*)&type, transformation_type_rotation, IMGUI_ICON_DEFAULT_SIZE, allowRotation);
		ImGui::SameLine(0.f, IMGUI_ICON_DEFAULT_SPACING);
		uiInteraction |= ImGui::IconRadioButton(imgui_icon_scale, (int*)&type, transformation_type_scale, IMGUI_ICON_DEFAULT_SIZE, allowScaling);
		ImGui::PopID();

		ImGui::SameLine(0.f, (float)IMGUI_ICON_DEFAULT_SIZE);

		ImGui::PopStyleColor();
	}

	ImGui::End();

	return keyboardInteraction || uiInteraction;
}

void transformation_gizmo::manipulateInternal(trs& transform, const render_camera& camera, const user_input& input, bool allowInput, ldr_render_pass* ldrRenderPass, bool allowNonUniformScale)
{
	if (!input.mouse.left.down || !allowInput)
	{
		dragging = false;
	}

	if (type == transformation_type_none)
	{
		return;
	}

	uint32 highlightAxis = -1;


	// Scale gizmos based on distance to camera.
	float scaling = length(transform.position - camera.position) / camera.getMinProjectionExtent() * 0.1f;

	if (allowInput)
	{
		float snapping = input.keyboard[key_ctrl].down ? (type == transformation_type_rotation ? deg2rad(45.f) : 0.5f) : 0.f;

		ray r = camera.generateWorldSpaceRay(input.mouse.relX, input.mouse.relY);

		switch (type)
		{
			case transformation_type_translation: highlightAxis = handleTranslation(transform, r, input, snapping, scaling); break;
			case transformation_type_rotation: highlightAxis = handleRotation(transform, r, input, snapping, scaling); break;
			case transformation_type_scale: highlightAxis = handleScaling(transform, r, input, scaling, camera, allowNonUniformScale); break; // TODO: Snapping for scale.
		}
	}


	// Render.

	quat rot = (space == transformation_global && type != transformation_type_scale) ? quat::identity : transform.rotation;

	{
		const quat rotations[] =
		{
			rot * quat(vec3(0.f, 0.f, -1.f), deg2rad(90.f)),
			rot,
			rot * quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f)),
		};

		const vec4 colors[] =
		{
			vec4(1.f, 0.f, 0.f, 1.f),
			vec4(0.f, 1.f, 0.f, 1.f),
			vec4(0.f, 0.f, 1.f, 1.f),
		};

		if (type != transformation_type_scale || allowNonUniformScale)
		{
			for (uint32 i = 0; i < 3; ++i)
			{
				debug_render_data data = {
					createModelMatrix(transform.position, rotations[i], scaling),
					mesh.vertexBuffer, mesh.indexBuffer,
					submeshes[type],
					colors[i] * (highlightAxis == i ? 0.5f : 1.f)
				};

				ldrRenderPass->renderOverlay<debug_simple_pipeline>(data);
			}
		}

		if (type == transformation_type_scale)
		{
			debug_render_data data = {
				createModelMatrix(transform.position, rot, scaling),
				mesh.vertexBuffer, mesh.indexBuffer,
				boxSubmesh,
				vec4(0.5f) * (highlightAxis == 3 ? 0.5f : 1.f)
			};

			ldrRenderPass->renderOverlay<debug_simple_pipeline>(data);
		}
	}

	if (type == transformation_type_translation)
	{
		const quat rotations[] =
		{
			rot,
			rot * quat(vec3(1.f, 0.f, 0.f), deg2rad(-90.f)),
			rot * quat(vec3(0.f, 0.f, 1.f), deg2rad(90.f)),
		};

		const vec4 colors[] =
		{
			vec4(1.f, 0.f, 1.f, 1.f),
			vec4(1.f, 1.f, 0.f, 1.f),
			vec4(0.f, 1.f, 1.f, 1.f),
		};

		for (uint32 i = 0; i < 3; ++i)
		{
			debug_render_data data = {
				createModelMatrix(transform.position, rotations[i], scaling),
				mesh.vertexBuffer, mesh.indexBuffer,
				planeSubmesh,
				colors[i] * (highlightAxis == i + 3 ? 0.5f : 1.f)
			};

			ldrRenderPass->renderOverlay<debug_simple_pipeline>(data);
		}
	}
}

bool transformation_gizmo::manipulateTransformation(trs& transform, const render_camera& camera, const user_input& input, bool allowInput, ldr_render_pass* ldrRenderPass)
{
	bool inputCaptured = handleUserInput(allowInput, true, true, true, true);
	manipulateInternal(transform, camera, input, allowInput, ldrRenderPass);
	return dragging;
}

bool transformation_gizmo::manipulatePosition(vec3& position, const render_camera& camera, const user_input& input, bool allowInput, ldr_render_pass* ldrRenderPass)
{
	if (type == transformation_type_rotation || type == transformation_type_scale)
	{
		type = transformation_type_translation;
	}
	space = transformation_global;

	bool inputCaptured = handleUserInput(allowInput, true, false, false, false);
	trs transform = { position, quat::identity, vec3(1.f, 1.f, 1.f) };
	manipulateInternal(transform, camera, input, allowInput, ldrRenderPass);
	position = transform.position;
	return dragging;
}

bool transformation_gizmo::manipulatePositionRotation(vec3& position, quat& rotation, const render_camera& camera, const user_input& input, bool allowInput, ldr_render_pass* ldrRenderPass)
{
	if (type == transformation_type_scale)
	{
		type = transformation_type_translation;
	}

	bool inputCaptured = handleUserInput(allowInput, true, true, false, true);
	trs transform = { position, rotation, vec3(1.f, 1.f, 1.f) };
	manipulateInternal(transform, camera, input, allowInput, ldrRenderPass);
	position = transform.position;
	rotation = transform.rotation;
	return dragging;
}

bool transformation_gizmo::manipulateNothing(const render_camera& camera, const user_input& input, bool allowInput, ldr_render_pass* ldrRenderPass)
{
	dragging = false;
	bool inputCaptured = handleUserInput(allowInput, true, true, true, true);
	return false;
}

bool transformation_gizmo::manipulateBoundingSphere(bounding_sphere& sphere, const trs& parentTransform, const render_camera& camera, const user_input& input, bool allowInput, ldr_render_pass* ldrRenderPass)
{
	if (type == transformation_type_rotation)
	{
		type = transformation_type_translation;
	}

	bool inputCaptured = handleUserInput(allowInput, true, false, true, true);
	trs transform = { parentTransform.rotation * sphere.center + parentTransform.position, parentTransform.rotation, vec3(sphere.radius) };
	manipulateInternal(transform, camera, input, allowInput, ldrRenderPass, false);
	sphere.center = conjugate(parentTransform.rotation) * (transform.position - parentTransform.position);
	sphere.radius = transform.scale.x;
	return dragging;
}

bool transformation_gizmo::manipulateBoundingCapsule(bounding_capsule& capsule, const trs& parentTransform, const render_camera& camera, const user_input& input, bool allowInput, ldr_render_pass* ldrRenderPass)
{
	bool inputCaptured = handleUserInput(allowInput, true, true, true, true);
	
	vec3 a = parentTransform.position + parentTransform.rotation * capsule.positionA;
	vec3 b = parentTransform.position + parentTransform.rotation * capsule.positionB;

	vec3 center = 0.5f * (a + b);
	
	trs transform = { center, parentTransform.rotation, capsule.radius };

	a = inverseTransformPosition(transform, a);
	b = inverseTransformPosition(transform, b);

	manipulateInternal(transform, camera, input, allowInput, ldrRenderPass, false);

	a = transformPosition(transform, a);
	b = transformPosition(transform, b);

	capsule.positionA = conjugate(parentTransform.rotation) * (a - parentTransform.position);
	capsule.positionB = conjugate(parentTransform.rotation) * (b - parentTransform.position);

	capsule.radius = transform.scale.x;
	return dragging;
}

bool transformation_gizmo::manipulateBoundingCylinder(bounding_cylinder& cylinder, const trs& parentTransform, const render_camera& camera, const user_input& input, bool allowInput, ldr_render_pass* ldrRenderPass)
{
	bounding_capsule c = { cylinder.positionA, cylinder.positionB, cylinder.radius };
	bool result = manipulateBoundingCapsule(c, parentTransform, camera, input, allowInput, ldrRenderPass);
	cylinder = { c.positionA, c.positionB, c.radius };
	return result;
}

bool transformation_gizmo::manipulateBoundingBox(bounding_box& aabb, const trs& parentTransform, const render_camera& camera, const user_input& input, bool allowInput, ldr_render_pass* ldrRenderPass)
{
	if (type == transformation_type_rotation)
	{
		type = transformation_type_translation;
	}

	bool inputCaptured = handleUserInput(allowInput, true, false, true, true);
	trs transform = { parentTransform.rotation * aabb.getCenter() + parentTransform.position, parentTransform.rotation, aabb.getRadius() };
	manipulateInternal(transform, camera, input, allowInput, ldrRenderPass);
	vec3 center = conjugate(parentTransform.rotation) * (transform.position - parentTransform.position);
	vec3 radius = transform.scale;
	aabb = bounding_box::fromCenterRadius(center, radius);
	return dragging;
}

bool transformation_gizmo::manipulateOrientedBoundingBox(bounding_oriented_box& obb, const trs& parentTransform, const render_camera& camera, const user_input& input, bool allowInput, ldr_render_pass* ldrRenderPass)
{
	bool inputCaptured = handleUserInput(allowInput, true, true, true, true);
	trs transform = { parentTransform.rotation * obb.center + parentTransform.position, parentTransform.rotation, obb.radius };
	manipulateInternal(transform, camera, input, allowInput, ldrRenderPass);
	obb.center = conjugate(parentTransform.rotation) * (transform.position - parentTransform.position);
	obb.rotation = conjugate(parentTransform.rotation) * transform.rotation * obb.rotation;
	obb.radius = transform.scale;
	return dragging;
}

