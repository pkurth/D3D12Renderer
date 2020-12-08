#include "cs.hlsli"
#include "camera.hlsli"
#include "light_culling_rs.hlsli"
#include "light_source.hlsli"

#define BLOCK_SIZE 16

ConstantBuffer<camera_cb> camera	: register(b0);
ConstantBuffer<light_culling_cb> cb	: register(b1);

Texture2D<float> depthBuffer                        : register(t0);

StructuredBuffer<point_light_cb> pointLights        : register(t1);
StructuredBuffer<spot_light_cb> spotLights          : register(t2);

StructuredBuffer<light_culling_view_frustum> frusta : register(t3);

RWStructuredBuffer<uint> lightIndexCounter          : register(u0);
RWStructuredBuffer<uint> pointLightIndexList        : register(u1);
RWStructuredBuffer<uint> spotLightIndexList         : register(u2);
RWTexture2D<uint4> lightGrid                        : register(u3);

groupshared uint groupMinDepth;
groupshared uint groupMaxDepth;
groupshared light_culling_view_frustum groupFrustum;

groupshared uint groupPointLightCount;
groupshared uint groupSpotLightCount;

groupshared uint groupPointLightIndexStartOffset;
groupshared uint groupSpotLightIndexStartOffset;

groupshared uint groupPointLightList[MAX_NUM_LIGHTS_PER_TILE];
groupshared uint groupSpotLightList[MAX_NUM_LIGHTS_PER_TILE];



struct spot_light_bounding_volume
{
    float3 tip;
    float height;
    float3 direction;
    float radius;
};

static spot_light_bounding_volume getSpotLightBoundingVolume(spot_light_cb sl)
{
    spot_light_bounding_volume result;
    result.tip = sl.position;
    result.direction = sl.direction;
    result.height = sl.maxDistance;
    float oc = sl.outerCutoff;
    result.radius = result.height * tan(acos(oc));// sqrt(1.f - oc * oc) / oc; // Same as height * tan(acos(oc)).
    return result;
}

static bool pointLightOutsidePlane(point_light_cb pl, light_culling_frustum_plane plane)
{
    return dot(plane.N, pl.position) - plane.d < -pl.radius;
}

static bool pointLightInsideFrustum(point_light_cb pl, light_culling_view_frustum frustum, light_culling_frustum_plane nearPlane, light_culling_frustum_plane farPlane)
{
    bool result = true;

    if (pointLightOutsidePlane(pl, nearPlane)
        || pointLightOutsidePlane(pl, farPlane))
    {
        result = false;
    }

    for (int i = 0; i < 4 && result; ++i)
    {
        if (pointLightOutsidePlane(pl, frustum.planes[i]))
        {
            result = false;
        }
    }

    return result;
}

static bool pointOutsidePlane(float3 p, light_culling_frustum_plane plane)
{
    return dot(plane.N, p) - plane.d < 0;
}

static bool spotLightOutsidePlane(spot_light_bounding_volume sl, light_culling_frustum_plane plane)
{
    float3 m = normalize(cross(cross(plane.N, sl.direction), sl.direction));
    float3 Q = sl.tip + sl.direction * sl.height - m * sl.radius;
    return pointOutsidePlane(sl.tip, plane) && pointOutsidePlane(Q, plane);
}

static bool spotLightInsideFrustum(spot_light_bounding_volume sl, light_culling_view_frustum frustum, light_culling_frustum_plane nearPlane, light_culling_frustum_plane farPlane)
{
    bool result = true;

    if (spotLightOutsidePlane(sl, nearPlane)
        || spotLightOutsidePlane(sl, farPlane))
    {
        result = false;
    }

    for (int i = 0; i < 4 && result; i++)
    {
        if (spotLightOutsidePlane(sl, frustum.planes[i]))
        {
            result = false;
        }
    }

    return result;
}

static void groupAppendPointLight(uint lightIndex)
{
    uint index;
    InterlockedAdd(groupPointLightCount, 1, index);
    if (index < MAX_NUM_LIGHTS_PER_TILE)
    {
        groupPointLightList[index] = lightIndex;
    }
}

static void groupAppendSpotLight(uint lightIndex)
{
    uint index;
    InterlockedAdd(groupSpotLightCount, 1, index);
    if (index < MAX_NUM_LIGHTS_PER_TILE)
    {
        groupSpotLightList[index] = lightIndex;
    }
}

[RootSignature(LIGHT_CULLING_RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(cs_input IN)
{
    uint2 texCoord = IN.dispatchThreadID.xy;
    float fDepth = depthBuffer.Load(uint3(texCoord, 0));
    // Since the depth is between 0 and 1, we can perform min and max operations on the bit pattern as uint. This is because native HLSL does not support floating point atomics.
    uint uDepth = asuint(fDepth);

    if (IN.groupIndex == 0) // Avoid contention by other threads in the group.
    {
        groupMinDepth = asuint(0.9999999f);
        groupMaxDepth = 0;
        groupPointLightCount = 0;
        groupSpotLightCount = 0;
        groupFrustum = frusta[IN.groupID.y * cb.numThreadGroupsX + IN.groupID.x];
    }

    GroupMemoryBarrierWithGroupSync();

    InterlockedMin(groupMinDepth, uDepth);
    InterlockedMax(groupMaxDepth, uDepth);

    GroupMemoryBarrierWithGroupSync();

    float fMinDepth = asfloat(groupMinDepth);
    float fMaxDepth = asfloat(groupMaxDepth);

    float3 forward = camera.forward.xyz;
    float3 cameraPos = camera.position.xyz;

    float nearZ = depthBufferDepthToEyeDepth(fMinDepth, camera.projectionParams); // Positive.
    float farZ = depthBufferDepthToEyeDepth(fMaxDepth, camera.projectionParams); // Positive.

    light_culling_frustum_plane cameraNearPlane = { forward, dot(forward, cameraPos + camera.projectionParams.x * forward) };
    light_culling_frustum_plane nearPlane = { forward, dot(forward, cameraPos + nearZ * forward) };
    light_culling_frustum_plane farPlane = { -forward, -dot(forward, cameraPos + farZ * forward) };


    const uint numPointLights = cb.numPointLights;
    for (uint i = IN.groupIndex; i < numPointLights; i += BLOCK_SIZE * BLOCK_SIZE)
    {
        point_light_cb pl = pointLights[i];
        if (pointLightInsideFrustum(pl, groupFrustum, nearPlane, farPlane))
        {
            groupAppendPointLight(i);
        }
    }

    const uint numSpotLights = cb.numSpotLights;
    for (i = IN.groupIndex; i < numSpotLights; i += BLOCK_SIZE * BLOCK_SIZE)
    {
        spot_light_bounding_volume sl = getSpotLightBoundingVolume(spotLights[i]);
        if (spotLightInsideFrustum(sl, groupFrustum, nearPlane, farPlane))
        {
            groupAppendSpotLight(i);
        }
    }


    GroupMemoryBarrierWithGroupSync();

    if (IN.groupIndex == 0)
    {
        InterlockedAdd(lightIndexCounter[0], groupPointLightCount, groupPointLightIndexStartOffset);
        InterlockedAdd(lightIndexCounter[1], groupSpotLightCount, groupSpotLightIndexStartOffset);

        lightGrid[IN.groupID.xy] = uint4(groupPointLightIndexStartOffset, groupPointLightCount, groupSpotLightIndexStartOffset, groupSpotLightCount);
    }

    GroupMemoryBarrierWithGroupSync();

    for (i = IN.groupIndex; i < groupPointLightCount; i += BLOCK_SIZE * BLOCK_SIZE)
    {
        pointLightIndexList[groupPointLightIndexStartOffset + i] = groupPointLightList[i];
    }

    for (i = IN.groupIndex; i < groupSpotLightCount; i += BLOCK_SIZE * BLOCK_SIZE)
    {
        spotLightIndexList[groupSpotLightIndexStartOffset + i] = groupSpotLightList[i];
    }
}
