
#define NUM_WEIGHTS 2

static const float kernelOffsets[2] = { 0.f, 1.33333333333333f };
static const float blurWeights[2] = { 0.29411764705882354f, 0.35294117647058826f };

#include "gaussian_blur_common.hlsli"
