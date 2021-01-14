#include "transform.hlsli"

#define MESH_RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	"DENY_VERTEX_SHADER_ROOT_ACCESS |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
    "DENY_PIXEL_SHADER_ROOT_ACCESS)," \
    "RootConstants(num32BitConstants=32, b0), " \
    "SRV(t0), " \
    "SRV(t1), " \
    "SRV(t2), " \
    "SRV(t3)"

struct mesh_output
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
};



struct mesh_vertex
{
    float3 position;
    float3 normal;
};

struct meshlet_info
{
    uint numVertices;
    uint firstVertex;
    uint numPrimitives;
    uint firstPrimitive;
};

ConstantBuffer<transform_cb> transform      : register(b0);

StructuredBuffer<mesh_vertex> vertices      : register(t0);
StructuredBuffer<meshlet_info> meshlets     : register(t1);
StructuredBuffer<uint> uniqueVertexIndices  : register(t2);
StructuredBuffer<uint> primitiveIndices     : register(t3);



[outputtopology("triangle")]
[numthreads(128, 1, 1)]
[RootSignature(MESH_RS)]
void main(
    in uint groupThreadID : SV_GroupThreadID, 
    in uint groupID : SV_GroupID, 
    out vertices mesh_output outVerts[64], 
    out indices uint3 outIndices[126]
)
{
    meshlet_info m = meshlets[groupID];

    SetMeshOutputCounts(m.numVertices, m.numPrimitives);

    if (groupThreadID < m.numPrimitives)
    {
        uint packedPrimitive = primitiveIndices[m.firstPrimitive + groupThreadID];

        // Unpacks a 10 bits per index triangle from a 32-bit uint.
        outIndices[groupThreadID] = uint3(packedPrimitive & 0x3FF, (packedPrimitive >> 10) & 0x3FF, (packedPrimitive >> 20) & 0x3FF);
    }

    if (groupThreadID < m.numVertices)
    {
        uint vertexIndex = uniqueVertexIndices[m.firstVertex + groupThreadID];
        uint meshletIndex = groupID;

        // Color based on meshlet index for visualization.
        outVerts[groupThreadID].color = float3(
            float(meshletIndex & 1),
            float(meshletIndex & 3) / 4,
            float(meshletIndex & 7) / 8);

        outVerts[groupThreadID].pos = mul(transform.mvp, float4(vertices[vertexIndex].position, 1.f));
    }
}
