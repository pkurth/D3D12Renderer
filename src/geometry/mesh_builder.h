#pragma once

#include "core/memory.h"
#include "physics/bounding_volumes.h"
#include "animation/animation.h"
#include "dx/dx_buffer.h"



struct submesh_info
{
	uint32 numIndices;
	uint32 firstIndex;
	uint32 baseVertex;
	uint32 numVertices;
};


// Members are always pushed in this order!
enum mesh_creation_flags
{
	mesh_creation_flags_none = 0,
	mesh_creation_flags_with_positions = (1 << 0),
	mesh_creation_flags_with_uvs = (1 << 1),
	mesh_creation_flags_with_normals = (1 << 2),
	mesh_creation_flags_with_tangents = (1 << 3),
	mesh_creation_flags_with_skin = (1 << 4),

	mesh_creation_flags_default = mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents,
	mesh_creation_flags_animated = mesh_creation_flags_default | mesh_creation_flags_with_skin,
};

static uint32 getVertexSize(uint32 meshFlags)
{
	uint32 size = 0;
	//if (meshFlags & mesh_creation_flags_with_positions) { size += sizeof(vec3); }
	if (meshFlags & mesh_creation_flags_with_uvs) { size += sizeof(vec2); }
	if (meshFlags & mesh_creation_flags_with_normals) { size += sizeof(vec3); }
	if (meshFlags & mesh_creation_flags_with_tangents) { size += sizeof(vec3); }
	if (meshFlags & mesh_creation_flags_with_skin) { size += sizeof(skinning_weights); }
	return size;
}



enum mesh_index_type
{
	mesh_index_uint16,
	mesh_index_uint32,
};

struct quad_mesh_desc
{
	vec3 center = 0.f;
	vec2 radius = 1.f;
	quat rotation = quat::identity;
};

struct box_mesh_desc
{
	vec3 center = 0.f;
	vec3 radius = 1.f;
	quat rotation = quat::identity;
};

struct tesselated_box_mesh_desc
{
	vec3 center = 0.f;
	vec3 radius = 1.f;
	uint32 numIntervals = 1;
	quat rotation = quat::identity;
};

struct sphere_mesh_desc
{
	vec3 center = 0.f;
	float radius = 1.f;
	uint32 slices = 15;
	uint32 rows = 15;
};

struct icosphere_mesh_desc
{
	vec3 center = 0.f;
	float radius = 1.f;
	uint32 refinement = 0;
};

// All meshes are standing upright by default.
struct capsule_mesh_desc
{
	vec3 center = 0.f;
	float height = 1.f;
	float radius = 0.4f;
	quat rotation = quat::identity;
	uint32 slices = 15;
	uint32 rows = 15;

	capsule_mesh_desc() {}
	capsule_mesh_desc(vec3 posA, vec3 posB, float radius)
	{
		center = (posA + posB) * 0.5f;
		height = length(posA - posB);
		this->radius = radius;
		rotation = rotateFromTo(vec3(0.f, 1.f, 0.f), posB - posA);
	}
};

struct cylinder_mesh_desc
{
	vec3 center = 0.f;
	float height = 1.f;
	float radius = 0.4f;
	quat rotation = quat::identity;
	uint32 slices = 15;
};

struct hollow_cylinder_mesh_desc
{
	vec3 center = 0.f;
	float height = 1.f;
	float radius = 0.4f;
	float innerRadius = 0.3f;
	quat rotation = quat::identity;
	uint32 slices = 15;
};

struct arrow_mesh_desc
{
	vec3 base = 0.f;
	float shaftLength = 1.f;
	float shaftRadius = 0.1f;
	float headLength = 0.3f;
	float headRadius = 0.2f;
	quat rotation = quat::identity;
	uint32 slices = 15;
};

struct torus_mesh_desc
{
	vec3 center = 0.f;
	float torusRadius = 1.f;
	float tubeRadius = 0.1f;
	quat rotation = quat::identity;
	uint32 slices = 15;
	uint32 segments = 15;
};

struct mace_mesh_desc
{
	vec3 base = 0.f;
	float shaftLength = 1.f;
	float shaftRadius = 0.1f;
	float headLength = 0.3f;
	float headRadius = 0.2f;
	quat rotation = quat::identity;
	uint32 slices = 15;
};

struct mesh_builder
{
	mesh_builder(uint32 vertexFlags = mesh_creation_flags_default, mesh_index_type indexType = mesh_index_uint16);
	mesh_builder(const mesh_builder& mesh) = delete;
	mesh_builder(mesh_builder&& mesh) = default;
	~mesh_builder();


	void pushQuad(const quad_mesh_desc& desc, bool flipWindingOrder = false);
	void pushBox(const box_mesh_desc& desc, bool flipWindingOrder = false);
	void pushTesselatedBox(const tesselated_box_mesh_desc& desc, bool flipWindingOrder = false);
	void pushSphere(const sphere_mesh_desc& desc, bool flipWindingOrder = false);
	void pushIcoSphere(const icosphere_mesh_desc& desc, bool flipWindingOrder = false);
	void pushCapsule(const capsule_mesh_desc& desc, bool flipWindingOrder = false);
	void pushCylinder(const cylinder_mesh_desc& desc, bool flipWindingOrder = false);
	void pushHollowCylinder(const hollow_cylinder_mesh_desc& desc, bool flipWindingOrder = false);
	void pushArrow(const arrow_mesh_desc& desc, bool flipWindingOrder = false);
	void pushTorus(const torus_mesh_desc& desc, bool flipWindingOrder = false);
	void pushMace(const mace_mesh_desc& desc, bool flipWindingOrder = false);

	void pushAssimpMesh(const struct aiMesh* mesh, float scale, bounding_box* aabb = 0, animation_skeleton* skeleton = 0);


	submesh_info endSubmesh();

	dx_mesh createDXMesh();

	vec3* getPositions() { return (vec3*)positionArena.base(); }
	void* getOthers() { return positionArena.base(); }
	void* getTriangles() { return indexArena.base(); }

	uint32 getNumVertices() { return totalNumVertices; }
	uint32 getNumTriangles() { return totalNumTriangles; }

private:

	std::tuple<vec3*, uint8*, uint8*, uint32> beginPrimitive(uint32 numVertices, uint32 numTriangles);

	memory_arena positionArena;
	memory_arena othersArena;
	memory_arena indexArena;

	uint32 vertexFlags;
	mesh_index_type indexType;
	uint32 othersSize;
	uint32 indexSize;
	uint32 skinOffset;

	uint32 totalNumVertices = 0;
	uint32 totalNumTriangles = 0;

	uint32 numVerticesInCurrentSubmesh = 0;
	uint32 numTrianglesInCurrentSubmesh = 0;

	uint32 numSubmeshes = 0;

};


struct vertex_uv_normal_tangent
{
	vec2 uv;
	vec3 normal;
	vec3 tangent;
};

static D3D12_INPUT_ELEMENT_DESC inputLayout_position[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static D3D12_INPUT_ELEMENT_DESC inputLayout_position_uv[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static D3D12_INPUT_ELEMENT_DESC inputLayout_position_normal[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static D3D12_INPUT_ELEMENT_DESC inputLayout_position_uv_normal[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static D3D12_INPUT_ELEMENT_DESC inputLayout_position_uv_normal_tangent[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

