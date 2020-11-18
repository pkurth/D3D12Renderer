#include "volumetrics_rs.hlsl"
#include "camera.hlsl"
#include "light_source.hlsl"

ConstantBuffer<volumetrics_bounding_box_cb> box	: register(b1);
ConstantBuffer<camera_cb> camera				: register(b2);

SamplerState depthBufferSampler					: register(s0);
Texture2D<float> depthBuffer					: register(t0);

SamplerComparisonState shadowSampler			: register(s1);
ConstantBuffer<directional_light_cb> sun		: register(b3);
Texture2D<float> sunShadowCascades[4]			: register(t1);


struct ps_input
{
	float boxBackfaceDepth : DEPTH;
	float4 position : SV_Position;
};

static float intersectAABB(float3 origin, float3 direction, float3 minCorner, float3 maxCorner)
{
	float3 invDir = 1.f / direction; // This can be Inf (when one direction component is 0) but still works.

	float tx1 = (minCorner.x - origin.x) * invDir.x;
	float tx2 = (maxCorner.x - origin.x) * invDir.x;

	float outT = min(tx1, tx2);
	float tmax = max(tx1, tx2);

	float ty1 = (minCorner.y - origin.y) * invDir.y;
	float ty2 = (maxCorner.y - origin.y) * invDir.y;

	outT = max(outT, min(ty1, ty2));
	tmax = min(tmax, max(ty1, ty2));

	float tz1 = (minCorner.z - origin.z) * invDir.z;
	float tz2 = (maxCorner.z - origin.z) * invDir.z;

	outT = max(outT, min(tz1, tz2));
	tmax = min(tmax, max(tz1, tz2));

	return outT;
}

#define RAY_STEP_LENGTH 1.f

[RootSignature(VOLUMETRICS_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	float2 screenUV = IN.position.xy * camera.invScreenDims;

	float3 V = restoreWorldDirection(camera.invViewProj, screenUV, camera.position.xyz) / camera.projectionParams.x;
	float3 O = camera.position.xyz;

	float t = intersectAABB(O, V, box.minCorner.xyz, box.maxCorner.xyz);


	float depth = depthBuffer.SampleLevel(depthBufferSampler, screenUV, 0);
	depth = depthBufferDepthToEyeDepth(depth, camera.projectionParams);

	if (depth < t)
	{
		return (float4)0.f; // Object is in front of volume, so no fog.
	}

	float minDepth = max(camera.projectionParams.x, t);
	float maxDepth = min(depth, IN.boxBackfaceDepth);

	float3 start = camera.position.xyz + V * minDepth;
	float3 end = camera.position.xyz + V * maxDepth;



	//uint currentCascadeIndex = sun.numShadowCascades - 1;
	//float4 bias = sun.bias;
	//float visibility = sampleShadowMap(sun.vp[currentCascadeIndex], end, sunShadowCascades[currentCascadeIndex],
	//	shadowSampler, SUN_SHADOW_TEXEL_SIZE, bias[currentCascadeIndex]);

	float fogLength = length(start - end);
	return float4(1.f, 1.f, 1.f, saturate(fogLength / 1000.f));
}
