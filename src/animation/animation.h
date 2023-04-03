#pragma once

#include "core/math.h"
#include "core/random.h"
#include "core/memory.h"
#include "dx/dx_buffer.h"
#include <unordered_map>

#define INVALID_JOINT 0xFFFFFFFF

struct skinning_weights
{
	uint8 skinIndices[4];
	uint8 skinWeights[4];
};


enum limb_type
{
	limb_type_unknown,

	limb_type_torso,
	limb_type_head,

	limb_type_upper_arm_right,
	limb_type_lower_arm_right,
	limb_type_hand_right,

	limb_type_upper_arm_left,
	limb_type_lower_arm_left,
	limb_type_hand_left,

	limb_type_upper_leg_right,
	limb_type_lower_leg_right,
	limb_type_foot_right,

	limb_type_upper_leg_left,
	limb_type_lower_leg_left,
	limb_type_foot_left,

	limb_type_count,
};

extern const char* limbTypeNames[limb_type_count];
extern const vec3 limbTypeColors[limb_type_count];

struct skeleton_joint
{
	std::string name;
	limb_type limbType;
	bool ik;

	mat4 invBindTransform; // Transforms from model space to joint space.
	mat4 bindTransform;	  // Position of joint relative to model space.
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
	fs::path filename;

	std::vector<float> positionTimestamps;
	std::vector<float> rotationTimestamps;
	std::vector<float> scaleTimestamps;

	std::vector<vec3> positionKeyframes;
	std::vector<quat> rotationKeyframes;
	std::vector<vec3> scaleKeyframes;

	std::vector<animation_joint> joints;

	animation_joint rootMotionJoint;
	
	float lengthInSeconds;
	bool looping = true;
	bool bakeRootRotationIntoPose = false;
	bool bakeRootXZTranslationIntoPose = false;
	bool bakeRootYTranslationIntoPose = false;


	void edit();
	trs getFirstRootTransform() const;
	trs getLastRootTransform() const;
};

struct limb_dimensions
{
	float minY, maxY;
	float radius;
	float xOffset, zOffset;
};

struct skeleton_limb
{
	uint32 representativeJoint = INVALID_JOINT;
	limb_dimensions dimensions;
};

struct animation_skeleton
{
	std::vector<skeleton_joint> joints;
	std::unordered_map<std::string, uint32> nameToJointID;

	std::vector<animation_clip> clips;
	std::vector<fs::path> files;

	skeleton_limb limbs[limb_type_count];

	void analyzeJoints(const vec3* positions, const void* others, uint32 otherStride, uint32 numVertices);

	void sampleAnimation(const animation_clip& clip, float time, trs* outLocalTransforms, trs* outRootMotion = 0) const;
	void sampleAnimation(uint32 index, float time, trs* outLocalTransforms, trs* outRootMotion = 0) const;
	void blendLocalTransforms(const trs* localTransforms1, const trs* localTransforms2, float t, trs* outBlendedLocalTransforms) const;
	void getSkinningMatricesFromLocalTransforms(const trs* localTransforms, mat4* outSkinningMatrices, const trs& worldTransform = trs::identity) const;
	void getSkinningMatricesFromLocalTransforms(const trs* localTransforms, trs* outGlobalTransforms, mat4* outSkinningMatrices, const trs& worldTransform = trs::identity) const;
	void getSkinningMatricesFromGlobalTransforms(const trs* globalTransforms, mat4* outSkinningMatrices) const;

	std::vector<uint32> getClipsByName(const std::string& name);

	void prettyPrintHierarchy() const;
};

struct animation_instance
{
	animation_instance() { }
	animation_instance(const animation_clip* clip, float startTime = 0.f);

	void set(const animation_clip* clip, float startTime = 0.f);
	void update(const animation_skeleton& skeleton, float dt, trs* outLocalTransforms, trs& outDeltaRootMotion);

	bool valid() const { return clip != 0; }

	const animation_clip* clip = 0;
	float time = 0.f;

	trs lastRootMotion;
};

#if 0
struct animation_blend_tree_1d
{
	animation_blend_tree_1d() { }
	animation_blend_tree_1d(std::initializer_list<animation_clip*> clips, float startRelTime = 0.f, float startBlendValue = 0.f);

	void setBlendValue(float blendValue);
	void update(const animation_skeleton& skeleton, float dt, trs* outLocalTransforms, trs& outDeltaRootMotion);

private:
	animation_clip* clips[8];
	uint32 numClips = 0;

	float value;
	uint32 first;
	uint32 second;
	float relTime;
	float blendValue;

	trs lastRootMotion;
};
#endif

struct animation_component
{
	animation_instance animation;
	float timeScale = 1.f;

	dx_vertex_buffer_group_view currentVertexBuffer;
	dx_vertex_buffer_group_view prevFrameVertexBuffer;
	trs* currentGlobalTransforms = 0;

	void update(const ref<struct multi_mesh>& mesh, memory_arena& arena, float dt, trs* transform = 0);
	void drawCurrentSkeleton(const ref<struct multi_mesh>& mesh, const trs& transform, struct ldr_render_pass* renderPass);
};

