#include "cs.hlsli"
#include "visualization_rs.hlsli"
#include "light_source.hlsli"
#include "camera.hlsli"
#include "random.hlsli"

ConstantBuffer<visualize_sun_shadow_cascades_cb> cb		: register(b0);
ConstantBuffer<directional_light_cb> sun				: register(b1);

RWTexture2D<float4> outputTexture						: register(u0);
Texture2D<float> depthBuffer							: register(t0);


static const float3 cascadeColors[4] =
{
	float3(1.f, 0.f, 0.f),
	float3(0.f, 1.f, 0.f),
	float3(0.f, 0.f, 1.f),
	float3(1.f, 1.f, 0.f)
};

[numthreads(16, 16, 1)]
[RootSignature(VISUALIZE_SUN_SHADOW_CASCADES_RS)]
void main(cs_input IN)
{
	float depth = depthBuffer[IN.dispatchThreadID.xy];
	if (depth == 1.f)
	{
		outputTexture[IN.dispatchThreadID.xy] = float4(0.f, 0.f, 0.f, 0.f);
		return;
	}

	uint2 dims;
	uint numMips;
	depthBuffer.GetDimensions(0, dims.x, dims.y, numMips);

	float2 invDims = rcp((float2)dims);
	float2 uv = (IN.dispatchThreadID.xy + 0.5f) * invDims;

	float3 worldPosition = restoreWorldSpacePosition(cb.invViewProj, uv, depth);

#if 1
	float worldDepth = dot(worldPosition - cb.cameraPosition.xyz, cb.cameraForward.xyz);

	float4 comparison = worldDepth.xxxx > sun.cascadeDistances;

	int currentCascadeIndex = dot(float4(sun.numShadowCascades > 0, sun.numShadowCascades > 1, sun.numShadowCascades > 2, sun.numShadowCascades > 3), comparison);
	currentCascadeIndex = min(currentCascadeIndex, sun.numShadowCascades - 1);

	outputTexture[IN.dispatchThreadID.xy] = float4(cascadeColors[currentCascadeIndex], 1.f);

#else

	float4 color = float4(0.f, 0.f, 0.f, 1.f);

	for (uint i = 0; i < sun.numShadowCascades; ++i)
	{
		float4x4 vp = sun.viewProjs[i];

		float4 lightProjected = mul(vp, float4(worldPosition, 1.f));
		lightProjected.xyz /= lightProjected.w;

		lightProjected.xy = lightProjected.xy * 0.5f + float2(0.5f, 0.5f);
		lightProjected.y = 1.f - lightProjected.y;

		if (all(lightProjected.xyz > 0.f && lightProjected.xyz < 1.f))
		{
			float size = 2048;
			float2 pixel = floor(lightProjected.xy * size) / size;

			color.rgb = cascadeColors[i];
			//color.rgb = random(pixel);
			break;
		}
	}

	outputTexture[IN.dispatchThreadID.xy] = color;

#endif
}
