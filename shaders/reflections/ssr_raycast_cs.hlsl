#include "ssr_rs.hlsli"
#include "cs.hlsli"
#include "camera.hlsli"
#include "normal.hlsli"
#include "brdf.hlsli"
#include "random.hlsli"

ConstantBuffer<ssr_raycast_cb> cb	: register(b0);
ConstantBuffer<camera_cb> camera	: register(b1);

Texture2D<float> depthBuffer		: register(t0);
Texture2D<float> linearDepthBuffer	: register(t1);
Texture2D<float2> worldNormals		: register(t2);
Texture2D<float4> reflectance		: register(t3);
Texture2D<float2> noise     		: register(t4);


RWTexture2D<float4> output			: register(u0);

SamplerState linearSampler			: register(s0);
SamplerState pointSampler			: register(s1);

static float distanceSquared(float2 a, float2 b) 
{
    a -= b;
    return dot(a, a);
}

static void swap(inout float a, inout float b)
{
    float t = a;
    a = b;
    b = t;
}

static bool intersectsDepthBuffer(float sceneZMax, float rayZMin, float rayZMax)
{
    // Increase thickness along distance. 
    float thickness = max(sceneZMax * 0.2f, 1.f);

    // Effectively remove line/tiny artifacts, mostly caused by Zbuffers precision.
    float depthScale = min(1.f, sceneZMax / 100.f);
    sceneZMax += lerp(0.05f, 0.f, depthScale);

    return (rayZMin >= sceneZMax) && (rayZMax - thickness <= sceneZMax);
}

static bool traceScreenSpaceRay(float3 rayOrigin, float3 rayDirection, float jitter, float roughness,
    out float2 hitPixel)
{
    hitPixel = float2(-1.f, -1.f);

    const float cameraNearPlane = -camera.projectionParams.x; // Now negative.
    float rayLength = ((rayOrigin.z + rayDirection.z * cb.maxDistance) > cameraNearPlane)
        ? (cameraNearPlane - rayOrigin.z) / rayDirection.z
        : cb.maxDistance;

    float3 rayEnd = rayOrigin + rayDirection * rayLength;

    // Project into screen space.
    float4 H0 = mul(camera.proj, float4(rayOrigin, 1.f));
    float4 H1 = mul(camera.proj, float4(rayEnd, 1.f));
    float k0 = 1.f / H0.w;
    float k1 = 1.f / H1.w;
    float3 Q0 = rayOrigin * k0;
    float3 Q1 = rayEnd * k1;

    Q0.z *= -1.f;
    Q1.z *= -1.f;

    // Screen space endpoints.
    float2 P0 = H0.xy * k0;
    float2 P1 = H1.xy * k1;
    P0 = P0 * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    P1 = P1 * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    P0.xy *= cb.dimensions;
    P1.xy *= cb.dimensions;

    // Avoid degenerate lines.
    P1 += (distanceSquared(P0, P1) < 0.0001f) ? 0.01f : 0.f;

    float2 screenDelta = P1 - P0;

    bool permute = false;
    if (abs(screenDelta.x) < abs(screenDelta.y))
    {
        permute = true;
        screenDelta = screenDelta.yx;
        P0 = P0.yx;
        P1 = P1.yx;
    }

    float stepSign = sign(screenDelta.x);
    float invdx = stepSign / screenDelta.x;

    // Derivatives of Q and k.
    float3 dQ = (Q1 - Q0) * invdx;
    float  dk = (k1 - k0) * invdx;
    float2 dP = float2(stepSign, screenDelta.y * invdx);

    float zMin = min(-rayEnd.z, -rayOrigin.z);
    float zMax = max(-rayEnd.z, -rayOrigin.z);


    // Stride based on roughness. Matte materials will recieve higher stride.
    float alphaRoughness = roughness * roughness;
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;

    float strideScale = 1.f - min(1.f, -rayOrigin.z / cb.strideCutoff);
    float strideRoughnessScale = lerp(cb.minStride, cb.maxStride, min(alphaRoughnessSq, 1.f)); // Climb exponentially at extreme conditions.
    float pixelStride = 1.f + strideScale * strideRoughnessScale;

    // Scale derivatives by stride.
    dP *= pixelStride; dQ *= pixelStride; dk *= pixelStride;
    P0 += dP * jitter; Q0 += dQ * jitter; k0 += dk * jitter;

    // Start values and derivatives packed together -> only one operation needed to increase.
    float4 PQk = float4(P0, Q0.z, k0);
    float4 dPQk = float4(dP, dQ.z, dk);


    float level = 0.f;

    float prevZMaxEstimate = -rayOrigin.z;
    float rayZMin = -rayOrigin.z;
    float rayZMax = -rayOrigin.z;
    float sceneZMax = rayZMax + 100000.0f;
    
    uint stepCount = 0;
    float end = P1.x * stepSign;

    hitPixel = float2(0.f, 0.f);

    while (((PQk.x * stepSign) <= end) &&
        (stepCount < cb.numSteps) &&
        (!intersectsDepthBuffer(sceneZMax, rayZMin, rayZMax)) &&
        (sceneZMax != 0.f))
    {
        if (any(hitPixel < 0.f || hitPixel > 1.f))
        {
            return false;
        }

        rayZMin = prevZMaxEstimate;

        // Compute the value at 1/2 step into the future.
        rayZMax = (dPQk.z * 0.5f + PQk.z) / (dPQk.w * 0.5f + PQk.w);
        rayZMax = clamp(rayZMax, zMin, zMax);

        prevZMaxEstimate = rayZMax;

        if (rayZMin > rayZMax) { swap(rayZMin, rayZMax); }

        const float hzbBias = 0.05f;
        const float hzbMinStep = 0.005f;

        // A simple HZB approach based on roughness.
        level += max(hzbBias * roughness, hzbMinStep);
        level = min(level, 6.f);

        hitPixel = permute ? PQk.yx : PQk.xy;
        hitPixel *= cb.invDimensions;

        sceneZMax = linearDepthBuffer.SampleLevel(linearSampler, hitPixel, level); // Already in world units.


        PQk += dPQk;
        ++stepCount;
    }

    return intersectsDepthBuffer(sceneZMax, rayZMin, rayZMax);
}

[numthreads(SSR_BLOCK_SIZE, SSR_BLOCK_SIZE, 1)]
[RootSignature(SSR_RAYCAST_RS)]
void main(cs_input IN)
{
    float2 uv = (IN.dispatchThreadID.xy + 0.5f) * cb.invDimensions;

    const float depth = depthBuffer.SampleLevel(pointSampler, uv, 0);
    if (depth == 1.f)
    {
        output[IN.dispatchThreadID.xy] = float4(0.f, 0.f, -1.f, 0.f);
        return;
    }

    const float3 normal = unpackNormal(worldNormals.SampleLevel(linearSampler, uv, 0));
    const float3 viewNormal = mul(camera.view, float4(normal, 0.f)).xyz;
    const float roughness = clamp(reflectance.SampleLevel(linearSampler, uv, 0).a, 0.03f, 0.97f);
    
    const float3 viewPos = restoreViewSpacePosition(camera.invProj, uv, depth);
    const float3 viewDir = normalize(viewPos);

    float2 h = halton23(cb.frameIndex & 31);
    uint3 noiseDims;
    noise.GetDimensions(0, noiseDims.x, noiseDims.y, noiseDims.z);
    float2 Xi = noise.SampleLevel(linearSampler, (uv + h) * cb.dimensions / float2(noiseDims.xy), 0);
    Xi.y = lerp(Xi.y, 0.f, SSR_GGX_IMPORTANCE_SAMPLE_BIAS);

    float4 H = importanceSampleGGX(Xi, viewNormal, roughness);
    float3 reflDir = reflect(viewDir, H.xyz);

    float jitter = interleavedGradientNoise(IN.dispatchThreadID.xy, cb.frameIndex);

    float2 hitPixel;
    bool hit = traceScreenSpaceRay(viewPos + reflDir * 0.001f, reflDir, jitter, roughness, hitPixel);

    float hitDepth = depthBuffer.SampleLevel(pointSampler, hitPixel, 0);
    float hitMul = hit ? 1.f : -1.f; // We pack the info whether we hit or not in the sign of the depth.
    output[IN.dispatchThreadID.xy] = float4(hitPixel, hitDepth * hitMul, H.w);
}
