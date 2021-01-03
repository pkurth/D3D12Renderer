#pragma once

#include "dx_buffer.h"
#include "geometry.h"
#include "math.h"

void initializeSkinning();
std::tuple<ref<dx_vertex_buffer>, submesh_info, mat4*> skinObject(const ref<dx_vertex_buffer>& vertexBuffer, submesh_info submesh, uint32 numJoints);
bool performSkinning();
