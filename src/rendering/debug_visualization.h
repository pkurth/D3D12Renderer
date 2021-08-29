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
	static void setupCommon(dx_command_list* cl, const common_material_info& materialInfo);
	static void render(dx_command_list* cl, const mat4& viewProj, const default_render_command<debug_simple_pipeline>& rc);
};

struct debug_unlit_pipeline
{
	using material_t = debug_material;

	static void initialize();
	static void setupCommon(dx_command_list* cl, const common_material_info& materialInfo);
	static void render(dx_command_list* cl, const mat4& viewProj, const default_render_command<debug_unlit_pipeline>& rc);
};




