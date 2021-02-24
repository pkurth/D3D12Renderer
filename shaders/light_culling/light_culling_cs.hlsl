#include "cs.hlsli"
#include "camera.hlsli"
#include "light_culling_rs.hlsli"
#include "light_source.hlsli"
#include "material.hlsli"

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
StructuredBuffer<pbr_decal_cb> decals               : register(t4);

RWTexture2D<uint4> tiledCullingGrid                 : register(u0);
RWStructuredBuffer<uint> tiledCullingIndexCounter   : register(u1);

RWStructuredBuffer<uint> tiledObjectsIndexList      : register(u2);


groupshared uint groupMinDepth;
groupshared uint groupMaxDepth;
groupshared light_culling_view_frustum groupFrustum;

groupshared uint groupObjectsStartOffset;

// Opaque.
groupshared uint groupObjectsListOpaque[MAX_NUM_INDICES_PER_TILE];
groupshared uint groupLightCountOpaque;

// Transparent.
groupshared uint groupObjectsListTransparent[MAX_NUM_INDICES_PER_TILE];
groupshared uint groupLightCountTransparent;




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

    float oc = sl.getOuterCutoff();
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

static bool decalOutsidePlane(pbr_decal_cb decal, light_culling_frustum_plane plane)
{
    float x = dot(decal.right, plane.N)     >= 0.f ? 1.f : -1.f;
    float y = dot(decal.up, plane.N)        >= 0.f ? 1.f : -1.f;
    float z = dot(decal.forward, plane.N)   >= 0.f ? 1.f : -1.f;

    float3 diag = x * decal.right + y * decal.up + z * decal.forward;

    float3 nPoint = decal.position + diag;
    return pointOutsidePlane(nPoint, plane);
}

static bool decalInsideFrustum(pbr_decal_cb decal, light_culling_view_frustum frustum, light_culling_frustum_plane nearPlane, light_culling_frustum_plane farPlane)
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



#define groupAppendLight(type, index)                               \
    {                                                               \
        uint v;                                                     \
        InterlockedAdd(groupLightCount##type, 1, v);                \
        if (v < MAX_NUM_LIGHTS_PER_TILE)                            \
        {                                                           \
            groupObjectsList##type[TILE_LIGHT_OFFSET + v] = index;  \
        }                                                           \
    }

// We flip the index, such that the first set bit corresponds to the front-most decal. We render the decals front to back, such that we can early exit when alpha >= 1.
#define groupAppendDecal(type, index)                               \
    {                                                               \
        const uint v = MAX_NUM_TOTAL_DECALS - index - 1;            \
        const uint bucket = v >> 5;                                 \
        const uint bit = v & ((1 << 5) - 1);                        \
        InterlockedOr(groupObjectsList##type[bucket], 1 << bit);    \
    }

#define groupAppendLightOpaque(index) groupAppendLight(Opaque, index)
#define groupAppendLightTransparent(index) groupAppendLight(Transparent, index)
#define groupAppendDecalOpaque(index) groupAppendDecal(Opaque, index)
#define groupAppendDecalTransparent(index) groupAppendDecal(Transparent, index)


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

        groupLightCountOpaque = 0;
        groupLightCountTransparent = 0;

        groupFrustum = frusta[IN.groupID.y * cb.numThreadGroupsX + IN.groupID.x];
    }

    // Initialize decal masks to zero.
    for (i = IN.groupIndex; i < NUM_DECAL_BUCKETS; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        groupObjectsListOpaque[i] = 0;
    }
    for (i = IN.groupIndex; i < NUM_DECAL_BUCKETS; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        groupObjectsListTransparent[i] = 0;
    }


    GroupMemoryBarrierWithGroupSync();




    // Determine minimum and maximum depth.
    uint2 screenSize;
    depthBuffer.GetDimensions(screenSize.x, screenSize.y);

    const float fDepth = depthBuffer[min(IN.dispatchThreadID.xy, screenSize - 1)];
    // Since the depth is between 0 and 1, we can perform min and max operations on the bit pattern as uint. This is because native HLSL does not support floating point atomics.
    const uint uDepth = asuint(fDepth);

    InterlockedMin(groupMinDepth, uDepth);
    InterlockedMax(groupMaxDepth, uDepth);

    GroupMemoryBarrierWithGroupSync();

    const float minDepth = asfloat(groupMinDepth);
    const float maxDepth = asfloat(groupMaxDepth);

    const float3 forward = camera.forward.xyz;
    const float3 cameraPos = camera.position.xyz;

    const float nearZ = depthBufferDepthToEyeDepth(minDepth, camera.projectionParams); // Positive.
    const float farZ  = depthBufferDepthToEyeDepth(maxDepth, camera.projectionParams); // Positive.

    const light_culling_frustum_plane cameraNearPlane = {  forward,  dot(forward, cameraPos + camera.projectionParams.x * forward) };
    const light_culling_frustum_plane nearPlane       = {  forward,  dot(forward, cameraPos + nearZ * forward) };
    const light_culling_frustum_plane farPlane        = { -forward, -dot(forward, cameraPos + farZ  * forward) };




    // Decals.
    const uint numDecals = cb.numDecals;
    for (i = IN.groupIndex; i < numDecals; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        pbr_decal_cb d = decals[i];
        if (decalInsideFrustum(d, groupFrustum, cameraNearPlane, farPlane))
        {
            groupAppendDecalTransparent(i);

            if (!decalOutsidePlane(d, nearPlane))
            {
                groupAppendDecalOpaque(i);
            }
        }
    }


    // Point lights.
    const uint numPointLights = cb.numPointLights;
    for (i = IN.groupIndex; i < numPointLights; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        point_light_cb pl = pointLights[i];
        if (pointLightInsideFrustum(pl, groupFrustum, cameraNearPlane, farPlane))
        {
            groupAppendLightTransparent(i);

            if (!pointLightOutsidePlane(pl, nearPlane))
            {
                groupAppendLightOpaque(i);
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();
    const uint numTilePointLightsOpaque = groupLightCountOpaque;
    const uint numTilePointLightsTransparent = groupLightCountTransparent;


    // Spot lights.
    const uint numSpotLights = cb.numSpotLights;
    for (i = IN.groupIndex; i < numSpotLights; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        spot_light_bounding_volume sl = getSpotLightBoundingVolume(spotLights[i]);
        if (spotLightInsideFrustum(sl, groupFrustum, cameraNearPlane, farPlane))
        {
            groupAppendLightTransparent(i);

            if (!spotLightOutsidePlane(sl, nearPlane))
            {
                groupAppendLightOpaque(i);
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();
    const uint numTileSpotLightsOpaque = groupLightCountOpaque - numTilePointLightsOpaque;
    const uint numTileSpotLightsTransparent = groupLightCountTransparent - numTilePointLightsTransparent;


    const uint totalIndexCountOpaque = groupLightCountOpaque + NUM_DECAL_BUCKETS;
    const uint totalIndexCountTransparent = groupLightCountTransparent + NUM_DECAL_BUCKETS;
    const uint totalIndexCount = totalIndexCountOpaque + totalIndexCountTransparent;
    if (IN.groupIndex == 0)
    {
        InterlockedAdd(tiledCullingIndexCounter[0], totalIndexCount, groupObjectsStartOffset);

        tiledCullingGrid[IN.groupID.xy] = uint4(
            groupObjectsStartOffset,
            (numTilePointLightsOpaque << 20) | (numTileSpotLightsOpaque << 10),
            groupObjectsStartOffset + totalIndexCountOpaque, // Transparent objects are directly after opaques.
            (numTilePointLightsTransparent << 20) | (numTileSpotLightsTransparent << 10)
        );
    }

    GroupMemoryBarrierWithGroupSync();

    const uint offsetOpaque = groupObjectsStartOffset + 0;
    for (i = IN.groupIndex; i < totalIndexCountOpaque; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        tiledObjectsIndexList[offsetOpaque + i] = groupObjectsListOpaque[i];
    }

    const uint offsetTransparent = groupObjectsStartOffset + totalIndexCountOpaque;
    for (i = IN.groupIndex; i < totalIndexCountTransparent; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        tiledObjectsIndexList[offsetTransparent + i] = groupObjectsListTransparent[i];
    }
}
