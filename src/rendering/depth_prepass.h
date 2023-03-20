#pragma once

#include "material.h"
#include "render_command.h"





struct depth_prepass_data
{
	D3D12_GPU_VIRTUAL_ADDRESS transformPtr;
	D3D12_GPU_VIRTUAL_ADDRESS prevFrameTransformPtr;
	D3D12_GPU_VIRTUAL_ADDRESS objectIDPtr;

	dx_vertex_buffer_group_view vertexBuffer;
	dx_vertex_buffer_view prevFrameVertexBuffer;
	dx_index_buffer_view indexBuffer;

	submesh_info submesh;

	uint32 numInstances;

	dx_cpu_descriptor_handle alphaCutoutTextureSRV = {};
};

struct depth_prepass_pipeline
{
	DEPTH_ONLY_RENDER_DECL(depth_prepass_data);

	struct single_sided;
	struct double_sided;
	struct alpha_cutout;
};

struct depth_prepass_pipeline::single_sided : depth_prepass_pipeline
{
	PIPELINE_SETUP_DECL;
};

struct depth_prepass_pipeline::double_sided : depth_prepass_pipeline
{
	PIPELINE_SETUP_DECL;
};

struct depth_prepass_pipeline::alpha_cutout : depth_prepass_pipeline
{
	PIPELINE_SETUP_DECL;
	DEPTH_ONLY_RENDER_DECL(depth_prepass_data);
};



void initializeDepthPrepassPipelines();
