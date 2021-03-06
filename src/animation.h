#pragma once

#include "math.h"
#include <unordered_map>

#define NO_PARENT 0xFFFFFFFF

struct skinning_weights
{
	uint8 skinIndices[4];
	uint8 skinWeights[4];
};

struct skeleton_joint
{
	std::string name;
	mat4 invBindMatrix; // Transforms from model space to joint space.
	trs bindTransform;	// Position of joint relative to model space.
	uint32 parentID;
};

struct animation_joint
{
	bool isAnimated = false;

	uint32 firstPositionKeyframe;
	uint32 numPositionKeyframes;

	uint32 firstRotationKeyframe;
	uint32 numRotationKeyframes;

	uint32 firstScaleKeyframe;
	uint32 numScaleKeyframes;
};

struct animation_clip
{
	std::string name;

	std::vector<float> positionTimestamps;
	std::vector<float> rotationTimestamps;
	std::vector<float> scaleTimestamps;

	std::vector<vec3> positionKeyframes;
	std::vector<quat> rotationKeyframes;
	std::vector<vec3> scaleKeyframes;

	std::vector<animation_joint> joints;
	
	float lengthInSeconds;
};

struct animation_skeleton
{
	std::vector<skeleton_joint> joints;
	std::unordered_map<std::string, uint32> nameToJointID;

	std::vector<animation_clip> clips;
	std::unordered_map<std::string, uint32> nameToClipID;

	std::vector<std::string> files;

	void loadFromAssimp(const struct aiScene* scene, float scale = 1.f);
	void pushAssimpAnimation(const std::string& suffix, const struct aiAnimation* animation, float scale = 1.f);
	void pushAssimpAnimations(const std::string& sceneFilename, float scale = 1.f);
	void pushAssimpAnimationsInDirectory(const std::string& directory, float scale = 1.f);

	void sampleAnimation(const std::string& name, float time, trs* outLocalTransforms) const;
	void getSkinningMatricesFromLocalTransforms(const trs* localTransforms, mat4* outSkinningMatrices, const trs& worldTransform = trs::identity) const;

	void prettyPrintHierarchy() const;
};
