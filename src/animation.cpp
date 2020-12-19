#include "pch.h"
#include "animation.h"

static vec3 samplePosition(const animation_clip& clip, const animation_joint& animJoint, uint32 firstKeyframeIndex, uint32 secondKeyframeIndex, float t)
{
	if (animJoint.numPositionKeyframes == 1)
	{
		return clip.positionKeyframes[animJoint.firstPositionKeyframe];
	}

	return lerp(clip.positionKeyframes[animJoint.firstPositionKeyframe + firstKeyframeIndex],
		clip.positionKeyframes[animJoint.firstPositionKeyframe + secondKeyframeIndex],
		t);
}

static quat sampleRotation(const animation_clip& clip, const animation_joint& animJoint, uint32 firstKeyframeIndex, uint32 secondKeyframeIndex, float t)
{
	if (animJoint.numRotationKeyframes == 1)
	{
		return clip.rotationKeyframes[animJoint.firstRotationKeyframe];
	}

	quat a = clip.rotationKeyframes[animJoint.firstRotationKeyframe + firstKeyframeIndex];
	quat b = clip.rotationKeyframes[animJoint.firstRotationKeyframe + secondKeyframeIndex];

	if (dot(a.v4, b.v4) < 0.f)
	{
		b.v4 *= -1.f;
	}

	return lerp(a, b, t);
}

static vec3 sampleScale(const animation_clip& clip, const animation_joint& animJoint, uint32 firstKeyframeIndex, uint32 secondKeyframeIndex, float t)
{
	if (animJoint.numScaleKeyframes == 1)
	{
		return clip.scaleKeyframes[animJoint.firstScaleKeyframe];
	}

	return lerp(clip.scaleKeyframes[animJoint.firstScaleKeyframe + firstKeyframeIndex],
		clip.scaleKeyframes[animJoint.firstScaleKeyframe + secondKeyframeIndex],
		t);
}

void animation_skeleton::sampleAnimation(const std::string& name, float time, trs* outLocalTransforms) const
{
	auto clipIndexIt = nameToClipID.find(name);
	assert(clipIndexIt != nameToClipID.end());

	const animation_clip& clip = clips[clipIndexIt->second];
	assert(clip.joints.size() == joints.size());

	time = fmod(time, clip.length);

	float tick = time * clip.ticksPerSecond;
	uint32 firstKeyframeIndex = (uint32)tick;
	uint32 secondKeyframeIndex = firstKeyframeIndex + 1;
	float t = tick - firstKeyframeIndex;

	uint32 numJoints = (uint32)joints.size();
	for (uint32 i = 0; i < numJoints; ++i)
	{
		const animation_joint& animJoint = clip.joints[i];

		if (animJoint.isAnimated)
		{
			outLocalTransforms[i].position = samplePosition(clip, animJoint, firstKeyframeIndex, secondKeyframeIndex, t);
			outLocalTransforms[i].rotation = sampleRotation(clip, animJoint, firstKeyframeIndex, secondKeyframeIndex, t);
			outLocalTransforms[i].scale = sampleScale(clip, animJoint, firstKeyframeIndex, secondKeyframeIndex, t);
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
