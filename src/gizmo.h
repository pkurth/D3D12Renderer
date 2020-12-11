#pragma once

#include "geometry.h"
#include "math.h"

enum gizmo_type
{
	gizmo_type_translation,
	gizmo_type_rotation,
	gizmo_type_scale,
};

enum gizmo_axis
{
	gizmo_axis_x,
	gizmo_axis_y,
	gizmo_axis_z,
};

struct gizmo
{
	void initialize(uint32 meshFlags = mesh_creation_flags_with_positions | mesh_creation_flags_with_normals);

	submesh_info submesh(gizmo_type type) { return submeshes[type]; }
	const char* name(gizmo_type type) { return names[type]; }

	quat rotation(gizmo_axis axis) { return rotations[axis]; }
	vec4 color(gizmo_axis axis) { return colors[axis]; }


	dx_mesh mesh;

private:
	union
	{
		struct
		{
			submesh_info translationSubmesh;
			submesh_info rotationSubmesh;
			submesh_info scaleSubmesh;
		};

		submesh_info submeshes[3];
	};

	const quat rotations[3] =
	{
		quat(vec3(0.f, 0.f, -1.f), deg2rad(90.f)),
		quat::identity,
		quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f)),
	};

	const vec4 colors[3] =
	{
		vec4(1.f, 0.f, 0.f, 1.f),
		vec4(0.f, 1.f, 0.f, 1.f),
		vec4(0.f, 0.f, 1.f, 1.f),
	};

	const char* names[3] =
	{
		"Translation",
		"Rotation",
		"Scale",
	};
};


