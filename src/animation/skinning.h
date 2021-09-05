#pragma once

#include "dx/dx_buffer.h"
#include "geometry/geometry.h"
#include "core/math.h"
#include "rendering/material.h"

struct vertex_range
{
	uint32 firstVertex;
	uint32 numVertices;
};

void initializeSkinning();
std::tuple<material_vertex_buffer_group_view, mat4*> skinObject(const material_vertex_buffer_group_view& vertexBuffer, vertex_range range, uint32 numJoints);
std::tuple<material_vertex_buffer_group_view, mat4*> skinObject(const material_vertex_buffer_group_view& vertexBuffer, uint32 numVertices, uint32 numJoints);
std::tuple<material_vertex_buffer_group_view, mat4*> skinObject(const material_vertex_buffer_group_view& vertexBuffer, submesh_info submesh, uint32 numJoints);



uint64 performSkinning();
