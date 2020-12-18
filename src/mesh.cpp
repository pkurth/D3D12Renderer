#include "pch.h"
#include "mesh.h"
#include "geometry.h"
#include "pbr.h"

#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <filesystem>

namespace fs = std::filesystem;

#define CACHE_FORMAT "assbin"

static const aiScene* loadAssimpSceneFile(const char* filepathRaw, Assimp::Importer& importer)
{
	fs::path filepath = filepathRaw;
	fs::path extension = filepath.extension();

	fs::path cachedFilename = filepath;
	cachedFilename.replace_extension(".cache." CACHE_FORMAT);

	fs::path cacheFilepath = L"bin_cache" / cachedFilename;

	const aiScene* scene = 0;

	{
		// Look for cached.

		WIN32_FILE_ATTRIBUTE_DATA cachedData;
		if (GetFileAttributesExW(cacheFilepath.c_str(), GetFileExInfoStandard, &cachedData))
		{
			FILETIME cachedFiletime = cachedData.ftLastWriteTime;

			WIN32_FILE_ATTRIBUTE_DATA originalData;
			assert(GetFileAttributesExW(filepath.c_str(), GetFileExInfoStandard, &originalData));
			FILETIME originalFiletime = originalData.ftLastWriteTime;

			if (CompareFileTime(&cachedFiletime, &originalFiletime) >= 0)
			{
				// Cached file is newer than original, so load this.
				scene = importer.ReadFile(cacheFilepath.string(), 0); 
			}
		}
	}

	if (!scene)
	{
		importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.f);
		importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
		importer.SetPropertyInteger(AI_CONFIG_PP_SLM_VERTEX_LIMIT, UINT16_MAX); // So that we can use 16 bit indices.

		uint32 importFlags = aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_OptimizeGraph | aiProcess_FlipUVs;
		uint32 exportFlags = 0;

		scene = importer.ReadFile(filepath.string(), importFlags);

		fs::create_directories(cacheFilepath.parent_path());
		Assimp::Exporter exporter;
#if 0
		uint32 exporterCount = exporter.GetExportFormatCount();
		for (uint32 i = 0; i < exporterCount; ++i)
		{
			const aiExportFormatDesc* desc = exporter.GetExportFormatDescription(i);
			int a = 0;
		}
#endif

		exporter.Export(scene, CACHE_FORMAT, cacheFilepath.string(), exportFlags);
	}

	if (scene)
	{
		scene = importer.GetScene();
	}

	return scene;
}

static ref<pbr_material> loadAssimpMaterial(const aiMaterial* material)
{
	aiString diffuse, normal, roughness, metallic;
	bool hasDiffuse = material->GetTexture(aiTextureType_DIFFUSE, 0, &diffuse) == aiReturn_SUCCESS;
	bool hasNormal = material->GetTexture(aiTextureType_HEIGHT, 0, &normal) == aiReturn_SUCCESS || 
		material->GetTexture(aiTextureType_NORMALS, 0, &normal) == aiReturn_SUCCESS;
	bool hasRoughness = material->GetTexture(aiTextureType_SHININESS, 0, &roughness) == aiReturn_SUCCESS;
	bool hasMetallic = material->GetTexture(aiTextureType_AMBIENT, 0, &metallic) == aiReturn_SUCCESS;

	const char* albedoName = 0;
	const char* normalName = 0;
	const char* roughnessName = 0;
	const char* metallicName = 0;

	aiColor3D aiColor;
	vec4 albedoTint(1.f);
	if (material->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == aiReturn_SUCCESS)
	{
		albedoTint.x = aiColor.r;
		albedoTint.y = aiColor.g;
		albedoTint.z = aiColor.b;
	}

	float roughnessOverride = 1.f;
	float metallicOverride = 0.f;

	if (hasDiffuse) { albedoName = diffuse.C_Str(); }
	if (hasNormal) { normalName = normal.C_Str(); }
	if (hasRoughness) 
	{ 
		roughnessName = roughness.C_Str(); 
	}
	else
	{
		float shininess;
		if (material->Get(AI_MATKEY_SHININESS, shininess) != aiReturn_SUCCESS)
		{
			shininess = 80.f; // Default value.
		}
		roughnessOverride = 1.f - sqrt(shininess * 0.01f);
	}

	if (hasMetallic) 
	{ 
		metallicName = metallic.C_Str(); 
	}
	else
	{
		if (material->Get(AI_MATKEY_REFLECTIVITY, metallicOverride) != aiReturn_SUCCESS)
		{
			metallicOverride = 0.f;
		}
	}

	return createMaterial(albedoName, normalName, roughnessName, metallicName, albedoTint, roughnessOverride, metallicOverride);
}

static mat4 readAssimpMatrix(const aiMatrix4x4& m)
{
	mat4 result;
	result.m00 = m.a1; result.m10 = m.b1; result.m20 = m.c1; result.m30 = m.d1;
	result.m01 = m.a2; result.m11 = m.b2; result.m21 = m.c2; result.m31 = m.d2;
	result.m02 = m.a3; result.m12 = m.b3; result.m22 = m.c3; result.m32 = m.d3;
	result.m03 = m.a4; result.m13 = m.b4; result.m23 = m.c4; result.m33 = m.d4;
	return result;
}

static vec3 readAssimpVector(const aiVectorKey& v)
{
	vec3 result;
	result.x = v.mValue.x;
	result.y = v.mValue.y;
	result.z = v.mValue.z;
	return result;
}

static quat readAssimpQuaternion(const aiQuatKey& q)
{
	quat result;
	result.x = q.mValue.x;
	result.y = q.mValue.y;
	result.z = q.mValue.z;
	result.w = q.mValue.w;
	return result;
}

static void getMeshNamesAndTransforms(const aiNode* node, composite_mesh& mesh, const mat4& parentTransform = mat4::identity)
{
	mat4 transform = parentTransform * readAssimpMatrix(node->mTransformation);
	for (uint32 i = 0; i < node->mNumMeshes; ++i)
	{
		uint32 meshIndex = node->mMeshes[i];
		auto& submesh = mesh.submeshes[meshIndex];
		submesh.name = node->mName.C_Str();
		submesh.transform = transform;
	}

	for (uint32 i = 0; i < node->mNumChildren; ++i)
	{
		getMeshNamesAndTransforms(node->mChildren[i], mesh, transform);
	}
}

static void loadAssimpAnimation(const aiAnimation* animation, animation_clip& clip, animation_skeleton& skeleton, float scale)
{
	clip.name = animation->mName.C_Str();
	clip.ticksPerSecond = (float)animation->mTicksPerSecond;
	clip.length = (float)animation->mDuration / clip.ticksPerSecond;

	clip.joints.resize(skeleton.joints.size());
	clip.positionKeyframes.clear();
	clip.rotationKeyframes.clear();
	clip.scaleKeyframes.clear();

	for (uint32 channelID = 0; channelID < animation->mNumChannels; ++channelID)
	{
		const aiNodeAnim* channel = animation->mChannels[channelID];
		std::string jointName = channel->mNodeName.C_Str();

		auto it = skeleton.nameToJointID.find(jointName);
		if (it != skeleton.nameToJointID.end())
		{
			animation_joint& joint = clip.joints[it->second];

			joint.firstPositionKeyframe = (uint32)clip.positionKeyframes.size();
			joint.firstRotationKeyframe = (uint32)clip.rotationKeyframes.size();
			joint.firstScaleKeyframe = (uint32)clip.scaleKeyframes.size();

			joint.numPositionKeyframes = channel->mNumPositionKeys;
			joint.numRotationKeyframes = channel->mNumRotationKeys;
			joint.numScaleKeyframes = channel->mNumScalingKeys;


			for (uint32 keyID = 0; keyID < channel->mNumPositionKeys; ++keyID)
			{
				clip.positionKeyframes.push_back(readAssimpVector(channel->mPositionKeys[keyID]));
			}

			for (uint32 keyID = 0; keyID < channel->mNumRotationKeys; ++keyID)
			{
				clip.rotationKeyframes.push_back(readAssimpQuaternion(channel->mRotationKeys[keyID]));
			}

			for (uint32 keyID = 0; keyID < channel->mNumScalingKeys; ++keyID)
			{
				clip.scaleKeyframes.push_back(readAssimpVector(channel->mScalingKeys[keyID]));
			}

			joint.isAnimated = true;
		}
	}

	for (uint32 i = 0; i < (uint32)skeleton.joints.size(); ++i)
	{
		if (skeleton.joints[i].parentID == NO_PARENT)
		{
			for (uint32 keyID = 0; keyID < clip.joints[i].numPositionKeyframes; ++keyID)
			{
				clip.positionKeyframes[clip.joints[i].firstPositionKeyframe + keyID] *= scale;
			}
			for (uint32 keyID = 0; keyID < clip.joints[i].numScaleKeyframes; ++keyID)
			{
				clip.scaleKeyframes[clip.joints[i].firstScaleKeyframe + keyID] *= scale;
			}
		}
	}
}

static void readAssimpSkeletonHierarchy(const aiNode* node, animation_skeleton& skeleton, uint32& insertIndex, uint32 parentID = NO_PARENT)
{
	std::string name = node->mName.C_Str();

	auto it = skeleton.nameToJointID.find(name);
	if (it != skeleton.nameToJointID.end())
	{
		uint32 jointID = it->second;

		skeleton.joints[jointID].parentID = parentID;

		// This sorts the joints, such that parents are before their children.
		skeleton.nameToJointID[name] = insertIndex;
		skeleton.nameToJointID[skeleton.joints[insertIndex].name] = jointID;
		std::swap(skeleton.joints[jointID], skeleton.joints[insertIndex]);

		parentID = insertIndex;

		++insertIndex;
	}

	for (uint32 i = 0; i < node->mNumChildren; ++i)
	{
		readAssimpSkeletonHierarchy(node->mChildren[i], skeleton, insertIndex, parentID);
	}
}

static void loadAssimpSkeleton(const aiScene* scene, animation_skeleton& skeleton, float scale = 1.f)
{
	mat4 scaleMatrix = mat4::identity * (1.f / scale);
	scaleMatrix.m33 = 1.f;

	for (uint32 meshID = 0; meshID < scene->mNumMeshes; ++meshID)
	{
		const aiMesh* mesh = scene->mMeshes[meshID];

		for (uint32 boneID = 0; boneID < mesh->mNumBones; ++boneID)
		{
			const aiBone* bone = mesh->mBones[boneID];
			std::string name = bone->mName.C_Str();

			auto it = skeleton.nameToJointID.find(name);
			if (it == skeleton.nameToJointID.end())
			{
				skeleton.nameToJointID[name] = (uint32)skeleton.joints.size();

				skeleton_joint& joint = skeleton.joints.emplace_back();
				joint.name = std::move(name);
				joint.invBindMatrix = readAssimpMatrix(bone->mOffsetMatrix) * scaleMatrix;
				joint.bindTransform = trs(invert(joint.invBindMatrix));
			}
		}
	}

	uint32 insertIndex = 0;
	readAssimpSkeletonHierarchy(scene->mRootNode, skeleton, insertIndex);
}

static composite_mesh loadMeshFromScene(const aiScene* scene, uint32 flags)
{
	cpu_mesh cpuMesh(flags);

	composite_mesh result;

	if (flags & mesh_creation_flags_with_skin)
	{
		loadAssimpSkeleton(scene, result.skeleton, 1.f);

		result.skeleton.clips.resize(scene->mNumAnimations);
		for (uint32 i = 0; i < scene->mNumAnimations; ++i)
		{
			loadAssimpAnimation(scene->mAnimations[i], result.skeleton.clips[i], result.skeleton, 1.f);
			result.skeleton.nameToClipID[result.skeleton.clips[i].name] = i;
		}
	}

	result.submeshes.resize(scene->mNumMeshes);
	getMeshNamesAndTransforms(scene->mRootNode, result);

	for (uint32 m = 0; m < scene->mNumMeshes; ++m)
	{
		submesh& sub = result.submeshes[m];

		aiMesh* mesh = scene->mMeshes[m];
		sub.info = cpuMesh.pushAssimpMesh(mesh, 1.f, &sub.aabb, (flags & mesh_creation_flags_with_skin) ? &result.skeleton : 0);
		sub.material = scene->HasMaterials() ? loadAssimpMaterial(scene->mMaterials[mesh->mMaterialIndex]) : getDefaultMaterial();
	}

	result.mesh = cpuMesh.createDXMesh();

	return result;
}

composite_mesh loadMeshFromFile(const char* sceneFilename, uint32 flags)
{
	Assimp::Importer importer;

	const aiScene* scene = loadAssimpSceneFile(sceneFilename, importer);
	return loadMeshFromScene(scene, flags);
}