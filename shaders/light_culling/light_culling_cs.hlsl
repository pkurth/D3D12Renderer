#include "cs.hlsli"
#include "camera.hlsli"
#include "light_culling_rs.hlsli"
#include "light_source.hlsli"

#define BLOCK_SIZE 16
#define GROUP_SIZE (BLOCK_SIZE * BLOCK_SIZE)

ConstantBuffer<camera_cb> camera	                : register(b0);
ConstantBuffer<light_culling_cb> cb	                : register(b1);

Texture2D<float> depthBuffer                        : register(t0);
StructuredBuffer<light_culling_view_frustum> frusta : register(t1);

StructuredBuffer<point_light_cb> pointLights        : register(t2);
StructuredBuffer<spot_light_cb> spotLights          : register(t3);
StructuredBuffer<decal_cb> decals                   : register(t4);

RWTexture2D<uint2> tiledCullingGrid                 : register(u0);
RWStructuredBuffer<uint> tiledCullingIndexCounter   : register(u1);

RWStructuredBuffer<uint> tiledObjectsIndexList      : register(u2);


groupshared uint groupMinDepth;
groupshared uint groupMaxDepth;
groupshared light_culling_view_frustum groupFrustum;

groupshared uint groupObjectsStartOffset;

#define TOTAL_GROUP_LIST_SIZE (MAX_NUM_LIGHTS_PER_TILE + MAX_NUM_DECALS_PER_TILE)
groupshared uint groupObjectsList[TOTAL_GROUP_LIST_SIZE];
groupshared uint groupObjectsCount;

// MAX_NUM_DECALS_PER_TILE must equal GROUP_SIZE
groupshared uint groupE[MAX_NUM_DECALS_PER_TILE];
groupshared uint groupF[MAX_NUM_DECALS_PER_TILE];
groupshared uint groupTotalFalses;
groupshared uint highestDecalIndex;


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

static void groupAppendObject(uint index)
{
    uint i;
    InterlockedAdd(groupObjectsCount, 1, i);
    if (i < TOTAL_GROUP_LIST_SIZE)
    {
        groupObjectsList[i] = index;
    }
}

// This assumes that the decal indices are first in the group memory (which is the case).
// Adapted from here: https://github.com/jakemco/gpu-radix-sort/blob/master/RadixSort.hlsl
static void sortDecalIndices(uint numDecals, uint groupIndex)
{
    const uint numBits = 32;// firstbithigh(highestDecalIndex); // This was intended as an optimization. For reasons I am too lazy to investigate right now, this causes a crash.

    //[unroll(32)]
    for (int n = 0; n < numBits; ++n)
    {
        // Elements which have a 1 in the E-array will be put first. Therefore we write a 1 for each valid element (index valid), whose bit is a 0.
        groupE[groupIndex] = (groupIndex < numDecals)
            && ((groupObjectsList[groupIndex] >> n) == 0);

        GroupMemoryBarrierWithGroupSync();

        groupF[groupIndex] = (groupIndex != 0) 
            ? groupE[groupIndex - 1] 
            : 0;

        GroupMemoryBarrierWithGroupSync();

        // Prefix sum -> How many zeros are before me?
        for (uint i = 1; i < GROUP_SIZE; i <<= 1)
        {
            uint t = (groupIndex > i) 
                ? (groupF[groupIndex] + groupF[groupIndex - i]) 
                : groupF[groupIndex];
            
            GroupMemoryBarrierWithGroupSync();
            groupF[groupIndex] = t;
            GroupMemoryBarrierWithGroupSync();
        }

        if (groupIndex == 0) 
        {
            groupTotalFalses = groupE[GROUP_SIZE - 1] + groupF[GROUP_SIZE - 1];
        }

        GroupMemoryBarrierWithGroupSync();

        uint destination = groupE[groupIndex] 
            ? groupF[groupIndex] 
            : (groupIndex - groupF[groupIndex] + groupTotalFalses);

        uint temp = groupObjectsList[groupIndex];

        GroupMemoryBarrierWithGroupSync();

        groupObjectsList[destination] = temp;

        GroupMemoryBarrierWithGroupSync();
    }
}

[RootSignature(LIGHT_CULLING_RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(cs_input IN)
{
    float fDepth = depthBuffer[IN.dispatchThreadID.xy];
    // Since the depth is between 0 and 1, we can perform min and max operations on the bit pattern as uint. This is because native HLSL does not support floating point atomics.
    uint uDepth = asuint(fDepth);

    if (IN.groupIndex == 0)
    {
        groupMinDepth = asuint(0.9999999f);
        groupMaxDepth = 0;

        groupObjectsCount = 0;
        highestDecalIndex = 0;

        groupFrustum = frusta[IN.groupID.y * cb.numThreadGroupsX + IN.groupID.x];
    }

    GroupMemoryBarrierWithGroupSync();

    InterlockedMin(groupMinDepth, uDepth);
    InterlockedMax(groupMaxDepth, uDepth);

    GroupMemoryBarrierWithGroupSync();

    float minDepth = asfloat(groupMinDepth);
    float maxDepth = asfloat(groupMaxDepth);

    float3 forward = camera.forward.xyz;
    float3 cameraPos = camera.position.xyz;

    float nearZ = depthBufferDepthToEyeDepth(minDepth, camera.projectionParams); // Positive.
    float farZ  = depthBufferDepthToEyeDepth(maxDepth, camera.projectionParams); // Positive.

    light_culling_frustum_plane cameraNearPlane = { forward, dot(forward, cameraPos + camera.projectionParams.x * forward) };
    light_culling_frustum_plane nearPlane = { forward, dot(forward, cameraPos + nearZ * forward) };
    light_culling_frustum_plane farPlane = { -forward, -dot(forward, cameraPos + farZ * forward) };

    // Decals.
    const uint numDecals = cb.numDecals;
    for (uint i = IN.groupIndex; i < numDecals; i += BLOCK_SIZE * BLOCK_SIZE)
    {
        decal_cb d = decals[i];
        if (decalInsideFrustum(d, groupFrustum, nearPlane, farPlane))
        {
            groupAppendObject(i);
            InterlockedMax(highestDecalIndex, i);
        }
    }

    GroupMemoryBarrierWithGroupSync();
    const uint numTileDecals = groupObjectsCount;

    [branch]
    if (numTileDecals)
    {
        sortDecalIndices(numTileDecals, IN.groupIndex);
    }


    // Point lights.
    const uint numPointLights = cb.numPointLights;
    for (i = IN.groupIndex; i < numPointLights; i += BLOCK_SIZE * BLOCK_SIZE)
    {
        point_light_cb pl = pointLights[i];
        if (pointLightInsideFrustum(pl, groupFrustum, nearPlane, farPlane))
        {
            groupAppendObject(i);
        }
    }

    GroupMemoryBarrierWithGroupSync();
    const uint numTilePointLights = groupObjectsCount - numTileDecals;

    // Spot lights.
    const uint numSpotLights = cb.numSpotLights;
    for (i = IN.groupIndex; i < numSpotLights; i += BLOCK_SIZE * BLOCK_SIZE)
    {
        spot_light_bounding_volume sl = getSpotLightBoundingVolume(spotLights[i]);
        if (spotLightInsideFrustum(sl, groupFrustum, nearPlane, farPlane))
        {
            groupAppendObject(i);
        }
    }

    GroupMemoryBarrierWithGroupSync();
    const uint numTileSpotLights = groupObjectsCount - numTileDecals - numTilePointLights;


    if (IN.groupIndex == 0)
    {
        InterlockedAdd(tiledCullingIndexCounter[0], groupObjectsCount, groupObjectsStartOffset);

        tiledCullingGrid[IN.groupID.xy] = uint2(
            groupObjectsStartOffset,
            (numTilePointLights << 20) | (numTileSpotLights << 10) | (numTileDecals << 0)
        );
    }

    GroupMemoryBarrierWithGroupSync();

    const uint offset = groupObjectsStartOffset;
    const uint count = groupObjectsCount;
    for (i = IN.groupIndex; i < count; i += BLOCK_SIZE * BLOCK_SIZE)
    {
        tiledObjectsIndexList[offset + i] = groupObjectsList[i];
    }
}
