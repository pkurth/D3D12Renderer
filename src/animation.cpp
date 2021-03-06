#include "pch.h"
#include "animation.h"

#include "assimp.h"

#include <filesystem>
namespace fs = std::filesystem;

void animation_skeleton::pushAssimpAnimation(const std::string& suffix, const aiAnimation* animation, float scale)
{
	animation_clip& clip = clips.emplace_back();

	clip.name = std::string(animation->mName.C_Str()) + std::string("(") + suffix + ")";
	clip.lengthInSeconds = (float)animation->mDuration * 0.001f;

	float timeNormalization = 1.f / (float)animation->mTicksPerSecond;

	clip.joints.resize(joints.size());
	clip.positionKeyframes.clear();
	clip.rotationKeyframes.clear();
	clip.scaleKeyframes.clear();

	for (uint32 channelID = 0; channelID < animation->mNumChannels; ++channelID)
	{
		const aiNodeAnim* channel = animation->mChannels[channelID];
		std::string jointName = channel->mNodeName.C_Str();

		auto it = nameToJointID.find(jointName);
		if (it != nameToJointID.end())
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
				clip.positionKeyframes.push_back(readAssimpVector(channel->mPositionKeys[keyID].mValue));
				clip.positionTimestamps.push_back((float)channel->mPositionKeys[keyID].mTime * 0.001f);
			}

			for (uint32 keyID = 0; keyID < channel->mNumRotationKeys; ++keyID)
			{
				clip.rotationKeyframes.push_back(readAssimpQuaternion(channel->mRotationKeys[keyID].mValue));
				clip.rotationTimestamps.push_back((float)channel->mRotationKeys[keyID].mTime * 0.001f);
			}

			for (uint32 keyID = 0; keyID < channel->mNumScalingKeys; ++keyID)
			{
				clip.scaleKeyframes.push_back(readAssimpVector(channel->mScalingKeys[keyID].mValue));
				clip.scaleTimestamps.push_back((float)channel->mScalingKeys[keyID].mTime * 0.001f);
			}

			joint.isAnimated = true;
		}
	}

	for (uint32 i = 0; i < (uint32)joints.size(); ++i)
	{
		if (joints[i].parentID == NO_PARENT)
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

	nameToClipID[clip.name] = (uint32)clips.size() - 1;
}

void animation_skeleton::pushAssimpAnimations(const std::string& sceneFilename, float scale)
{
	Assimp::Importer importer;

	const aiScene* scene = loadAssimpSceneFile(sceneFilename, importer);

	if (scene)
	{
		for (uint32 i = 0; i < scene->mNumAnimations; ++i)
		{
			pushAssimpAnimation(sceneFilename, scene->mAnimations[i], scale);
		}

		files.push_back(sceneFilename);
	}
}

void animation_skeleton::pushAssimpAnimationsInDirectory(const std::string& directory, float scale)
{
	for (auto& p : fs::directory_iterator(directory))
	{
		pushAssimpAnimations(p.path().string().c_str());
	}
}

static void readAssimpSkeletonHierarchy(const aiNode* node, animation_skeleton& skeleton, uint32& insertIndex, uint32 parentID = NO_PARENT)
{
	std::string name = node->mName.C_Str();

	if (name == "Animation") // TODO: Temporary fix for the pilot.fbx mesh.
	{
		return;
	}

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

void animation_skeleton::loadFromAssimp(const aiScene* scene, float scale)
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

			auto it = nameToJointID.find(name);
			if (it == nameToJointID.end())
			{
				nameToJointID[name] = (uint32)joints.size();

				skeleton_joint& joint = joints.emplace_back();
				joint.name = std::move(name);
				joint.invBindMatrix = readAssimpMatrix(bone->mOffsetMatrix) * scaleMatrix;
				joint.bindTransform = trs(invert(joint.invBindMatrix));
			}
#if 0
			else
			{
				mat4 invBind = readAssimpMatrix(bone->mOffsetMatrix) * scaleMatrix;
				assert(invBind == joints[it->second].invBindMatrix);
			}
#endif
		}
	}

	uint32 insertIndex = 0;
	readAssimpSkeletonHierarchy(scene->mRootNode, *this, insertIndex);
}

static vec3 samplePosition(const animation_clip& clip, const animation_joint& animJoint, float time)
{
	if (animJoint.numPositionKeyframes == 1)
	{
		return clip.positionKeyframes[animJoint.firstPositionKeyframe];
	}

	uint32 firstKeyframeIndex = -1;
	for (uint32 i = 0; i < animJoint.numPositionKeyframes - 1; ++i)
	{
		uint32 j = i + animJoint.firstPositionKeyframe;
		if (time < clip.positionTimestamps[j + 1])
		{
			firstKeyframeIndex = j;
			break;
		}
	}
	assert(firstKeyframeIndex != -1);

	uint32 secondKeyframeIndex = firstKeyframeIndex + 1;

	float t = inverseLerp(clip.positionTimestamps[firstKeyframeIndex], clip.positionTimestamps[secondKeyframeIndex], time);

	vec3 a = clip.positionKeyframes[firstKeyframeIndex];
	vec3 b = clip.positionKeyframes[secondKeyframeIndex];

	return lerp(a, b, t);
}

static quat sampleRotation(const animation_clip& clip, const animation_joint& animJoint, float time)
{
	if (animJoint.numRotationKeyframes == 1)
	{
		return clip.rotationKeyframes[animJoint.firstRotationKeyframe];
	}

	uint32 firstKeyframeIndex = -1;
	for (uint32 i = 0; i < animJoint.numRotationKeyframes - 1; ++i)
	{
		uint32 j = i + animJoint.firstRotationKeyframe;
		if (time < clip.rotationTimestamps[j + 1])
		{
			firstKeyframeIndex = j;
			break;
		}
	}
	assert(firstKeyframeIndex != -1);

	uint32 secondKeyframeIndex = firstKeyframeIndex + 1;

	float t = inverseLerp(clip.rotationTimestamps[firstKeyframeIndex], clip.rotationTimestamps[secondKeyframeIndex], time);

	quat a = clip.rotationKeyframes[firstKeyframeIndex];
	quat b = clip.rotationKeyframes[secondKeyframeIndex];

	if (dot(a.v4, b.v4) < 0.f)
	{
		b.v4 *= -1.f;
	}

	return lerp(a, b, t);
}

static vec3 sampleScale(const animation_clip& clip, const animation_joint& animJoint, float time)
{
	if (animJoint.numScaleKeyframes == 1)
	{
		return clip.scaleKeyframes[animJoint.firstScaleKeyframe];
	}

	uint32 firstKeyframeIndex = -1;
	for (uint32 i = 0; i < animJoint.numScaleKeyframes - 1; ++i)
	{
		uint32 j = i + animJoint.firstScaleKeyframe;
		if (time < clip.scaleTimestamps[j + 1])
		{
			firstKeyframeIndex = j;
			break;
		}
	}
	assert(firstKeyframeIndex != -1);

	uint32 secondKeyframeIndex = firstKeyframeIndex + 1;

	float t = inverseLerp(clip.scaleTimestamps[firstKeyframeIndex], clip.scaleTimestamps[secondKeyframeIndex], time);

	vec3 a = clip.scaleKeyframes[firstKeyframeIndex];
	vec3 b = clip.scaleKeyframes[secondKeyframeIndex];

	return lerp(a, b, t);
}

void animation_skeleton::sampleAnimation(const std::string& name, float time, trs* outLocalTransforms) const
{
	auto clipIndexIt = nameToClipID.find(name);
	assert(clipIndexIt != nameToClipID.end());

	const animation_clip& clip = clips[clipIndexIt->second];
	assert(clip.joints.size() == joints.size());

	time = fmod(time, clip.lengthInSeconds);

	uint32 numJoints = (uint32)joints.size();
	for (uint32 i = 0; i < numJoints; ++i)
	{
		const animation_joint& animJoint = clip.joints[i];

		if (animJoint.isAnimated)
		{
			outLocalTransforms[i].position = samplePosition(clip, animJoint, time);
			outLocalTransforms[i].rotation = sampleRotation(clip, animJoint, time);
			outLocalTransforms[i].scale = sampleScale(clip, animJoint, time);
		}
		else
		{
			outLocalTransforms[i] = trs::identity;
		}
	}
}

void animation_skeleton::getSkinningMatricesFromLocalTransforms(const trs* localTransforms, mat4* outSkinningMatrices, const trs& worldTransform) const
{
	uint32 numJoints = (uint32)joints.size();
	trs* globalTransforms = (trs*)alloca(sizeof(trs) * numJoints);

	for (uint32 i = 0; i < numJoints; ++i)
	{
		const skeleton_joint& skelJoint = joints[i];
		if (skelJoint.parentID != NO_PARENT)
		{
			assert(i > skelJoint.parentID); // Parent already processed.
			globalTransforms[i] = globalTransforms[skelJoint.parentID] * localTransforms[i];
		}
		else
		{
			globalTransforms[i] = worldTransform * localTransforms[i];
		}

		outSkinningMatrices[i] = trsToMat4(globalTransforms[i]) * joints[i].invBindMatrix;
	}
}

static void prettyPrint(const animation_skeleton& skeleton, uint32 parent, uint32 indent)
{
	for (uint32 i = 0; i < (uint32)skeleton.joints.size(); ++i)
	{
		if (skeleton.joints[i].parentID == parent)
		{
			std::cout << std::string(indent, ' ') << skeleton.joints[i].name << '\n';
			prettyPrint(skeleton, i, indent + 1);
		}
	}
}

void animation_skeleton::prettyPrintHierarchy() const
{
	prettyPrint(*this, NO_PARENT, 0);
}
