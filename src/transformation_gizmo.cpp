#include "pch.h"
#include "transformation_gizmo.h"
#include "geometry.h"
#include "bounding_volumes.h"
#include "dx_command_list.h"
#include "dx_pipeline.h"

#include "flat_simple_rs.hlsli"

enum gizmo_axis
{
	gizmo_axis_x,
	gizmo_axis_y,
	gizmo_axis_z,
};

static dx_mesh mesh;

static union
{
	struct
	{
		submesh_info translationSubmesh;
		submesh_info rotationSubmesh;
		submesh_info scaleSubmesh;
		submesh_info planeSubmesh;
	};

	submesh_info submeshes[4];
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

static bool dragging;
static uint32 axisIndex;
static float anchor;
static vec3 anchor3;
static vec4 plane;

static vec3 originalPosition;
static quat originalRotation;
static vec3 originalScale;

static dx_pipeline gizmoPipeline;

static ref<struct gizmo_material> materials[6];

struct gizmo_material : material_base
{
	vec4 color;

	static void setupPipeline(dx_command_list* cl, const common_material_info& info)
	{
		cl->setPipelineState(*gizmoPipeline.pipeline);
		cl->setGraphicsRootSignature(*gizmoPipeline.rootSignature);

		cl->setGraphicsDynamicConstantBuffer(2, info.cameraCBV);
	}

	void prepareForRendering(dx_command_list* cl)
	{
		cl->setGraphics32BitConstants(1, color);
	}
};


void initializeTransformationGizmos()
{
	cpu_mesh mesh(mesh_creation_flags_with_positions | mesh_creation_flags_with_normals);
	float shaftLength = 1.f;
	float headLength = 0.2f;
	float radius = 0.03f;
	float headRadius = 0.065f;
	translationSubmesh = mesh.pushArrow(6, radius, headRadius, shaftLength, headLength);
	rotationSubmesh = mesh.pushTorus(6, 64, shaftLength, radius);
	scaleSubmesh = mesh.pushMace(6, radius, headRadius, shaftLength, headLength);
	planeSubmesh = mesh.pushCube(vec3(shaftLength, 0.01f, shaftLength) * 0.2f, false, vec3(shaftLength, 0.f, shaftLength) * 0.35f);
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

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position_normal)
			.renderTargets(dx_renderer::hdrFormat[0], dx_renderer::hdrDepthStencilFormat)
			;

		gizmoPipeline = createReloadablePipeline(desc, { "flat_simple_vs", "flat_simple_ps" });

		for (uint32 i = 0; i < 6; ++i)
		{
			materials[i] = make_ref<gizmo_material>();
		}
	}
}


static uint32 handleTranslation(trs& transform, ray r, const user_input& input, transformation_space space, float snapping, float scaling)
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

static uint32 handleRotation(trs& transform, ray r, const user_input& input, transformation_space space, float snapping, float scaling)
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

static uint32 handleScaling(trs& transform, ray r, const user_input& input, transformation_space space, float scaling)
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
	}

	if (input.mouse.left.clickEvent && hoverAxisIndex != -1)
	{
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
		originalScale = transform.scale;
	}

	if (dragging)
	{
		float t;
		r.intersectPlane(plane.xyz, plane.w, t);

		vec3 axis(0.f); axis.data[axisIndex] = 1.f;
		axis = rot * axis;

		vec3 d = (dot(r.origin + t * r.direction - transform.position, axis) / anchor) * axis;

		if (space == transformation_local)
		{
			d = conjugate(rot) * d;
			transform.scale.data[axisIndex] = originalScale.data[axisIndex] * d.data[axisIndex];
		}
		else
		{
			d = conjugate(transform.rotation) * d;
			axis = conjugate(transform.rotation) * axis;

			d = abs(d);
			axis = abs(axis);

			float factorX = (d.x > 1.f) ? ((d.x - 1.f) * axis.x + 1.f) : (1.f - (1.f - d.x) * axis.x);
			float factorY = (d.y > 1.f) ? ((d.y - 1.f) * axis.y + 1.f) : (1.f - (1.f - d.y) * axis.y);
			float factorZ = (d.z > 1.f) ? ((d.z - 1.f) * axis.z + 1.f) : (1.f - (1.f - d.z) * axis.z);

			transform.scale = originalScale * vec3(factorX, factorY, factorZ);
		}
	}

	return dragging ? axisIndex : hoverAxisIndex;
}

bool manipulateTransformation(trs& transform, transformation_type& type, transformation_space& space, const render_camera& camera, const user_input& input, bool allowInput, overlay_render_pass* overlayRenderPass)
{
	if (!input.mouse.left.down)
	{
		dragging = false;
	}

	uint32 highlightAxis = -1;


	// Scale gizmos based on distance to camera.
	float scaling = length(transform.position - camera.position) / camera.getMinProjectionExtent() * 0.1f;

	if (allowInput)
	{
		if (input.keyboard['G'].pressEvent)
		{
			space = (transformation_space)(1 - space);
			dragging = false;
		}
		if (input.keyboard['Q'].pressEvent)
		{
			type = transformation_type_none;
			dragging = false;
		}
		if (input.keyboard['W'].pressEvent)
		{
			type = transformation_type_translation;
			dragging = false;
		}
		if (input.keyboard['E'].pressEvent)
		{
			type = transformation_type_rotation;
			dragging = false;
		}
		if (input.keyboard['R'].pressEvent)
		{
			type = transformation_type_scale;
			dragging = false;
		}

		if (type == transformation_type_none)
		{
			return false;
		}

		float snapping = input.keyboard[key_ctrl].down ? (type == transformation_type_rotation ? deg2rad(45.f) : 0.5f) : 0.f;

		vec3 originalPosition = transform.position;

		ray r = camera.generateWorldSpaceRay(input.mouse.relX, input.mouse.relY);


		switch (type)
		{
			case transformation_type_translation: highlightAxis = handleTranslation(transform, r, input, space, snapping, scaling); break;
			case transformation_type_rotation: highlightAxis = handleRotation(transform, r, input, space, snapping, scaling); break;
			case transformation_type_scale: highlightAxis = handleScaling(transform, r, input, space, scaling); break; // TODO: Snapping for scale.
		}
	}

	if (type == transformation_type_none)
	{
		return false;
	}


	// Render.

	quat rot = (space == transformation_global) ? quat::identity : transform.rotation;

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
			materials[i]->color = colors[i] * (highlightAxis == i ? 0.5f : 1.f);

			overlayRenderPass->renderObject(mesh.vertexBuffer, mesh.indexBuffer,
				submeshes[type],
				materials[i],
				createModelMatrix(transform.position, rotations[i], scaling),
				true
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
			materials[i + 3]->color = colors[i] * (highlightAxis == i + 3 ? 0.5f : 1.f);

			overlayRenderPass->renderObject(mesh.vertexBuffer, mesh.indexBuffer,
				planeSubmesh,
				materials[i + 3],
				createModelMatrix(transform.position, rotations[i], scaling),
				true
			);
		}
	}
	
	return dragging;
}

