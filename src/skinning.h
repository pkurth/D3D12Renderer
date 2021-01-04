#pragma once

#include "dx_buffer.h"
#include "geometry.h"
#include "math.h"

struct vertex_range
{
	uint32 firstVertex;
	uint32 numVertices;
};

void initializeSkinning();
std::tuple<ref<dx_vertex_buffer>, vertex_range, mat4*> skinObject(const ref<dx_vertex_buffer>& vertexBuffer, vertex_range range, uint32 numJoints);
std::tuple<ref<dx_vertex_buffer>, uint32, mat4*> skinObject(const ref<dx_vertex_buffer>& vertexBuffer, uint32 numJoints);
std::tuple<ref<dx_vertex_buffer>, submesh_info, mat4*> skinObject(const ref<dx_vertex_buffer>& vertexBuffer, submesh_info submesh, uint32 numJoints);
bool performSkinning();
