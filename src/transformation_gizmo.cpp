#include "pch.h"
#include "transformation_gizmo.h"
#include "geometry.h"
#include "bounding_volumes.h"


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
	};

	submesh_info submeshes[3];
};

static const quat rotations[] =
{
	quat(vec3(0.f, 0.f, -1.f), deg2rad(90.f)),
	quat::identity,
	quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f)),
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

static const vec4 colors[] =
{
	vec4(1.f, 0.f, 0.f, 1.f),
	vec4(0.f, 1.f, 0.f, 1.f),
	vec4(0.f, 0.f, 1.f, 1.f),
};

static submesh_info submesh(transformation_type type) { return submeshes[type]; }

static quat rotation(gizmo_axis axis) { return rotations[axis]; }
static vec4 color(gizmo_axis axis) { return colors[axis]; }


static bool dragging;
static uint32 axisIndex;
static float anchor;
static vec4 plane;

static vec3 originalPosition;
static quat originalRotation;
static vec3 originalScale;

void initializeTransformationGizmos()
{
	cpu_mesh mesh(mesh_creation_flags_with_positions | mesh_creation_flags_with_normals);
	float shaftLength = 2.f;
	float headLength = 0.4f;
	float radius = 0.06f;
	float headRadius = 0.13f;
	translationSubmesh = mesh.pushArrow(6, radius, headRadius, shaftLength, headLength);
	rotationSubmesh = mesh.pushTorus(6, 64, shaftLength, radius);
	scaleSubmesh = mesh.pushMace(6, radius, headRadius, shaftLength, headLength);
	::mesh = mesh.createDXMesh();

	for (uint32 i = 0; i < 3; ++i)
	{
		cylinders[i].positionB *= shaftLength + headLength;
		cylinders[i].radius *= radius * 1.1f;

		tori[i].majorRadius *= shaftLength;
		tori[i].tubeRadius *= radius * 1.1f;
	}
}


static uint32 handleTranslation(trs& transform, ray r, const user_input& input, transformation_space space)
{
	quat rot = (space == transformation_global) ? quat::identity : transform.rotation;

	uint32 hoverAxisIndex = -1;
	float minT = FLT_MAX;
	for (uint32 i = 0; i < 3; ++i)
	{
		float t;
		bounding_cylinder cylinder = { rot * cylinders[i].positionA + transform.position, rot * cylinders[i].positionB + transform.position, cylinders[i].radius };
		if (r.intersectCylinder(cylinder, t) && t < minT)
		{
			hoverAxisIndex = i;
			minT = t;
		}
	}

	if (input.mouse.left.clickEvent && hoverAxisIndex != -1)
	{
		const vec4 planes[][2] =
		{
			{ createPlane(transform.position, rot * vec3(0.f, 1.f, 0.f)), createPlane(transform.position, rot * vec3(0.f, 0.f, 1.f)) },
			{ createPlane(transform.position, rot * vec3(0.f, 0.f, 1.f)), createPlane(transform.position, rot * vec3(0.f, 0.f, 1.f)) },
			{ createPlane(transform.position, rot * vec3(1.f, 0.f, 0.f)), createPlane(transform.position, rot * vec3(0.f, 1.f, 0.f)) },
		};

		dragging = true;
		axisIndex = hoverAxisIndex;
		
		const vec4* axisPlanes = planes[hoverAxisIndex];
		float a = abs(dot(r.direction, axisPlanes[0].xyz));
		float b = abs(dot(r.direction, axisPlanes[1].xyz));

		plane = (a > b) ? axisPlanes[0] : axisPlanes[1];

		vec3 axis(0.f); axis.data[axisIndex] = 1.f;
		axis = rot * axis;

		float t;
		r.intersectPlane(plane.xyz, plane.w, t);
		anchor = dot(r.origin + t * r.direction - transform.position, axis);
		originalPosition = transform.position;
	}

	if (dragging)
	{
		float t;
		r.intersectPlane(plane.xyz, plane.w, t);

		vec3 axis(0.f); axis.data[axisIndex] = 1.f;
		axis = rot * axis;

		transform.position = originalPosition + (dot(r.origin + t * r.direction - originalPosition, axis) - anchor) * axis;
	}

	return dragging ? axisIndex : hoverAxisIndex;
}

static uint32 handleRotation(trs& transform, ray r, const user_input& input, transformation_space space)
{
	quat rot = (space == transformation_global) ? quat::identity : transform.rotation;

	uint32 hoverAxisIndex = -1;
	float minT = FLT_MAX;
	for (uint32 i = 0; i < 3; ++i)
	{
		float t;
		bounding_torus torus = { rot * tori[i].position + transform.position, rot * tori[i].upAxis, tori[i].majorRadius, tori[i].tubeRadius };
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

static uint32 handleScaling(trs& transform, ray r, const user_input& input, transformation_space space)
{
	quat rot = (space == transformation_global) ? quat::identity : transform.rotation;

	uint32 hoverAxisIndex = -1;
	float minT = FLT_MAX;
	for (uint32 i = 0; i < 3; ++i)
	{
		float t;
		bounding_cylinder cylinder = { rot * cylinders[i].positionA + transform.position, rot * cylinders[i].positionB + transform.position, cylinders[i].radius };
		if (r.intersectCylinder(cylinder, t) && t < minT)
		{
			hoverAxisIndex = i;
			minT = t;
		}
	}

	if (input.mouse.left.clickEvent && hoverAxisIndex != -1)
	{
		const vec4 planes[][2] =
		{
			{ createPlane(transform.position, rot * vec3(0.f, 1.f, 0.f)), createPlane(transform.position, rot * vec3(0.f, 0.f, 1.f)) },
			{ createPlane(transform.position, rot * vec3(0.f, 0.f, 1.f)), createPlane(transform.position, rot * vec3(0.f, 0.f, 1.f)) },
			{ createPlane(transform.position, rot * vec3(1.f, 0.f, 0.f)), createPlane(transform.position, rot * vec3(0.f, 1.f, 0.f)) },
		};

		dragging = true;
		axisIndex = hoverAxisIndex;

		const vec4* axisPlanes = planes[hoverAxisIndex];
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

bool manipulateTransformation(trs& transform, transformation_type type, transformation_space space, const render_camera& camera, const user_input& input, dx_renderer* renderer)
{
	if (!input.mouse.left.down)
	{
		dragging = false;
	}

	vec3 originalPosition = transform.position;

	ray r = camera.generateWorldSpaceRay(input.mouse.relX, input.mouse.relY);


	uint32 highlightAxis = -1;
	switch (type)
	{
		case transformation_type_translation: highlightAxis = handleTranslation(transform, r, input, space); break;
		case transformation_type_rotation: highlightAxis = handleRotation(transform, r, input, space); break;
		case transformation_type_scale: highlightAxis = handleScaling(transform, r, input, space); break;
	}

	quat rot = (space == transformation_global) ? quat::identity : transform.rotation;

	visualization_render_pass* visualizationPass = renderer->beginVisualizationPass();
	visualizationPass->renderObject(mesh.vertexBuffer, mesh.indexBuffer,
		submesh(type),
		createModelMatrix(transform.position, rot * rotation(gizmo_axis_x)),
		color(gizmo_axis_x) * (highlightAxis == gizmo_axis_x ? 0.5f : 1.f)
	);
	visualizationPass->renderObject(mesh.vertexBuffer, mesh.indexBuffer,
		submesh(type),
		createModelMatrix(transform.position, rot * rotation(gizmo_axis_y)),
		color(gizmo_axis_y) * (highlightAxis == gizmo_axis_y ? 0.5f : 1.f)
	);
	visualizationPass->renderObject(mesh.vertexBuffer, mesh.indexBuffer,
		submesh(type),
		createModelMatrix(transform.position, rot * rotation(gizmo_axis_z)),
		color(gizmo_axis_z) * (highlightAxis == gizmo_axis_z ? 0.5f : 1.f)
	);

	return dragging;
}

