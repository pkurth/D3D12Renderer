#include "ssr_rs.hlsli"
#include "cs.hlsli"

// Adapted from Wicked Engine. https://github.com/turanszkij/WickedEngine

/*
Copyright (c) 2018 Wicked Engine

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
IN THE SOFTWARE.
*/


ConstantBuffer<ssr_temporal_cb> cb	: register(b0);

RWTexture2D<float4> output			: register(u0);

Texture2D<float4> currResolve	    : register(t0);
Texture2D<float4> prevResolve	    : register(t1);
Texture2D<float2> motion		    : register(t2);

SamplerState linearSampler			: register(s0);



static const int2 sampleOffset[9] = 
{ 
    int2(-1, -1), 
    int2(0, -1), 
    int2(1, -1), 
    int2(-1, 0), 
    int2(0, 0), 
    int2(1, 0), 
    int2(-1, 1), 
    int2(0, 1), 
    int2(1, 1) 
};


static float luma4(float3 color)
{
    return (color.g * 2.f) + (color.r + color.b);
}

static float hdrWeight4(float3 color, float exposure)
{
    return rcp(luma4(color) * exposure + 4.f);
}

static void resolverAABB(Texture2D<float4> currentColor, SamplerState currentSampler, float sharpness, float exposureScale, float AABBScale, float2 uv, float2 texelSize, 
    inout float4 currentMin, inout float4 currentMax, inout float4 currentAverage, inout float4 currentOutput)
{
    // Modulate Luma HDR.

    float4 sampleColors[9];
    [unroll]
    for (uint i = 0; i < 9; i++)
    {
        sampleColors[i] = currentColor.SampleLevel(currentSampler, uv + (sampleOffset[i] * texelSize), 0.f);
    }

    float sampleWeights[9];
    [unroll]
    for (uint j = 0; j < 9; j++)
    {
        sampleWeights[j] = hdrWeight4(sampleColors[j].rgb, exposureScale);
    }

    float totalWeight = 0;
    [unroll]
    for (uint k = 0; k < 9; k++)
    {
        totalWeight += sampleWeights[k];
    }
    sampleColors[4] = (sampleColors[0] * sampleWeights[0] + sampleColors[1] * sampleWeights[1] + sampleColors[2] * sampleWeights[2] + sampleColors[3] * sampleWeights[3] + sampleColors[4] * sampleWeights[4] +
        sampleColors[5] * sampleWeights[5] + sampleColors[6] * sampleWeights[6] + sampleColors[7] * sampleWeights[7] + sampleColors[8] * sampleWeights[8]) / totalWeight;

    // Variance Clipping (AABB).

    float4 m1 = 0.f;
    float4 m2 = 0.f;
    [unroll]
    for (uint x = 0; x < 9; x++)
    {
        m1 += sampleColors[x];
        m2 += sampleColors[x] * sampleColors[x];
    }

    float4 mean = m1 / 9.f;
    float4 stddev = sqrt((m2 / 9.f) - mean * mean);

    currentMin = mean - AABBScale * stddev;
    currentMax = mean + AABBScale * stddev;

    currentOutput = sampleColors[4];
    currentMin = min(currentMin, currentOutput);
    currentMax = max(currentMax, currentOutput);
    currentAverage = mean;
}

static float4 clipAABB(float3 aabb_min, float3 aabb_max, float4 p, float4 q)
{
    float3 p_clip = 0.5f * (aabb_max + aabb_min);
    float3 e_clip = 0.5f * (aabb_max - aabb_min) + 0.00000001f;

    float4 v_clip = q - float4(p_clip, p.w);
    float3 v_unit = v_clip.xyz / e_clip;
    float3 a_unit = abs(v_unit);
    float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

    if (ma_unit > 1.f)
    {
        return float4(p_clip, p.w) + v_clip / ma_unit;
    }
    else
    {
        return q; // Point inside AABB.
    }
}

[numthreads(SSR_BLOCK_SIZE, SSR_BLOCK_SIZE, 1)]
[RootSignature(SSR_TEMPORAL_RS)]
void main(cs_input IN)
{
    const float2 uv = (IN.dispatchThreadID.xy + 0.5f) * cb.invDimensions;

    const float2 m = motion.SampleLevel(linearSampler, uv, 0);
    const float2 prevUV = uv + m;

    float4 prev = prevResolve.SampleLevel(linearSampler, prevUV, 0);


    float4 current = 0.f.xxxx;
    float4 currentMin, currentMax, currentAverage;
    resolverAABB(currResolve, linearSampler, 0.f, 10.f, 3.f, uv, cb.invDimensions, currentMin, currentMax, currentAverage, current);

    prev.xyz = clipAABB(currentMin.xyz, currentMax.xyz, clamp(currentAverage, currentMin, currentMax), prev).xyz;
    prev.a = clamp(prev.a, currentMin.a, currentMax.a);



    float blendFinal = lerp(0.85f, 0.98f, saturate(1.f - length(m) * 100.f));

    float4 result = lerp(current, prev, blendFinal);
    output[IN.dispatchThreadID.xy] = max(0, result);
}
