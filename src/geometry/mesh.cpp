#include "pch.h"
#include "mesh.h"
#include "mesh_builder.h"
#include "rendering/pbr.h"
#include "core/hash.h"
#include "core/file_registry.h"

#include "core/assimp.h"


static void getMeshNamesAndTransforms(const aiNode* node, ref<composite_mesh>& mesh, const mat4& parentTransform = mat4::identity)
{
	mat4 transform = parentTransform * readAssimpMatrix(node->mTransformation);
	for (uint32 i = 0; i < node->mNumMeshes; ++i)
	{
		uint32 meshIndex = node->mMeshes[i];
		auto& submesh = mesh->submeshes[meshIndex];
		submesh.name = node->mName.C_Str();
		submesh.transform = mat4ToTRS(transform);
	}

	for (uint32 i = 0; i < node->mNumChildren; ++i)
	{
		getMeshNamesAndTransforms(node->mChildren[i], mesh, transform);
	}
}

static std::unordered_map<asset_handle, weakref<composite_mesh>> meshCache; // TODO: Pack flags into key.
static std::mutex mutex;

static ref<composite_mesh> loadMeshFromFileInternal(asset_handle handle, const fs::path& sceneFilename, uint32 flags)
{
	Assimp::Importer importer;

	const aiScene* scene = loadAssimpSceneFile(sceneFilename, importer);

	if (!scene)
	{
		return 0;
	}

	mesh_builder builder(flags);

	ref<composite_mesh> result = make_ref<composite_mesh>();

	if (flags & mesh_creation_flags_with_skin)
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

		// Load animations.
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
		builder.pushAssimpMesh(mesh, 1.f, &sub.aabb, (flags & mesh_creation_flags_with_skin) ? &result->skeleton : 0);
		sub.info = builder.endSubmesh();
		sub.material = scene->HasMaterials() ? loadAssimpMaterial(scene, sceneFilename, scene->mMaterials[mesh->mMaterialIndex]) : getDefaultPBRMaterial();

		result->aabb.grow(sub.aabb.minCorner);
		result->aabb.grow(sub.aabb.maxCorner);
	}

	result->mesh = builder.createDXMesh();

	result->handle = handle;
	result->flags = flags;
	return result;
}

ref<composite_mesh> loadMeshFromFile(const fs::path& sceneFilename, uint32 flags)
{
	asset_handle handle = getAssetHandleFromPath(sceneFilename.lexically_normal());

	mutex.lock();

	auto sp = meshCache[handle].lock();
	if (!sp)
	{
		meshCache[handle] = sp = loadMeshFromFileInternal(handle, sceneFilename, flags);
	}

	mutex.unlock();
	return sp;
}

ref<composite_mesh> loadMeshFromHandle(asset_handle handle, uint32 flags)
{
	fs::path sceneFilename = getPathFromAssetHandle(handle);

	mutex.lock();

	auto sp = meshCache[handle].lock();
	if (!sp)
	{
		meshCache[handle] = sp = loadMeshFromFileInternal(handle, sceneFilename, flags);
	}

	mutex.unlock();
	return sp;
}
