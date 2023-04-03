#include "pch.h"
#include "model_asset.h"
#include "io.h"

#include "core/cpu_profiling.h"

//#define PROFILE(name) CPU_PRINT_PROFILE_BLOCK(name)
#define PROFILE(name) 

static const uint32 BIN_HEADER = 'BIN ';

struct bin_header
{
	uint32 header = BIN_HEADER;
	uint32 version = 1;
	uint32 flags;
	uint32 numMeshes;
	uint32 numMaterials;
	uint32 numSkeletons;
	uint32 numAnimations;
};

struct bin_mesh_header
{
	uint32 numSubmeshes;
	int32 skeletonIndex;
	uint32 nameLength;
};

enum bin_submesh_flag
{
	bin_submesh_flag_positions = (1 << 0),
	bin_submesh_flag_uvs = (1 << 1),
	bin_submesh_flag_normals = (1 << 2),
	bin_submesh_flag_tangents = (1 << 3),
	bin_submesh_flag_colors = (1 << 4),
	bin_submesh_flag_skin = (1 << 5),
};

struct bin_submesh_header
{
	uint32 numVertices;
	uint32 numTriangles;
	int32 materialIndex;
	uint32 flags;
};

struct bin_material_header
{
	uint32 albedoPathLength;
	uint32 normalPathLength;
	uint32 roughnessPathLength;
	uint32 metallicPathLength;
};

struct bin_skeleton_header
{
	uint32 numJoints;
};

struct bin_animation_header
{
	float duration;
	uint32 numJoints;
	uint32 numPositionKeyframes;
	uint32 numRotationKeyframes;
	uint32 numScaleKeyframes;

	uint32 nameLength;
};





template <typename T>
static void writeArray(const std::vector<T>& in, FILE* file)
{
	fwrite(in.data(), sizeof(T), in.size(), file);
}

static void writeMesh(const mesh_asset& mesh, FILE* file)
{
	bin_mesh_header header;
	header.skeletonIndex = mesh.skeletonIndex;
	header.numSubmeshes = (uint32)mesh.submeshes.size();
	header.nameLength = (uint32)mesh.name.length();

	fwrite(&header, sizeof(header), 1, file);
	fwrite(mesh.name.c_str(), sizeof(char), header.nameLength, file);

	for (uint32 i = 0; i < header.numSubmeshes; ++i)
	{
		const submesh_asset& in = mesh.submeshes[i];

		bin_submesh_header subHeader;
		subHeader.materialIndex = in.materialIndex;
		subHeader.numVertices = (uint32)in.positions.size();
		subHeader.numTriangles = (uint32)in.triangles.size();

		subHeader.flags = 0;
		if (!in.positions.empty()) { subHeader.flags |= bin_submesh_flag_positions; }
		if (!in.uvs.empty()) { subHeader.flags |= bin_submesh_flag_uvs; }
		if (!in.normals.empty()) { subHeader.flags |= bin_submesh_flag_normals; }
		if (!in.tangents.empty()) { subHeader.flags |= bin_submesh_flag_tangents; }
		if (!in.colors.empty()) { subHeader.flags |= bin_submesh_flag_colors; }
		if (!in.skin.empty()) { subHeader.flags |= bin_submesh_flag_skin; }

		fwrite(&subHeader, sizeof(bin_submesh_header), 1, file);

		if (!in.positions.empty()) { writeArray(in.positions, file); }
		if (!in.uvs.empty()) { writeArray(in.uvs, file); }
		if (!in.normals.empty()) { writeArray(in.normals, file); }
		if (!in.tangents.empty()) { writeArray(in.tangents, file); }
		if (!in.colors.empty()) { writeArray(in.colors, file); }
		if (!in.skin.empty()) { writeArray(in.skin, file); }
		writeArray(in.triangles, file);
	}
}

static void writeMaterial(const pbr_material_desc& material, FILE* file)
{
	std::string albedo = material.albedo.string();
	std::string normal = material.normal.string();
	std::string roughness = material.roughness.string();
	std::string metallic = material.metallic.string();

	bin_material_header header;
	header.albedoPathLength = (uint32)albedo.length();
	header.normalPathLength = (uint32)normal.length();
	header.roughnessPathLength = (uint32)roughness.length();
	header.metallicPathLength = (uint32)metallic.length();

	fwrite(&header, sizeof(header), 1, file);
	fwrite(albedo.c_str(), sizeof(char), header.albedoPathLength, file);
	fwrite(normal.c_str(), sizeof(char), header.normalPathLength, file);
	fwrite(roughness.c_str(), sizeof(char), header.roughnessPathLength, file);
	fwrite(metallic.c_str(), sizeof(char), header.metallicPathLength, file);

	fwrite(&material.albedoFlags, sizeof(uint32), 1, file);
	fwrite(&material.normalFlags, sizeof(uint32), 1, file);
	fwrite(&material.roughnessFlags, sizeof(uint32), 1, file);
	fwrite(&material.metallicFlags, sizeof(uint32), 1, file);

	fwrite(&material.emission, sizeof(vec4), 1, file);
	fwrite(&material.albedoTint, sizeof(vec4), 1, file);
	fwrite(&material.roughnessOverride, sizeof(float), 1, file);
	fwrite(&material.metallicOverride, sizeof(float), 1, file);
	fwrite(&material.shader, sizeof(pbr_material_shader), 1, file);
	fwrite(&material.uvScale, sizeof(float), 1, file);
	fwrite(&material.translucency, sizeof(float), 1, file);
}

static void writeSkeleton(const skeleton_asset& skeleton, FILE* file)
{
	bin_skeleton_header header;
	header.numJoints = (uint32)skeleton.joints.size();

	fwrite(&header, sizeof(header), 1, file);

	for (uint32 i = 0; i < header.numJoints; ++i)
	{
		uint32 nameLength = (uint32)skeleton.joints[i].name.length();
		fwrite(&nameLength, sizeof(uint32), 1, file);
		fwrite(skeleton.joints[i].name.c_str(), sizeof(char), nameLength, file);
		fwrite(&skeleton.joints[i].limbType, sizeof(limb_type), 1, file);
		fwrite(&skeleton.joints[i].ik, sizeof(bool), 1, file);
		fwrite(&skeleton.joints[i].invBindTransform, sizeof(mat4), 1, file);
		fwrite(&skeleton.joints[i].bindTransform, sizeof(mat4), 1, file);
		fwrite(&skeleton.joints[i].parentID, sizeof(uint32), 1, file);
	}
}

static void writeAnimation(const animation_asset& animation, FILE* file)
{
	bin_animation_header header;
	header.duration = animation.duration;
	header.numJoints = (uint32)animation.joints.size();
	header.numPositionKeyframes = (uint32)animation.positionKeyframes.size();
	header.numRotationKeyframes = (uint32)animation.rotationKeyframes.size();
	header.numScaleKeyframes = (uint32)animation.scaleKeyframes.size();
	header.nameLength = (uint32)animation.name.length();

	fwrite(&header, sizeof(header), 1, file);
	fwrite(animation.name.c_str(), sizeof(char), header.nameLength, file);

	for (auto [name, joint] : animation.joints)
	{
		uint32 nameLength = (uint32)name.length();
		fwrite(&nameLength, sizeof(uint32), 1, file);
		fwrite(name.c_str(), sizeof(char), nameLength, file);

		fwrite(&joint, sizeof(animation_joint), 1, file);
	}

	writeArray(animation.positionTimestamps, file);
	writeArray(animation.rotationTimestamps, file);
	writeArray(animation.scaleTimestamps, file);
	writeArray(animation.positionKeyframes, file);
	writeArray(animation.rotationKeyframes, file);
	writeArray(animation.scaleKeyframes, file);
}

void writeBIN(const model_asset& asset, const fs::path& path)
{
	FILE* file = fopen(path.string().c_str(), "wb");

	bin_header header;
	header.flags = asset.flags;
	header.numMeshes = (uint32)asset.meshes.size();
	header.numMaterials = (uint32)asset.materials.size();
	header.numSkeletons = (uint32)asset.skeletons.size();
	header.numAnimations = (uint32)asset.animations.size();

	fwrite(&header, sizeof(header), 1, file);

	for (uint32 i = 0; i < header.numMeshes; ++i)
	{
		writeMesh(asset.meshes[i], file);
	}
	for (uint32 i = 0; i < header.numMaterials; ++i)
	{
		writeMaterial(asset.materials[i], file);
	}
	for (uint32 i = 0; i < header.numSkeletons; ++i)
	{
		writeSkeleton(asset.skeletons[i], file);
	}
	for (uint32 i = 0; i < header.numAnimations; ++i)
	{
		writeAnimation(asset.animations[i], file);
	}

	fclose(file);
}










template <typename T>
static void readArray(entire_file& file, std::vector<T>& out, uint32 count)
{
	out.resize(count);
	T* ptr = file.consume<T>(count);
	memcpy(out.data(), ptr, sizeof(T) * count);
}

static mesh_asset readMesh(entire_file& file)
{
	bin_mesh_header* header = file.consume<bin_mesh_header>();
	char* name = file.consume<char>(header->nameLength);

	mesh_asset result;
	result.name = std::string(name, header->nameLength);
	result.submeshes.resize(header->numSubmeshes);
	result.skeletonIndex = header->skeletonIndex;

	for (uint32 i = 0; i < header->numSubmeshes; ++i)
	{
		bin_submesh_header* subHeader = file.consume<bin_submesh_header>();

		submesh_asset& sub = result.submeshes[i];
		sub.materialIndex = subHeader->materialIndex;
		if (subHeader->flags & bin_submesh_flag_positions) { readArray(file, sub.positions, subHeader->numVertices); }
		if (subHeader->flags & bin_submesh_flag_uvs) { readArray(file, sub.uvs, subHeader->numVertices); }
		if (subHeader->flags & bin_submesh_flag_normals) { readArray(file, sub.normals, subHeader->numVertices); }
		if (subHeader->flags & bin_submesh_flag_tangents) { readArray(file, sub.tangents, subHeader->numVertices); }
		if (subHeader->flags & bin_submesh_flag_colors) { readArray(file, sub.colors, subHeader->numVertices); }
		if (subHeader->flags & bin_submesh_flag_skin) { readArray(file, sub.skin, subHeader->numVertices); }
		readArray(file, sub.triangles, subHeader->numTriangles);
	}

	return result;
}

static pbr_material_desc readMaterial(entire_file& file)
{
	bin_material_header* header = file.consume<bin_material_header>();

	char* albedo = file.consume<char>(header->albedoPathLength);
	char* normal = file.consume<char>(header->normalPathLength);
	char* roughness = file.consume<char>(header->roughnessPathLength);
	char* metallic = file.consume<char>(header->metallicPathLength);

	pbr_material_desc result;
	result.albedo = std::string(albedo, header->albedoPathLength);
	result.normal = std::string(normal, header->normalPathLength);
	result.roughness = std::string(roughness, header->roughnessPathLength);
	result.metallic = std::string(metallic, header->metallicPathLength);

	result.albedoFlags = *file.consume<uint32>();
	result.normalFlags = *file.consume<uint32>();
	result.roughnessFlags = *file.consume<uint32>();
	result.metallicFlags = *file.consume<uint32>();

	result.emission = *file.consume<vec4>();
	result.albedoTint = *file.consume<vec4>();
	result.roughnessOverride = *file.consume<float>();
	result.metallicOverride = *file.consume<float>();
	result.shader = *file.consume<pbr_material_shader>();
	result.uvScale = *file.consume<float>();
	result.translucency = *file.consume<float>();

	return result;
}

static skeleton_asset readSkeleton(entire_file& file)
{
	bin_skeleton_header* header = file.consume<bin_skeleton_header>();
	
	skeleton_asset result;
	result.joints.resize(header->numJoints);
	result.nameToJointID.reserve(header->numJoints);

	for (uint32 i = 0; i < header->numJoints; ++i)
	{
		uint32 nameLength = *file.consume<uint32>();
		char* name = file.consume<char>(nameLength);

		result.joints[i].name = std::string(name, nameLength);
		result.joints[i].limbType = *file.consume<limb_type>();
		result.joints[i].ik = *file.consume<bool>();
		result.joints[i].invBindTransform = *file.consume<mat4>();
		result.joints[i].bindTransform = *file.consume<mat4>();
		result.joints[i].parentID = *file.consume<uint32>();

		result.nameToJointID[result.joints[i].name] = i;
	}

	return result;
}

static animation_asset readAnimation(entire_file& file)
{
	bin_animation_header* header = file.consume<bin_animation_header>();

	animation_asset result;
	result.duration = header->duration;

	char* name = file.consume<char>(header->nameLength);
	result.name = std::string(name, header->nameLength);

	result.joints.reserve(header->numJoints);

	for (uint32 i = 0; i < header->numJoints; ++i)
	{
		uint32 nameLength = *file.consume<uint32>();
		char* name = file.consume<char>(nameLength);

		animation_joint joint = *file.consume<animation_joint>();
		result.joints[std::string(name, nameLength)] = joint;
	}

	readArray(file, result.positionTimestamps, header->numPositionKeyframes);
	readArray(file, result.rotationTimestamps, header->numRotationKeyframes);
	readArray(file, result.scaleTimestamps, header->numScaleKeyframes);
	readArray(file, result.positionKeyframes, header->numPositionKeyframes);
	readArray(file, result.rotationKeyframes, header->numRotationKeyframes);
	readArray(file, result.scaleKeyframes, header->numScaleKeyframes);


	return result;
}

model_asset loadBIN(const fs::path& path)
{
	PROFILE("Loading BIN");

	entire_file file = loadFile(path);

	bin_header* header = file.consume<bin_header>();
	if (header->header != BIN_HEADER)
	{
		freeFile(file);
		return {};
	}

	model_asset result;
	result.meshes.resize(header->numMeshes);
	result.materials.resize(header->numMaterials);
	result.skeletons.resize(header->numSkeletons);
	result.animations.resize(header->numAnimations);

	for (uint32 i = 0; i < header->numMeshes; ++i)
	{
		result.meshes[i] = readMesh(file);
	}
	for (uint32 i = 0; i < header->numMaterials; ++i)
	{
		result.materials[i] = readMaterial(file);
	}
	for (uint32 i = 0; i < header->numSkeletons; ++i)
	{
		result.skeletons[i] = readSkeleton(file);
	}
	for (uint32 i = 0; i < header->numAnimations; ++i)
	{
		result.animations[i] = readAnimation(file);
	}

	freeFile(file);

	return result;
}
