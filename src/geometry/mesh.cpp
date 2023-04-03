#include "pch.h"
#include "mesh.h"
#include "mesh_builder.h"
#include "rendering/pbr.h"
#include "core/hash.h"
#include "asset/file_registry.h"
#include "asset/model_asset.h"


static std::unordered_map<asset_handle, weakref<multi_mesh>> meshCache; // TODO: Pack flags into key.
static std::mutex mutex;

static ref<multi_mesh> loadMeshFromFileInternal(asset_handle handle, const fs::path& sceneFilename, uint32 flags, mesh_load_callback cb)
{
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
		fs::path path = sceneFilename.lexically_normal().make_preferred();
		meshCache[handle] = sp = loadMeshFromFileInternal(handle, path, flags, cb);
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
