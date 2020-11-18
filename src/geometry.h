#pragma once

#include "dx_render_primitives.h"
#include "bounding_volumes.h"
#include "animation.h"

// Members are always pushed in this order!
enum mesh_creation_flags
{
	mesh_creation_flags_none			= 0,
	mesh_creation_flags_with_positions	= (1 << 0),
	mesh_creation_flags_with_uvs		= (1 << 1),
	mesh_creation_flags_with_normals	= (1 << 2),
	mesh_creation_flags_with_tangents	= (1 << 3),
	mesh_creation_flags_with_skin		= (1 << 4),
};

struct cpu_mesh
{
	cpu_mesh() {}
	cpu_mesh(uint32 flags);
	cpu_mesh(const cpu_mesh& mesh) = delete;
	cpu_mesh(cpu_mesh&& mesh);
	~cpu_mesh();

	uint32 flags = 0;
	uint32 vertexSize = 0;
	uint32 skinOffset = 0;

	uint8* vertices = 0;
	indexed_triangle16* triangles = 0;

	uint32 numVertices = 0;
	uint32 numTriangles = 0;

	submesh_info pushQuad(vec2 radius);
	submesh_info pushQuad(float radius) { return pushQuad(vec2(radius, radius)); }
	submesh_info pushCube(vec3 radius, bool flipWindingOrder = false);
	submesh_info pushCube(float radius, bool flipWindingOrder = false) { return pushCube(vec3(radius, radius, radius), flipWindingOrder); }
	submesh_info pushSphere(uint16 slices, uint16 rows, float radius);
	submesh_info pushCapsule(uint16 slices, uint16 rows, float height, float radius);
	submesh_info pushCylinder(uint16 slices, float radius, float height);
	submesh_info pushArrow(uint16 slices, float shaftRadius, float headRadius, float shaftLength, float headLength);
	submesh_info pushTorus(uint16 slices, uint16 segments, float torusRadius, float tubeRadius);
	submesh_info pushMace(uint16 slices, float shaftRadius, float headRadius, float shaftLength, float headLength);

	submesh_info pushAssimpMesh(const struct aiMesh* mesh, float scale, bounding_box* aabb = 0);

	dx_mesh createDXMesh();
	dx_vertex_buffer createVertexBufferWithAlternativeLayout(uint32 otherFlags, bool allowUnorderedAccess = false);

private:
	void alignNextTriangle();
	void reserve(uint32 vertexCount, uint32 triangleCount);
	void pushTriangle(uint16 a, uint16 b, uint16 c);
};


static D3D12_INPUT_ELEMENT_DESC inputLayout_position[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static D3D12_INPUT_ELEMENT_DESC inputLayout_position_uv[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static D3D12_INPUT_ELEMENT_DESC inputLayout_position_uv_normal[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static D3D12_INPUT_ELEMENT_DESC inputLayout_position_uv_normal_tangent[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static D3D12_INPUT_ELEMENT_DESC inputLayout_position_uv_normal_skin[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "SKIN_INDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "SKIN_WEIGHTS", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static D3D12_INPUT_ELEMENT_DESC inputLayout_position_uv_normal_tangent_skin[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "SKIN_INDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "SKIN_WEIGHTS", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

