#include "pch.h"
#include "transformation_gizmo.h"
#include "dx/dx_command_list.h"
#include "dx/dx_pipeline.h"
#include "geometry/geometry.h"
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
	cpu_mesh mesh(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals);
	float shaftLength = 1.f;
	float headLength = 0.2f;
	float radius = 0.03f;
	float headRadius = 0.065f;
	translationSubmesh = mesh.pushArrow(6, radius, headRadius, shaftLength, headLength);
	rotationSubmesh = mesh.pushTorus(6, 64, shaftLength, radius);
	scaleSubmesh = mesh.pushMace(6, radius, headRadius, shaftLength, headLength);
	planeSubmesh = mesh.pushCube(vec3(shaftLength, 0.01f, shaftLength) * 0.2f, false, vec3(shaftLength, 0.f, shaftLength) * 0.35f);
	boxSubmesh = mesh.pushCube((shaftLength + headLength) * 0.3f);
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

uint32 transformation_gizmo::handleScaling(trs& transform, ray r, const user_input& input, float scaling, const render_camera& camera)
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
	else
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

bool transformation_gizmo::handleUserInput(const user_input& input, bool allowKeyboardInput, 
	bool allowTranslation, bool allowRotation, bool allowScaling, bool allowSpaceChange)
{
	bool keyboardInteraction = false;
	bool uiInteraction = false;

	if (allowKeyboardInput)
	{
		if (type != transformation_type_scale)
		{
			if (input.keyboard['G'].pressEvent && allowSpaceChange)
			{
				space = (transformation_space)(1 - space);
				dragging = false;
				keyboardInteraction = true;
			}
		}
		if (input.keyboard['Q'].pressEvent)
		{
			type = transformation_type_none;
			dragging = false;
			keyboardInteraction = true;
		}
		if (input.keyboard['W'].pressEvent && allowTranslation)
		{
			type = transformation_type_translation;
			dragging = false;
			keyboardInteraction = true;
		}
		if (input.keyboard['E'].pressEvent && allowRotation)
		{
			type = transformation_type_rotation;
			dragging = false;
			keyboardInteraction = true;
		}
		if (input.keyboard['R'].pressEvent && allowScaling)
		{
			type = transformation_type_scale;
			dragging = false;
			keyboardInteraction = true;
		}
	}

	const uint32 iconSize = 35;
	const float iconSpacing = 3.f;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f);
	ImGui::SetNextWindowSize(ImVec2(0.f, 0.f)); // Auto-resize to content.
	if (ImGui::Begin("##GizmoControls", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove))
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
		uiInteraction |= ImGui::IconRadioButton(imgui_icon_global, (int*)&space, transformation_global, iconSize, allowGlobal);
		ImGui::SameLine(0.f, iconSpacing);
		uiInteraction |= ImGui::IconRadioButton(imgui_icon_local, (int*)&space, transformation_local, iconSize, allowLocal);
		ImGui::PopID();

		ImGui::SameLine(0.f, (float)iconSize);


		ImGui::PushID(&type);
		uiInteraction |= ImGui::IconRadioButton(imgui_icon_cross, (int*)&type, transformation_type_none, iconSize, true);
		ImGui::SameLine(0.f, iconSpacing);
		uiInteraction |= ImGui::IconRadioButton(imgui_icon_translate, (int*)&type, transformation_type_translation, iconSize, allowTranslation);
		ImGui::SameLine(0.f, iconSpacing);
		uiInteraction |= ImGui::IconRadioButton(imgui_icon_rotate, (int*)&type, transformation_type_rotation, iconSize, allowRotation);
		ImGui::SameLine(0.f, iconSpacing);
		uiInteraction |= ImGui::IconRadioButton(imgui_icon_scale, (int*)&type, transformation_type_scale, iconSize, allowScaling);
		ImGui::PopID();

		ImGui::SameLine(0.f, (float)iconSize);

		ImGui::PopStyleColor();
	}

	ImGui::End();
	ImGui::PopStyleVar(1);

	return keyboardInteraction || uiInteraction;
}

void transformation_gizmo::manipulateInternal(trs& transform, const render_camera& camera, const user_input& input, bool allowInput, ldr_render_pass* ldrRenderPass)
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
			case transformation_type_scale: highlightAxis = handleScaling(transform, r, input, scaling, camera); break; // TODO: Snapping for scale.
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

		for (uint32 i = 0; i < 3; ++i)
		{
			ldrRenderPass->renderOverlay<debug_simple_pipeline>(createModelMatrix(transform.position, rotations[i], scaling),
				mesh.vertexBuffer, mesh.indexBuffer,
				submeshes[type],
				debug_material{ colors[i] * (highlightAxis == i ? 0.5f : 1.f) }
			);
		}

		if (type == transformation_type_scale)
		{
			ldrRenderPass->renderOverlay<debug_simple_pipeline>(createModelMatrix(transform.position, rot, scaling),
				mesh.vertexBuffer, mesh.indexBuffer,
				boxSubmesh,
				debug_material{ vec4(0.5f) * (highlightAxis == 3 ? 0.5f : 1.f) }
			);
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
			ldrRenderPass->renderOverlay<debug_simple_pipeline>(createModelMatrix(transform.position, rotations[i], scaling),
				mesh.vertexBuffer, mesh.indexBuffer,
				planeSubmesh,
				debug_material{ colors[i] * (highlightAxis == i + 3 ? 0.5f : 1.f) }
			);
		}
	}
}

bool transformation_gizmo::manipulateTransformation(trs& transform, const render_camera& camera, const user_input& input, bool allowInput, ldr_render_pass* ldrRenderPass)
{
	bool inputCaptured = handleUserInput(input, allowInput, true, true, true, true);
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

	bool inputCaptured = handleUserInput(input, allowInput, true, false, false, false);
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

	bool inputCaptured = handleUserInput(input, allowInput, true, true, false, true);
	trs transform = { position, rotation, vec3(1.f, 1.f, 1.f) };
	manipulateInternal(transform, camera, input, allowInput, ldrRenderPass);
	position = transform.position;
	rotation = transform.rotation;
	return dragging;
}

bool transformation_gizmo::manipulateNothing(const render_camera& camera, const user_input& input, bool allowInput, ldr_render_pass* ldrRenderPass)
{
	dragging = false;
	bool inputCaptured = handleUserInput(input, allowInput, true, true, true, true);
	return false;
}

