#include "pch.h"
#include "mesh.h"
#include "mesh_builder.h"
#include "rendering/pbr.h"
#include "core/hash.h"
#include "core/file_registry.h"
#include "asset/model_asset.h"

#include "core/assimp.h"


static void getMeshNamesAndTransforms(const aiNode* node, ref<multi_mesh>& mesh, const mat4& parentTransform = mat4::identity)
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

static std::unordered_map<asset_handle, weakref<multi_mesh>> meshCache; // TODO: Pack flags into key.
static std::mutex mutex;

static ref<multi_mesh> loadMeshFromFileInternal(asset_handle handle, const fs::path& sceneFilename, uint32 flags, mesh_load_callback cb)
{
#if 0
	Assimp::Importer importer;

	bool allow16BitIndices = !(flags & mesh_creation_flags_with_skin); // Currently, Assimp fails to split skinned meshes: https://github.com/assimp/assimp/issues/3576
	const aiScene* scene = loadAssimpSceneFile(sceneFilename, importer, allow16BitIndices);

	if (!scene)
	{
		return 0;
	}

	mesh_index_type indexType = allow16BitIndices ? mesh_index_uint16 : mesh_index_uint32;

	if (indexType == mesh_index_uint16)
	{
		for (uint32 m = 0; m < scene->mNumMeshes; ++m)
		{
			aiMesh* mesh = scene->mMeshes[m];
			ASSERT(mesh->mNumVertices <= UINT16_MAX);
		}
	}

	mesh_builder builder(flags, indexType);

	ref<multi_mesh> result = make_ref<multi_mesh>();

	if (flags & mesh_creation_flags_with_skin)
	{
		result->skeleton.loadFromAssimp(scene, 1.f);

#if 0
		result->skeleton.prettyPrintHierarchy();

		for (uint32 i = 0; i < (uint32)result->skeleton.joints.size(); ++i)
		{
			auto& joint = result->skeleton.joints[i];

			auto it = result->skeleton.nameToJointID.find(joint.name);
			ASSERT(it != result->skeleton.nameToJointID.end());
			ASSERT(it->second == i);
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

	if (flags & mesh_creation_flags_with_skin)
	{
		result->skeleton.analyzeJoints(builder.getPositions(), (uint8*)builder.getOthers() + builder.getSkinOffset(), builder.getOthersSize(), builder.getNumVertices());
	}

#else
	ref<multi_mesh> result = make_ref<multi_mesh>();

	result->aabb = bounding_box::negativeInfinity();

	model_asset asset = load3DModelFromFile(sceneFilename);
	mesh_builder builder(flags);
	for (auto& mesh : asset.meshes)
	{
		for (auto& sub : mesh.submeshes)
		{
			const pbr_material_desc& materialDesc = asset.materials[sub.materialIndex];
			auto material = createPBRMaterial(materialDesc);

			bounding_box aabb;
			builder.pushMesh(sub, 1.f, &aabb);
			result->submeshes.push_back({ builder.endSubmesh(), aabb, trs::identity, material, mesh.name });

			result->aabb.grow(aabb.minCorner);
			result->aabb.grow(aabb.maxCorner);
		}
	}


	animation_skeleton& skeleton = result->skeleton;

	// Load skeleton.
	if (!asset.skeletons.empty() && flags & mesh_creation_flags_with_skin)
	{
		skeleton_asset& in = asset.skeletons.front();

		skeleton.joints = std::move(in.joints);
		skeleton.nameToJointID = std::move(in.nameToJointID);
		skeleton.analyzeJoints(builder.getPositions(), (uint8*)builder.getOthers() + builder.getSkinOffset(), builder.getOthersSize(), builder.getNumVertices());
	}

	// Load animations.
	if (!asset.animations.empty())
	{
		animation_asset& in = asset.animations.front();

		animation_clip& clip = skeleton.clips.emplace_back();
		clip.name = std::move(in.name);
		clip.filename = sceneFilename;
		clip.lengthInSeconds = in.duration;
		clip.joints.resize(skeleton.joints.size(), {});

		clip.positionKeyframes = std::move(in.positionKeyframes);
		clip.positionTimestamps = std::move(in.positionTimestamps);
		clip.rotationKeyframes = std::move(in.rotationKeyframes);
		clip.rotationTimestamps = std::move(in.rotationTimestamps);
		clip.scaleKeyframes = std::move(in.scaleKeyframes);
		clip.scaleTimestamps = std::move(in.scaleTimestamps);

		for (auto [name, joint] : in.joints)
		{
			auto it = skeleton.nameToJointID.find(name);
			if (it != skeleton.nameToJointID.end())
			{
				animation_joint& j = clip.joints[it->second];
				j = joint;
			}
		}
	}

#endif

	if (cb)
	{
		cb(builder, result->submeshes, result->aabb);
	}

	result->mesh = builder.createDXMesh();

	result->handle = handle;
	result->flags = flags;
	return result;
}

static ref<multi_mesh> loadMeshFromFileAndHandle(const fs::path& sceneFilename, asset_handle handle, uint32 flags, mesh_load_callback cb)
{
	mutex.lock();

	auto sp = meshCache[handle].lock();
	if (!sp)
	{
		meshCache[handle] = sp = loadMeshFromFileInternal(handle, sceneFilename, flags, cb);
	}

	mutex.unlock();
	return sp;
}

ref<multi_mesh> loadMeshFromFile(const fs::path& sceneFilename, uint32 flags, mesh_load_callback cb)
{
	asset_handle handle = getAssetHandleFromPath(sceneFilename.lexically_normal());
	return loadMeshFromFileAndHandle(sceneFilename, handle, flags, cb);
}

ref<multi_mesh> loadMeshFromHandle(asset_handle handle, uint32 flags, mesh_load_callback cb)
{
	fs::path sceneFilename = getPathFromAssetHandle(handle);
	return loadMeshFromFileAndHandle(sceneFilename, handle, flags, cb);
}
