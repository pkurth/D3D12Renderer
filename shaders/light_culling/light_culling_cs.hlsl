#include "cs.hlsli"
#include "camera.hlsli"
#include "light_culling_rs.hlsli"
#include "light_source.hlsli"

/*
    This shader culls lights and decals against screen space tiles.
    The emitted indices are handled differently for lights and decals.

    - For lights we simply write a dense list of indices per tile.
    - For decals we output a bit mask, where a 1 means that this decal influences this surface point. This limits the total number of decals per frame, but this way we don't need
      a sorting step, to properly draw the decals.
*/


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

groupshared uint groupObjectsList[MAX_NUM_INDICES_PER_TILE];
groupshared uint groupLightCount;


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

static void groupAppendLight(uint index)
{
    uint i;
    InterlockedAdd(groupLightCount, 1, i);
    if (i < MAX_NUM_LIGHTS_PER_TILE)
    {
        groupObjectsList[TILE_LIGHT_OFFSET + i] = index;
    }
}

static void groupAppendDecal(uint index)
{
#if 1
    index = MAX_NUM_TOTAL_DECALS - index - 1;   // This way the first set bit corresponds to the front-most decal. We render the decals front to back, such that we can early exit when alpha >= 1.
#endif

    const uint bucket = index >> 5;             // Divide by 32.
    const uint bit = index & ((1 << 5) - 1);    // Modulo 32.
    InterlockedOr(groupObjectsList[bucket], 1 << bit);

}

[RootSignature(LIGHT_CULLING_RS)]
[numthreads(LIGHT_CULLING_TILE_SIZE, LIGHT_CULLING_TILE_SIZE, 1)]
void main(cs_input IN)
{
    uint i;

    // Initialize.
    if (IN.groupIndex == 0)
    {
        groupMinDepth = asuint(0.9999999f);
        groupMaxDepth = 0;

        groupLightCount = 0;

        groupFrustum = frusta[IN.groupID.y * cb.numThreadGroupsX + IN.groupID.x];
    }

    // Initialize decal masks to zero.
    for (i = IN.groupIndex; i < NUM_DECAL_BUCKETS; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        groupObjectsList[i] = 0;
    }


    GroupMemoryBarrierWithGroupSync();




    // Determine minimum and maximum depth.
    uint2 screenSize;
    depthBuffer.GetDimensions(screenSize.x, screenSize.y);

    float fDepth = depthBuffer[min(IN.dispatchThreadID.xy, screenSize - 1)];
    // Since the depth is between 0 and 1, we can perform min and max operations on the bit pattern as uint. This is because native HLSL does not support floating point atomics.
    uint uDepth = asuint(fDepth);

    InterlockedMin(groupMinDepth, uDepth);
    InterlockedMax(groupMaxDepth, uDepth);

    GroupMemoryBarrierWithGroupSync();

    float minDepth = asfloat(groupMinDepth);
    float maxDepth = asfloat(groupMaxDepth);

    float3 forward = camera.forward.xyz;
    float3 cameraPos = camera.position.xyz;

    float nearZ = depthBufferDepthToEyeDepth(minDepth, camera.projectionParams); // Positive.
    float farZ  = depthBufferDepthToEyeDepth(maxDepth, camera.projectionParams); // Positive.

    light_culling_frustum_plane cameraNearPlane = {  forward,  dot(forward, cameraPos + camera.projectionParams.x * forward) };
    light_culling_frustum_plane nearPlane       = {  forward,  dot(forward, cameraPos + nearZ * forward) };
    light_culling_frustum_plane farPlane        = { -forward, -dot(forward, cameraPos + farZ  * forward) };




    // Decals.
    const uint numDecals = cb.numDecals;
    for (i = IN.groupIndex; i < numDecals; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        decal_cb d = decals[i];
        if (decalInsideFrustum(d, groupFrustum, nearPlane, farPlane))
        {
            groupAppendDecal(i);
        }
    }


    // Point lights.
    const uint numPointLights = cb.numPointLights;
    for (i = IN.groupIndex; i < numPointLights; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        point_light_cb pl = pointLights[i];
        if (pointLightInsideFrustum(pl, groupFrustum, nearPlane, farPlane))
        {
            groupAppendLight(i);
        }
    }

    GroupMemoryBarrierWithGroupSync();
    const uint numTilePointLights = groupLightCount;

    // Spot lights.
    const uint numSpotLights = cb.numSpotLights;
    for (i = IN.groupIndex; i < numSpotLights; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        spot_light_bounding_volume sl = getSpotLightBoundingVolume(spotLights[i]);
        if (spotLightInsideFrustum(sl, groupFrustum, nearPlane, farPlane))
        {
            groupAppendLight(i);
        }
    }

    GroupMemoryBarrierWithGroupSync();
    const uint numTileSpotLights = groupLightCount - numTilePointLights;


    const uint totalIndexCount = groupLightCount + NUM_DECAL_BUCKETS;
    if (IN.groupIndex == 0)
    {
        InterlockedAdd(tiledCullingIndexCounter[0], totalIndexCount, groupObjectsStartOffset);

        tiledCullingGrid[IN.groupID.xy] = uint2(
            groupObjectsStartOffset,
            (numTilePointLights << 20) | (numTileSpotLights << 10)
        );
    }

    GroupMemoryBarrierWithGroupSync();

    const uint offset = groupObjectsStartOffset;
    for (i = IN.groupIndex; i < totalIndexCount; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        tiledObjectsIndexList[offset + i] = groupObjectsList[i];
    }
}
