#include "pch.h"
#include "animation.h"

static vec3 samplePosition(const animation_clip& clip, const animation_joint& animJoint, float tick)
{
	if (animJoint.numPositionKeyframes == 1)
	{
		return clip.positionKeyframes[animJoint.firstPositionKeyframe];
	}

	uint32 firstKeyframeIndex = -1;
	for (uint32 i = 0; i < animJoint.numPositionKeyframes - 1; ++i)
	{
		uint32 j = i + animJoint.firstPositionKeyframe;
		if (tick < clip.positionTimestamps[j + 1])
		{
			firstKeyframeIndex = j;
			break;
		}
	}
	assert(firstKeyframeIndex != -1);

	uint32 secondKeyframeIndex = firstKeyframeIndex + 1;

	float t = inverseLerp(clip.positionTimestamps[firstKeyframeIndex], clip.positionTimestamps[secondKeyframeIndex], tick);

	vec3 a = clip.positionKeyframes[firstKeyframeIndex];
	vec3 b = clip.positionKeyframes[secondKeyframeIndex];

	return lerp(a, b, t);
}

static quat sampleRotation(const animation_clip& clip, const animation_joint& animJoint, float tick)
{
	if (animJoint.numRotationKeyframes == 1)
	{
		return clip.rotationKeyframes[animJoint.firstRotationKeyframe];
	}

	uint32 firstKeyframeIndex = -1;
	for (uint32 i = 0; i < animJoint.numRotationKeyframes - 1; ++i)
	{
		uint32 j = i + animJoint.firstRotationKeyframe;
		if (tick < clip.rotationTimestamps[j + 1])
		{
			firstKeyframeIndex = j;
			break;
		}
	}
	assert(firstKeyframeIndex != -1);

	uint32 secondKeyframeIndex = firstKeyframeIndex + 1;

	float t = inverseLerp(clip.rotationTimestamps[firstKeyframeIndex], clip.rotationTimestamps[secondKeyframeIndex], tick);

	quat a = clip.rotationKeyframes[firstKeyframeIndex];
	quat b = clip.rotationKeyframes[secondKeyframeIndex];

	if (dot(a.v4, b.v4) < 0.f)
	{
		b.v4 *= -1.f;
	}

	return lerp(a, b, t);
}

static vec3 sampleScale(const animation_clip& clip, const animation_joint& animJoint, float tick)
{
	if (animJoint.numScaleKeyframes == 1)
	{
		return clip.scaleKeyframes[animJoint.firstScaleKeyframe];
	}

	uint32 firstKeyframeIndex = -1;
	for (uint32 i = 0; i < animJoint.numScaleKeyframes - 1; ++i)
	{
		uint32 j = i + animJoint.firstScaleKeyframe;
		if (tick < clip.scaleTimestamps[j + 1])
		{
			firstKeyframeIndex = j;
			break;
		}
	}
	assert(firstKeyframeIndex != -1);

	uint32 secondKeyframeIndex = firstKeyframeIndex + 1;

	float t = inverseLerp(clip.scaleTimestamps[firstKeyframeIndex], clip.scaleTimestamps[secondKeyframeIndex], tick);

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

	time = fmod(time, clip.length);

	float tick = time * clip.ticksPerSecond;

	uint32 numJoints = (uint32)joints.size();
	for (uint32 i = 0; i < numJoints; ++i)
	{
		const animation_joint& animJoint = clip.joints[i];

		if (animJoint.isAnimated)
		{
			outLocalTransforms[i].position = samplePosition(clip, animJoint, tick);
			outLocalTransforms[i].rotation = sampleRotation(clip, animJoint, tick);
			outLocalTransforms[i].scale = sampleScale(clip, animJoint, tick);
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
