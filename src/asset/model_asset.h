#pragma once

#include "core/math.h"
#include "animation/animation.h"
#include "geometry/mesh.h"


struct skeleton_asset
{
	std::vector<skeleton_joint> joints;
	std::unordered_map<std::string, uint32> nameToJointID;
};

struct material_asset
{

};

struct submesh_asset
{
	int32 materialIndex;

	std::vector<vec3> positions;
	std::vector<vec2> uvs;
	std::vector<vec3> normals;
	std::vector<skinning_weights> skin;

	std::vector<indexed_triangle16> triangles;
};

struct mesh_asset
{
	std::vector<submesh_asset> submeshes;
	int32 skeletonIndex;
};

struct model_asset
{
	std::vector<mesh_asset> meshes;
	std::vector<material_asset> materials;
	std::vector<skeleton_asset> skeletons;
};



enum mesh_flags
{
	mesh_flag_load_uvs			= (1 << 0),
	mesh_flag_load_normals		= (1 << 1),
	mesh_flag_load_skin			= (1 << 2),
	mesh_flag_load_materials	= (1 << 3),

	mesh_flag_default = mesh_flag_load_uvs | mesh_flag_load_normals | mesh_flag_load_skin | mesh_flag_load_materials,
};


model_asset load3DModelFromFile(const fs::path& path, uint32 meshFlags = mesh_flag_default);

