#pragma once

enum gizmo_type
{
	gizmo_type_none,
	gizmo_type_translation,
	gizmo_type_rotation,
	gizmo_type_scale,

	gizmo_type_count,
};

static const char* gizmoTypeNames[] =
{
	"None",
	"Translation",
	"Rotation",
	"Scale",
};
