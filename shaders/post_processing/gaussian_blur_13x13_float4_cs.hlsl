
#ifndef DATA_T
#define DATA_T float4
#endif

#define NUM_WEIGHTS 4

static const float kernelOffsets[4] = { 0.f, 1.411764705882353f, 3.2941176470588234f, 5.176470588235294f };
static const float blurWeights[4] = { 0.1964825501511404f, 0.2969069646728344f, 0.09447039785044732f, 0.010381362401148057f };

#include "gaussian_blur_common.hlsli"
