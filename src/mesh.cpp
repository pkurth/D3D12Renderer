#include "pch.h"
#include "mesh.h"
#include "geometry.h"
#include "pbr.h"

#include "assimp.h"


static void getMeshNamesAndTransforms(const aiNode* node, ref<composite_mesh>& mesh, const mat4& parentTransform = mat4::identity)
{
	mat4 transform = parentTransform * readAssimpMatrix(node->mTransformation);
	for (uint32 i = 0; i < node->mNumMeshes; ++i)
	{
		uint32 meshIndex = node->mMeshes[i];
		auto& submesh = mesh->submeshes[meshIndex];
		submesh.name = node->mName.C_Str();
		submesh.transform = transform;
	}

	for (uint32 i = 0; i < node->mNumChildren; ++i)
	{
		getMeshNamesAndTransforms(node->mChildren[i], mesh, transform);
	}
}

ref<composite_mesh> loadMeshFromFile(const char* sceneFilename, uint32 flags)
{
	Assimp::Importer importer;

	const aiScene* scene = loadAssimpSceneFile(sceneFilename, importer);

	cpu_mesh cpuMesh(flags);

	ref<composite_mesh> result = make_ref<composite_mesh>();

	if (flags & mesh_creation_flags_with_skin)
	{
		result->skeleton.loadFromAssimp(scene, 1.f);

		for (uint32 i = 0; i < scene->mNumAnimations; ++i)
		{
			result->skeleton.pushAssimpAnimation(sceneFilename, scene->mAnimations[i], 1.f);
		}
	}

	result->submeshes.resize(scene->mNumMeshes);
	getMeshNamesAndTransforms(scene->mRootNode, result);

	result->aabb = bounding_box::negativeInfinity();

	for (uint32 m = 0; m < scene->mNumMeshes; ++m)
	{
		submesh& sub = result->submeshes[m];

		aiMesh* mesh = scene->mMeshes[m];
		sub.info = cpuMesh.pushAssimpMesh(mesh, 1.f, &sub.aabb, (flags & mesh_creation_flags_with_skin) ? &result->skeleton : 0);
		sub.material = scene->HasMaterials() ? loadAssimpMaterial(scene->mMaterials[mesh->mMaterialIndex]) : getDefaultPBRMaterial();

		result->aabb.grow(sub.aabb.minCorner);
		result->aabb.grow(sub.aabb.maxCorner);
	}

	result->mesh = cpuMesh.createDXMesh();

	return result;
	result->filepath = sceneFilename;
	result->flags = flags;
	return result;
}