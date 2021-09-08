#include "pch.h"
#include "mesh.h"
#include "geometry.h"
#include "rendering/pbr.h"
#include "core/hash.h"

#include "core/assimp.h"


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

static std::unordered_map<fs::path, weakref<composite_mesh>> meshCache; // TODO: Pack flags into key.
static std::mutex mutex;

static ref<composite_mesh> loadMeshFromFileInternal(const fs::path& sceneFilename, bool loadSkeleton, bool loadAnimations, uint32 flags)
{
	Assimp::Importer importer;

	const aiScene* scene = loadAssimpSceneFile(sceneFilename, importer);

	if (!scene)
	{
		return 0;
	}

	cpu_mesh cpuMesh(flags);

	ref<composite_mesh> result = make_ref<composite_mesh>();

	if (loadSkeleton && (flags & mesh_creation_flags_with_skin))
	{
		result->skeleton.loadFromAssimp(scene, 1.f);

#if 0
		result->skeleton.prettyPrintHierarchy();

		for (uint32 i = 0; i < (uint32)result->skeleton.joints.size(); ++i)
		{
			auto& joint = result->skeleton.joints[i];

			auto it = result->skeleton.nameToJointID.find(joint.name);
			assert(it != result->skeleton.nameToJointID.end());
			assert(it->second == i);
		}
#endif

		if (loadAnimations)
		{
			for (uint32 i = 0; i < scene->mNumAnimations; ++i)
			{
				result->skeleton.pushAssimpAnimation(sceneFilename, scene->mAnimations[i], 1.f);
			}
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
		sub.material = scene->HasMaterials() ? loadAssimpMaterial(scene, sceneFilename, scene->mMaterials[mesh->mMaterialIndex]) : getDefaultPBRMaterial();

		result->aabb.grow(sub.aabb.minCorner);
		result->aabb.grow(sub.aabb.maxCorner);
	}

	result->mesh = cpuMesh.createDXMesh();

	result->filepath = sceneFilename;
	result->flags = flags;
	return result;
}

ref<composite_mesh> loadMeshFromFile(const fs::path& sceneFilename, bool loadSkeleton, bool loadAnimations, uint32 flags)
{
	mutex.lock();

	auto sp = meshCache[sceneFilename].lock();
	if (!sp)
	{
		meshCache[sceneFilename] = sp = loadMeshFromFileInternal(sceneFilename, loadSkeleton, loadAnimations, flags);
	}

	mutex.unlock();
	return sp;
}