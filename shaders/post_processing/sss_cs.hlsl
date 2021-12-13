#include "cs.hlsli"
#include "post_processing_rs.hlsli"
#include "camera.hlsli"
#include "math.hlsli"
#include "random.hlsli"

ConstantBuffer<sss_cb> cb				: register(b0);
ConstantBuffer<camera_cb> camera		: register(b1);

Texture2D<float> linearDepthBuffer		: register(t0);

RWTexture2D<float> result               : register(u0);

SamplerState pointSampler			    : register(s0);


[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
[RootSignature(SSS_RS)]
void main(cs_input IN)
{
    float2 uv = (IN.dispatchThreadID.xy + 0.5f) * cb.invDimensions;
	float depth = linearDepthBuffer.SampleLevel(pointSampler, uv, 1);

    float visibility = 1.f;

    if (depth < cb.maxDistanceFromCamera)
    {
        float3 rayOrigin = camera.restoreViewSpacePositionEyeDepth(uv, depth);
        float3 rayDirection = cb.lightDirection;

        float jitter = 0.f;
        jitter = random(uv * 51239.f + cb.seed) * 0.05f;

        float occlusion = 0.f;

        float stepSize = cb.rayDistance / cb.numSteps;
        float3 step = rayDirection * stepSize;

        float3 rayPos = rayOrigin + step * jitter;

        for (uint i = 0; i < cb.numSteps; ++i)
        {
            rayPos += step;
            float4 proj = mul(camera.proj, float4(rayPos, 1.f));
            proj.xyz /= proj.w;

            float2 uv = proj.xy * float2(0.5f, -0.5f) + 0.5f;

            [branch]
            if (isSaturated(uv) && isSaturated(proj.z))
            {
                float rayDepth = proj.w;
                float depth = linearDepthBuffer.SampleLevel(pointSampler, uv, 1);
                float delta = rayDepth - depth;

                if (delta > 0.02f * (1.f - jitter) && delta < cb.thickness)
                {
                    float borderDist = min(1.f - max(uv.x, uv.y), min(uv.x, uv.y));
                    occlusion = saturate(borderDist * cb.invBorderFadeout);

                    break;
                }
            }

        }

        float t = smoothstep(cb.maxDistanceFromCamera - cb.distanceFadeoutRange, cb.maxDistanceFromCamera, depth);
        occlusion = lerp(occlusion, 0.f, t);

        visibility = 1.f - occlusion;
    }

    result[IN.dispatchThreadID.xy] = visibility;
}
