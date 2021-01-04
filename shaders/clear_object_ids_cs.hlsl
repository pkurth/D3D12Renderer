#define RS \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 2), " \
    "DescriptorTable( UAV(u0, numDescriptors = 1, flags = DESCRIPTORS_VOLATILE) )"

#include "cs.hlsli"

#define BLOCK_SIZE 16

cbuffer clear_object_ids_cb : register(b0)
{
    uint width, height;
};

RWTexture2D<uint> objectIDs : register(u0);

[RootSignature(RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(cs_input IN)
{
    uint2 texCoord = IN.dispatchThreadID.xy;
    if (texCoord.x >= width || texCoord.y >= height) return;

    objectIDs[texCoord] = 0xFFFFFFFF; // -1
}
