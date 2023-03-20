#pragma once

#include "pbr_material.h"
#include "render_command.h"

struct dx_command_list;

struct pbr_render_data
{
	D3D12_GPU_VIRTUAL_ADDRESS transformPtr;
	dx_vertex_buffer_group_view vertexBuffer;
	dx_index_buffer_view indexBuffer;
	submesh_info submesh;

	ref<pbr_material> material;

	uint32 numInstances;
};

struct pbr_pipeline
{
	static void initialize();

	PIPELINE_RENDER_DECL(pbr_render_data);

	struct opaque;
	struct opaque_double_sided;
	struct transparent;

protected:
	static void setupPBRCommon(dx_command_list* cl, const common_render_data& common);
};

struct pbr_pipeline::opaque : pbr_pipeline
{
	PIPELINE_SETUP_DECL;
};

struct pbr_pipeline::opaque_double_sided : pbr_pipeline
{
	PIPELINE_SETUP_DECL;
};

struct pbr_pipeline::transparent : pbr_pipeline
{
	PIPELINE_SETUP_DECL;
};


