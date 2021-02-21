
#define NUM_WEIGHTS 3

// http://dev.theomader.com/gaussian-kernel-calculator/
// 5x5 kernel with sigma 1.

static const float kernelOffsets[3] = { 0.f, 1.f, 2.f };
static const float blurWeights[3] = { 0.38774f, 0.24477f, 0.06136f };

#include "gaussian_blur_common.hlsli"
