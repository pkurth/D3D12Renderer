#pragma once

#include "render_pass.h"
#include "dx/dx_command_list.h"
#include "core/camera.h"

#include "visualization_rs.hlsli"

struct debug_material
{
	vec4 color;
	ref<dx_texture> texture; // Multiplied with color. Can be null, in which case this is white * color.
	vec2 uv0 = vec2(0.f, 0.f);
	vec2 uv1 = vec2(1.f, 1.f);
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






