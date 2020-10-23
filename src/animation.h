#pragma once

struct skinning_weights
{
	uint8 skinIndices[4];
	uint8 skinWeights[4];
};

struct skeleton_joint
{
	char name[16];
	uint32 parentID;
	trs bindTransform;	// Position of joint relative to model space.
	mat4 invBindMatrix; // Transforms from model space to joint space.
};

struct animation_skeleton
{
	skeleton_joint* joints;
	uint32 numJoints;
};

struct animation_position_keyframe
{
	vec3 position;
	float time;
};

struct animation_rotation_keyframe
{
	quat rotation;
	float time;
};

struct animation_scale_keyframe
{
	float scale;
	float time;
};

struct animation_joint
{
	animation_position_keyframe* positions;
	animation_rotation_keyframe* rotations;
	animation_scale_keyframe* scales;

	uint32 numPositions;
	uint32 numRotations;
	uint32 numScales;
};

struct animation_clip
{
	char name[16];
	float length;
	bool looping;

	animation_joint* joints;
	uint32 numJoints;

	animation_position_keyframe* allPositionKeyframes;
	animation_rotation_keyframe* allRotationKeyframes;
	animation_scale_keyframe* allScaleKeyframes;

	uint32 totalNumberOfPositionKeyFrames;
	uint32 totalNumberOfRotationKeyFrames;
	uint32 totalNumberOfScaleKeyFrames;
};
