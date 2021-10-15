#include "pch.h"
#include "locomotion_inference.h"


#if __has_include("../tmp/network.h")
#include "../tmp/network.h"
#include "locomotion_environment.h"


template <uint32 inputSize, uint32 outputSize>
static void applyLayer(const float(&weights)[outputSize][inputSize], const float(&bias)[outputSize], const float* from, float* to, bool activation)
{
	for (uint32 y = 0; y < outputSize; ++y)
	{
		const float* row = weights[y];

		float sum = 0.f;
		for (uint32 x = 0; x < inputSize; ++x)
		{
			sum += row[x] * from[x];
		}
		sum += bias[y];
		to[y] = activation ? tanh(sum) : sum;
	}
}

void locomotion_inference_environment::initialize(game_scene& scene, const humanoid_ragdoll& ragdoll)
{
	locomotion_environment::initialize(ragdoll);
	reset(scene);
}

void locomotion_inference_environment::update(game_scene& scene)
{
	learning_state state;
	if (getState(state))
	{
		lastSmoothedAction = {};
		applyAction(scene, {});
	}

	learning_action action;

	float* buffers = (float*)alloca(sizeof(float) * HIDDEN_LAYER_SIZE * 2);

	float* a = buffers;
	float* b = buffers + HIDDEN_LAYER_SIZE;

	// State to a.
	applyLayer(policyWeights1, policyBias1, (const float*)&state, a, true);

	// a to b.
	applyLayer(policyWeights2, policyBias2, a, b, true);

	// b to action.
	applyLayer(actionWeights, actionBias, b, (float*)&action, false);


	applyAction(scene, action);
}

#else
void locomotion_inference_environment::initialize(game_scene& scene, const humanoid_ragdoll& ragdoll) {}
void locomotion_inference_environment::update(game_scene& scene) {}
#endif