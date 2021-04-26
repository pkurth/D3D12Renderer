#include "pch.h"

#define STATE_SIZE 16
#define ACTION_SIZE 16


extern "C" __declspec(dllexport) int getPhysicsStateSize()
{
	return STATE_SIZE;
}

extern "C" __declspec(dllexport) int getPhysicsActionSize()
{
	return ACTION_SIZE;
}

// Returns true, if simulation has ended.
extern "C" __declspec(dllexport) int updatePhysics(float* action, float* outState, float* outReward)
{
	return true;
}

extern "C" __declspec(dllexport) void resetPhysics(float* outState)
{
	
}
