#include "water_rs.hlsli"
#include "camera.hlsli"

ConstantBuffer<water_cb> cb			: register(b1);
ConstantBuffer<camera_cb> camera	: register(b2);

Texture2D<float4> opaqueColor		: register(t0);
Texture2D<float> opaqueDepth		: register(t1);


struct ps_input
{
    float4 screenPosition	: SV_POSITION;
};


[RootSignature(WATER_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	uint2 screen = uint2(IN.screenPosition.xy);
	float4 backgroundColor = opaqueColor[screen];
	float backgroundDepth = camera.depthBufferDepthToEyeDepth(opaqueDepth[screen]);
	float thisDepth = camera.depthBufferDepthToEyeDepth(IN.screenPosition.z);

	float waterDepth = backgroundDepth - thisDepth;

	float t = smoothstep(0.f, cb.transition, waterDepth);

	float4 color = lerp(cb.shallowColor, cb.deepColor, t);

	return float4(color.xyz, 1.f) * backgroundColor;
}
