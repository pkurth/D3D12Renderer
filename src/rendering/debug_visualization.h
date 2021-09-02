#pragma once

#include "render_pass.h"
#include "dx/dx_command_list.h"
#include "core/camera.h"

#include "visualization_rs.hlsli"

struct debug_material
{
	vec4 color;
};

struct debug_simple_pipeline
{
	using material_t = debug_material;

	static void initialize();

	PIPELINE_SETUP_DECL;
	PIPELINE_RENDER_DECL;
};

struct debug_unlit_pipeline
{
	using material_t = debug_material;

	static void initialize();

	PIPELINE_SETUP_DECL;
	PIPELINE_RENDER_DECL;
};




