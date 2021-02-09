#include "cs.hlsli"
#include "camera.hlsli"
#include "light_culling_rs.hlsli"
#include "light_source.hlsli"

#define BLOCK_SIZE 16

ConstantBuffer<camera_cb> camera	                : register(b0);
ConstantBuffer<light_culling_cb> cb	                : register(b1);

Texture2D<float> depthBuffer                        : register(t0);
StructuredBuffer<light_culling_view_frustum> frusta : register(t1);

StructuredBuffer<point_light_cb> pointLights        : register(t2);
StructuredBuffer<spot_light_cb> spotLights          : register(t3);

RWTexture2D<uint4> tiledCullingGrid                 : register(u0);
RWStructuredBuffer<uint> tiledCullingIndexCounter   : register(u1);

RWStructuredBuffer<uint> tiledPointLightIndexList   : register(u2);
RWStructuredBuffer<uint> tiledSpotLightIndexList    : register(u3);


groupshared uint groupMinDepth;
groupshared uint groupMaxDepth;
groupshared light_culling_view_frustum groupFrustum;

groupshared uint groupPointLightCount;
groupshared uint groupSpotLightCount;
groupshared uint groupDecalCount;

groupshared uint groupPointLightIndexStartOffset;
groupshared uint groupSpotLightIndexStartOffset;
groupshared uint groupDecalIndexStartOffset;

groupshared uint groupPointLightList[MAX_NUM_LIGHTS_PER_TILE];
groupshared uint groupSpotLightList[MAX_NUM_LIGHTS_PER_TILE];
groupshared uint groupDecalList[MAX_NUM_DECALS_PER_TILE];



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

    float oc = getOuterCutoff(sl.innerAndOuterCutoff);
    result.radius = result.height * sqrt(1.f - oc * oc) / oc; // Same as height * tan(acos(oc)).
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

    for (int i = 0; i < 4 && result; ++i)
    {
        if (spotLightOutsidePlane(sl, frustum.planes[i]))
        {
            result = false;
        }
    }

    return result;
}

static bool decalOutsidePlane(decal_cb decal, light_culling_frustum_plane plane)
{
    float x = dot(decal.right, plane.N)     >= 0.f ? 1.f : -1.f;
    float y = dot(decal.up, plane.N)        >= 0.f ? 1.f : -1.f;
    float z = dot(decal.forward, plane.N)   >= 0.f ? 1.f : -1.f;

    float3 diag = x * decal.right + y * decal.up + z * decal.forward;

    float3 nPoint = decal.position + diag;
    return pointOutsidePlane(nPoint, plane);
}

static bool decalInsideFrustum(decal_cb decal, light_culling_view_frustum frustum, light_culling_frustum_plane nearPlane, light_culling_frustum_plane farPlane)
{
    bool result = true;

    if (decalOutsidePlane(decal, nearPlane)
        || decalOutsidePlane(decal, farPlane))
    {
        result = false;
    }

    for (int i = 0; i < 4 && result; ++i)
    {		
        if (decalOutsidePlane(decal, frustum.planes[i]))
        {
            result = false;
        }
    }	
        
    return result;
}

static void groupAppendPointLight(uint index)
{
    uint i;
    InterlockedAdd(groupPointLightCount, 1, i);
    if (i < MAX_NUM_LIGHTS_PER_TILE)
    {
        groupPointLightList[i] = index;
    }
}

static void groupAppendSpotLight(uint index)
{
    uint i;
    InterlockedAdd(groupSpotLightCount, 1, i);
    if (i < MAX_NUM_LIGHTS_PER_TILE)
    {
        groupSpotLightList[i] = index;
    }
}

static void groupAppendDecal(uint index)
{
    uint i;
    InterlockedAdd(groupDecalCount, 1, i);
    if (i < MAX_NUM_DECALS_PER_TILE)
    {
        groupDecalList[i] = index;
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

#if 0
    const uint numDecals = cb.numDecals;
    for (i = IN.groupIndex; i < numDecals; i += BLOCK_SIZE * BLOCK_SIZE)
    {
        decal_cb d = decals[i];
        if (decalInsideFrustum(d, groupFrustum, nearPlane, farPlane))
        {
            groupAppendDecal(i);
        }
    }
#endif


    GroupMemoryBarrierWithGroupSync();

    if (IN.groupIndex == 0)
    {
        InterlockedAdd(tiledCullingIndexCounter[0], groupPointLightCount, groupPointLightIndexStartOffset);
        InterlockedAdd(tiledCullingIndexCounter[1], groupSpotLightCount, groupSpotLightIndexStartOffset);
        InterlockedAdd(tiledCullingIndexCounter[2], groupDecalCount, groupDecalIndexStartOffset);

        tiledCullingGrid[IN.groupID.xy] = uint4(
            (groupPointLightIndexStartOffset << 16) | groupPointLightCount, 
            (groupSpotLightIndexStartOffset << 16) | groupSpotLightCount, 
            (groupDecalIndexStartOffset << 16) | groupDecalCount,
            0);
    }

    GroupMemoryBarrierWithGroupSync();

    for (i = IN.groupIndex; i < groupPointLightCount; i += BLOCK_SIZE * BLOCK_SIZE)
    {
        tiledPointLightIndexList[groupPointLightIndexStartOffset + i] = groupPointLightList[i];
    }

    for (i = IN.groupIndex; i < groupSpotLightCount; i += BLOCK_SIZE * BLOCK_SIZE)
    {
        tiledSpotLightIndexList[groupSpotLightIndexStartOffset + i] = groupSpotLightList[i];
    }

#if 0
    for (i = IN.groupIndex; i < groupDecalCount; i += BLOCK_SIZE * BLOCK_SIZE)
    {
        tiledDecalIndexList[groupDecalIndexStartOffset + i] = groupDecalList[i];
    }
#endif
}
