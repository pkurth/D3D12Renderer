#pragma once

#include "geometry/mesh.h"
#include "shadow_map_cache.h"

struct particle_draw_info
{
	ref<dx_buffer> particleBuffer;
	ref<dx_buffer> aliveList;
	ref<dx_buffer> commandBuffer;
	uint32 aliveListOffset;
	uint32 commandBufferOffset;
	uint32 rootParameterOffset;
};


template <typename material_t>
struct default_render_command
{
	mat4 transform;
	material_vertex_buffer_group_view vertexBuffer;
	material_index_buffer_view indexBuffer;
	submesh_info submesh;

	material_t material;
};

template <typename material_t>
struct particle_render_command
{
	material_vertex_buffer_group_view vertexBuffer;
	material_index_buffer_view indexBuffer;
	particle_draw_info drawInfo;

	material_t material;
};

struct static_depth_only_render_command
{
	mat4 transform;
	material_vertex_buffer_view vertexBuffer;
	material_index_buffer_view indexBuffer;
	submesh_info submesh;
	uint32 objectID;
};

struct dynamic_depth_only_render_command
{
	mat4 transform;
	mat4 prevFrameTransform;
	material_vertex_buffer_view vertexBuffer;
	material_index_buffer_view indexBuffer;
	submesh_info submesh;
	uint32 objectID;
};

struct animated_depth_only_render_command
{
	mat4 transform;
	mat4 prevFrameTransform;
	material_vertex_buffer_view vertexBuffer;
	D3D12_GPU_VIRTUAL_ADDRESS prevFrameVertexBufferAddress;
	material_index_buffer_view indexBuffer;
	submesh_info submesh;
	uint32 objectID;
};

struct outline_render_command
{
	mat4 transform;
	material_vertex_buffer_view vertexBuffer;
	material_index_buffer_view indexBuffer;
	submesh_info submesh;
};

struct shadow_render_command
{
	mat4 transform;
	material_vertex_buffer_view vertexBuffer;
	material_index_buffer_view indexBuffer;
	submesh_info submesh;
};




