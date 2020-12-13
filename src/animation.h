#pragma once

struct skinning_weights
{
	uint8 skinIndices[4];
	uint8 skinWeights[4];
};

struct skeleton_joint
{
	mat4 invBindMatrix; // Transforms from model space to joint space.
	trs bindTransform;	// Position of joint relative to model space.
	char name[16];
	uint32 parentID;
};

struct animation_skeleton
{
	std::vector<skeleton_joint> joints;
};

struct animation_clip
{
	char name[16];
	float length;
	bool looping;
};
