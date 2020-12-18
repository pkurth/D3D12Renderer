#include "pch.h"
#include "animation.h"

void animation_skeleton::sampleAnimation(const std::string& name, float time, trs* outLocalTransforms) const
{
	auto clipIndexIt = nameToClipID.find(name);
	assert(clipIndexIt != nameToClipID.end());

	const animation_clip& clip = clips[clipIndexIt->second];
	assert(clip.joints.size() == joints.size());

	time = fmod(time, clip.length);
	float tick = time * clip.ticksPerSecond;
	uint32 firstKeyframeIndex = (uint32)tick;

	float t = tick - firstKeyframeIndex;

	uint32 numJoints = (uint32)joints.size();
	for (uint32 i = 0; i < numJoints; ++i)
	{
		const animation_joint& animJoint = clip.joints[i];

		vec3 position;
		quat rotation;
		vec3 scale;

		if (animJoint.numPositionKeyframes == 1)
		{
			position = clip.positionKeyframes[animJoint.firstPositionKeyframe];
		}
		else
		{
			assert(firstKeyframeIndex < animJoint.numPositionKeyframes);
			uint32 secondKeyframeIndex = min(firstKeyframeIndex + 1, animJoint.numPositionKeyframes);
			
			position = lerp(clip.positionKeyframes[animJoint.firstPositionKeyframe + firstKeyframeIndex], 
				clip.positionKeyframes[animJoint.firstPositionKeyframe + secondKeyframeIndex],
				t);
		}

		if (animJoint.numRotationKeyframes == 1)
		{
			rotation = clip.rotationKeyframes[animJoint.firstRotationKeyframe];
		}
		else
		{
			assert(firstKeyframeIndex < animJoint.numRotationKeyframes);
			uint32 secondKeyframeIndex = min(firstKeyframeIndex + 1, animJoint.numRotationKeyframes);

			rotation = lerp(clip.rotationKeyframes[animJoint.firstRotationKeyframe + firstKeyframeIndex],
				clip.rotationKeyframes[animJoint.firstRotationKeyframe + secondKeyframeIndex],
				t);
		}

		if (animJoint.numScaleKeyframes == 1)
		{
			scale = clip.scaleKeyframes[animJoint.firstScaleKeyframe];
		}
		else
		{
			assert(firstKeyframeIndex < animJoint.numScaleKeyframes);
			uint32 secondKeyframeIndex = min(firstKeyframeIndex + 1, animJoint.numScaleKeyframes);

			scale = lerp(clip.scaleKeyframes[animJoint.firstScaleKeyframe + firstKeyframeIndex],
				clip.scaleKeyframes[animJoint.firstScaleKeyframe + secondKeyframeIndex],
				t);
		}

		outLocalTransforms[i].position = position;
		outLocalTransforms[i].rotation = rotation;
		outLocalTransforms[i].scale = scale;
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
