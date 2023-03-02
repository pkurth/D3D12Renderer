#pragma once

#include "core/math.h"
#include "core/reflect.h"

struct tag_component
{
	char name[16];

	tag_component() {}

	tag_component(const char* n)
	{
		strncpy(name, n, sizeof(name));
		name[sizeof(name) - 1] = 0;
	}
};
REFLECT_STRUCT(tag_component,
	(name, "Name")
);

struct transform_component : trs
{
	transform_component() {}
	transform_component(const trs& t) : trs(t) {}
	transform_component(vec3 position, quat rotation, vec3 scale = vec3(1.f, 1.f, 1.f)) : trs(position, rotation, scale) {}
	transform_component& operator=(const trs& t) { trs::operator=(t); return *this; }
};
REFLECT_STRUCT(transform_component,
	(position, "Position"),
	(rotation, "Rotation"),
	(scale, "Scale")
);

struct dynamic_transform_component : trs
{
	dynamic_transform_component() {}
	dynamic_transform_component(const trs& t) : trs(t) {}
	dynamic_transform_component(vec3 position, quat rotation, vec3 scale = vec3(1.f, 1.f, 1.f)) : trs(position, rotation, scale) {}
	dynamic_transform_component& operator=(const trs& t) { trs::operator=(t); return *this; }
};

struct position_component
{
	vec3 position;
};
REFLECT_STRUCT(position_component,
	(position, "Position")
);

struct position_rotation_component
{
	vec3 position;
	quat rotation;
};
REFLECT_STRUCT(position_rotation_component,
	(position, "Position"),
	(rotation, "Rotation")
);

struct position_scale_component
{
	vec3 position;
	vec3 scale;
};
REFLECT_STRUCT(position_scale_component,
	(position, "Position"),
	(scale, "Scale")
);
