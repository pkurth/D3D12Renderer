#pragma once

#include "core/math.h"
#include "animation/animation.h"
#include "geometry/mesh.h"
#include "rendering/pbr_material.h"


struct skeleton_asset
{
	std::vector<skeleton_joint> joints;
	std::unordered_map<std::string, uint32> nameToJointID;
};

struct animation_asset
{
	std::unordered_map<std::string, animation_joint> joints;

	std::vector<float> positionTimestamps;
	std::vector<float> rotationTimestamps;
	std::vector<float> scaleTimestamps;

	std::vector<vec3> positionKeyframes;
	std::vector<quat> rotationKeyframes;
	std::vector<vec3> scaleKeyframes;

	float duration;
};

struct material_asset
{
	std::string albedo;
	std::string normal;
	std::string roughness;
	std::string metallic;

	vec4 emission;
	vec4 albedoTint;
	float roughnessOverride;
	float metallicOverride;
	pbr_material_shader shader;
	float uvScale;
	float translucency;
};

struct submesh_asset
{
	int32 materialIndex;

	std::vector<vec3> positions;
	std::vector<vec2> uvs;
	std::vector<vec3> normals;
	std::vector<vec3> tangents;
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
	std::vector<animation_asset> animations;
};



enum mesh_flags
{
	mesh_flag_load_uvs				= (1 << 0),
	mesh_flag_flip_uvs_vertically	= (1 << 1),
	mesh_flag_load_normals			= (1 << 2),
	mesh_flag_load_tangents			= (1 << 3),
	mesh_flag_gen_normals			= (1 << 4), // Only if mesh has no normals.
	mesh_flag_gen_tangents			= (1 << 5), // Only if mesh has no tangents.
	mesh_flag_load_skin				= (1 << 6),

	mesh_flag_default = mesh_flag_load_uvs | mesh_flag_flip_uvs_vertically | 
		mesh_flag_load_normals | mesh_flag_gen_normals | 
		mesh_flag_load_tangents | mesh_flag_gen_tangents |
		mesh_flag_load_skin,
};


model_asset load3DModelFromFile(const fs::path& path, uint32 meshFlags = mesh_flag_default);

