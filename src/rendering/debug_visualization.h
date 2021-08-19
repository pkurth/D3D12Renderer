#pragma once

#include "render_utils.h"
#include "render_pass.h"
#include "dx/dx_command_list.h"
#include "core/camera.h"

#include "visualization_rs.hlsli"

struct flat_simple_material : material_base
{
	vec4 color;

	static void setupPipeline(dx_command_list* cl, const common_material_info& info);
	void prepareForRendering(dx_command_list* cl);
};


struct flat_unlit_material : material_base
{
	vec4 color;

	void prepareForRendering(dx_command_list* cl);
};

struct flat_unlit_triangle_material : flat_unlit_material
{
	static void setupPipeline(dx_command_list* cl, const common_material_info& info);
};

struct flat_unlit_line_material : flat_unlit_material
{
	static void setupPipeline(dx_command_list* cl, const common_material_info& info);
};



