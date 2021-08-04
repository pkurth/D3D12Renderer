#pragma once

#include "dx/dx_buffer.h"
#include "geometry/geometry.h"
#include "core/math.h"

struct vertex_range
{
	uint32 firstVertex;
	uint32 numVertices;
};

void initializeSkinning();
std::tuple<vertex_buffer_group, vertex_range, mat4*> skinObject(const vertex_buffer_group& vertexBuffer, vertex_range range, uint32 numJoints);
std::tuple<vertex_buffer_group, uint32, mat4*> skinObject(const vertex_buffer_group& vertexBuffer, uint32 numJoints);
std::tuple<vertex_buffer_group, submesh_info, mat4*> skinObject(const vertex_buffer_group& vertexBuffer, submesh_info submesh, uint32 numJoints);
uint64 performSkinning();
