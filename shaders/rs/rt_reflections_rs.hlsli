#ifndef RT_REFLECTIONS_RS_HLSLI
#define RT_REFLECTIONS_RS_HLSLI

#include "../common/light_probe.hlsli"

struct rt_reflections_cb
{
	uint32 sampleSkyFromTexture;
	uint32 frameIndex;
};

#define RT_REFLECTIONS_RS_RESOURCES	0
#define RT_REFLECTIONS_RS_CB		1
#define RT_REFLECTIONS_RS_CAMERA	2
#define RT_REFLECTIONS_RS_LIGHTING	3



#endif
