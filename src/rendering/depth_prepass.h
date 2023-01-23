#pragma once

#include "material.h"
#include "render_command.h"





struct static_depth_prepass_data
{
	mat4 transform;

	dx_vertex_buffer_view vertexBuffer;
	dx_index_buffer_view indexBuffer;
	submesh_info submesh;
};

struct static_depth_prepass_pipeline
{
	using render_data_t = static_depth_prepass_data;

	DEPTH_ONLY_RENDER_DECL;

	struct single_sided;
	struct double_sided;
};

struct static_depth_prepass_pipeline::single_sided : static_depth_prepass_pipeline
{
	PIPELINE_SETUP_DECL;
};

struct static_depth_prepass_pipeline::double_sided : static_depth_prepass_pipeline
{
	PIPELINE_SETUP_DECL;
};






struct dynamic_depth_prepass_data
{
	mat4 transform;
	mat4 prevFrameTransform;

	dx_vertex_buffer_view vertexBuffer;
	dx_index_buffer_view indexBuffer;
	submesh_info submesh;
};

struct dynamic_depth_prepass_pipeline
{
	using render_data_t = dynamic_depth_prepass_data;

	DEPTH_ONLY_RENDER_DECL;

	struct single_sided;
	struct double_sided;
};

struct dynamic_depth_prepass_pipeline::single_sided : dynamic_depth_prepass_pipeline
{
	PIPELINE_SETUP_DECL;
};

struct dynamic_depth_prepass_pipeline::double_sided : dynamic_depth_prepass_pipeline
{
	PIPELINE_SETUP_DECL;
};






struct animated_depth_prepass_data
{
	mat4 transform;
	mat4 prevFrameTransform;

	D3D12_GPU_VIRTUAL_ADDRESS prevFrameVertexBufferAddress;

	dx_vertex_buffer_view vertexBuffer;
	dx_index_buffer_view indexBuffer;
	submesh_info submesh;
};

struct animated_depth_prepass_pipeline
{
	using render_data_t = animated_depth_prepass_data;

	DEPTH_ONLY_RENDER_DECL;

	struct single_sided;
	struct double_sided;
};

struct animated_depth_prepass_pipeline::single_sided : animated_depth_prepass_pipeline
{
	PIPELINE_SETUP_DECL;
};

struct animated_depth_prepass_pipeline::double_sided : animated_depth_prepass_pipeline
{
	PIPELINE_SETUP_DECL;
};



void initializeDepthPrepassPipelines();
