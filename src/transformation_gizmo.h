#pragma once

#include "math.h"
#include "input.h"
#include "camera.h"
#include "dx_renderer.h"

enum transformation_type
{
	transformation_type_translation,
	transformation_type_rotation,
	transformation_type_scale,
};

enum transformation_space
{
	transformation_global,
	transformation_local,
};

static const char* transformationTypeNames[] =
{
	"Translation",
	"Rotation",
	"Scale",
};

static const char* transformationSpaceNames[] =
{
	"Global",
	"Local",
};

void initializeTransformationGizmos();
bool manipulateTransformation(trs& transform, transformation_type& type, transformation_space& space, const render_camera& camera, const user_input& input, dx_renderer* renderer);

