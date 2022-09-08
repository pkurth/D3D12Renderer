#pragma once

#include "render_pass.h"
#include "dx/dx_command_list.h"
#include "core/camera.h"

#include "visualization_rs.hlsli"


struct position_color
{
	vec3 position;
	vec3 color;

	position_color(vec3 p, vec3 c = vec3(1.f, 1.f, 1.f))
		: position(p), color(c) {}
};

struct debug_material
{
	vec4 color;
	ref<dx_texture> texture; // Multiplied with color. Can be null, in which case this is white * color.
	vec2 uv0 = vec2(0.f, 0.f);
	vec2 uv1 = vec2(1.f, 1.f);
};

struct debug_line_material
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

struct debug_unlit_line_pipeline
{
	using material_t = debug_line_material;

	static void initialize();

	PIPELINE_SETUP_DECL;
	PIPELINE_RENDER_DECL;
};

void renderWireDebug(const mat4& transform, const struct dx_dynamic_vertex_buffer& vb, const struct dx_dynamic_index_buffer& ib, vec4 color, ldr_render_pass* renderPass, bool overlay = false);

void renderLine(vec3 positionA, vec3 positionB, vec4 color, ldr_render_pass* renderPass, bool overlay = false);
void renderWireSphere(vec3 position, float radius, vec4 color, ldr_render_pass* renderPass, bool overlay = false);
void renderWireCapsule(vec3 positionA, vec3 positionB, float radius, vec4 color, ldr_render_pass* renderPass, bool overlay = false);
void renderWireCone(vec3 position, vec3 direction, float distance, float angle, vec4 color, ldr_render_pass* renderPass, bool overlay = false);
void renderWireBox(vec3 position, vec3 radius, quat rotation, vec4 color, ldr_render_pass* renderPass, bool overlay = false);
void renderCameraFrustum(const render_camera& frustum, vec4 color, ldr_render_pass* renderPass, float alternativeFarPlane = -1.f, bool overlay = false);
