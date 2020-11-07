#include "cs.hlsl"
#include "camera.hlsl"
#include "light_culling.hlsl"

#define BLOCK_SIZE 16
#define MAX_NUM_LIGHTS_PER_TILE 1024

ConstantBuffer<camera_cb> camera	: register(b0);
ConstantBuffer<light_culling_cb> cb	: register(b1);

Texture2D<float> depthBuffer                                : register(t0);

StructuredBuffer<light_culling_view_frustum> frusta         : register(t1);
StructuredBuffer<point_light_bounding_volume> pointLights   : register(t2);
StructuredBuffer<spot_light_bounding_volume> spotLights     : register(t3);

RWStructuredBuffer<uint> opaqueLightIndexCounter    : register(u0);
RWStructuredBuffer<uint> opaqueLightIndexList       : register(u1);
RWTexture2D<uint2> opaqueLightGrid                  : register(u2);

groupshared uint groupMinDepth;
groupshared uint groupMaxDepth;
groupshared light_culling_view_frustum groupFrustum;

groupshared uint opaqueLightCount;
groupshared uint opaqueLightIndexStartOffset;
groupshared uint opaqueLightList[MAX_NUM_LIGHTS_PER_TILE];


static bool pointLightOutsidePlane(point_light_bounding_volume pl, light_culling_frustum_plane plane)
{
    return dot(plane.N, pl.position) - plane.d < -pl.radius;
}

static bool pointLightInsideFrustum(point_light_bounding_volume pl, light_culling_view_frustum frustum, light_culling_frustum_plane nearPlane, light_culling_frustum_plane farPlane)
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
    float3 m = cross(cross(plane.N, sl.direction), sl.direction);
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

static void opaqueAppendLight(uint lightIndex)
{
    uint index;
    InterlockedAdd(opaqueLightCount, 1, index);
    if (index < MAX_NUM_LIGHTS_PER_TILE)
    {
        opaqueLightList[index] = lightIndex;
    }
}

[RootSignature(LIGHT_CULLING_RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(cs_input IN)
{
    uint2 texCoord = IN.dispatchThreadID.xy;
    float fDepth = depthBuffer.Load(uint3(texCoord, 0)).r;
    // Since the depth is between 0 and 1, we can perform min and max operations on the bit pattern as uint. This is because native HLSL does not support floating point atomics.
    uint uDepth = asuint(fDepth);

    if (IN.groupIndex == 0) // Avoid contention by other threads in the group.
    {
        groupMinDepth = 0xffffffff;
        groupMaxDepth = 0;
        opaqueLightCount = 0;
        groupFrustum = frusta[IN.groupID.x + (IN.groupID.y * cb.numThreadGroupsX)];
    }

    GroupMemoryBarrierWithGroupSync();

    InterlockedMin(groupMinDepth, uDepth);
    InterlockedMax(groupMaxDepth, uDepth);

    GroupMemoryBarrierWithGroupSync();

    float fMinDepth = asfloat(groupMinDepth);
    float fMaxDepth = asfloat(groupMaxDepth);

    float3 forward = camera.forward.xyz;

    float nearZ = depthBufferDepthToLinearWorldDepthEyeToFarPlane(fMinDepth, camera.projectionParams);
    float farZ = depthBufferDepthToLinearWorldDepthEyeToFarPlane(fMaxDepth, camera.projectionParams);

    light_culling_frustum_plane cameraNearPlane = { forward, -camera.projectionParams.x };
    light_culling_frustum_plane nearPlane = { forward, -nearZ };
    light_culling_frustum_plane farPlane = { -forward, farZ };


    uint numPointLights = cb.numPointLights;
    for (uint i = IN.groupIndex; i < numPointLights; i += BLOCK_SIZE * BLOCK_SIZE)
    {
        point_light_bounding_volume pl = pointLights[i];
        if (pointLightInsideFrustum(pl, groupFrustum, nearPlane, farPlane))
        {
            opaqueAppendLight(i);
        }
    }

    // TODO: Spotlights.
    uint numSpotLights = 0;

    GroupMemoryBarrierWithGroupSync();

    if (IN.groupIndex == 0)
    {
        InterlockedAdd(opaqueLightIndexCounter[0], opaqueLightCount, opaqueLightIndexStartOffset);
        opaqueLightGrid[IN.groupID.xy] = uint2(opaqueLightIndexStartOffset, opaqueLightCount);

        //InterlockedAdd(t_LightIndexCounter[0], t_LightCount, t_LightIndexStartOffset);
        //t_LightGrid[IN.groupID.xy] = uint2(t_LightIndexStartOffset, t_LightCount);
    }

    GroupMemoryBarrierWithGroupSync();

    for (i = IN.groupIndex; i < opaqueLightCount; i += BLOCK_SIZE * BLOCK_SIZE)
    {
        opaqueLightIndexList[opaqueLightIndexStartOffset + i] = opaqueLightList[i];
    }
}
