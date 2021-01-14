#include "transform.hlsli"

ConstantBuffer<transform_cb> transform : register(b0);

#define MESH_RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	"DENY_VERTEX_SHADER_ROOT_ACCESS |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
    "DENY_PIXEL_SHADER_ROOT_ACCESS)," \
    "RootConstants(num32BitConstants=32, b0)"

struct mesh_output
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
};

[outputtopology("triangle")]
[numthreads(1, 1, 1)]
[RootSignature(MESH_RS)]
void main(
	out vertices mesh_output outVerts[8], 
	out indices uint3 outIndices[12]
)
{
	const uint numVertices = 8;
	const uint numPrimitives = 12;

	SetMeshOutputCounts(numVertices, numPrimitives);

	outVerts[0].pos = mul(transform.mvp, float4(-0.5f, -0.5f, -0.5f, 1.f));
	outVerts[0].color = float3(0.f, 0.f, 0.f);

	outVerts[1].pos = mul(transform.mvp, float4(-0.5f, -0.5f, 0.5f, 1.f));
	outVerts[1].color = float3(0.f, 0.f, 1.f);

	outVerts[2].pos = mul(transform.mvp, float4(-0.5f, 0.5f, -0.5f, 1.f));
	outVerts[2].color = float3(0.f, 1.f, 0.f);

	outVerts[3].pos = mul(transform.mvp, float4(-0.5f, 0.5f, 0.5f, 1.f));
	outVerts[3].color = float3(0.f, 1.f, 1.f);

	outVerts[4].pos = mul(transform.mvp, float4(0.5f, -0.5f, -0.5f, 1.f));
	outVerts[4].color = float3(1.f, 0.f, 0.f);

	outVerts[5].pos = mul(transform.mvp, float4(0.5f, -0.5f, 0.5f, 1.f));
	outVerts[5].color = float3(1.f, 0.f, 1.f);

	outVerts[6].pos = mul(transform.mvp, float4(0.5f, 0.5f, -0.5f, 1.f));
	outVerts[6].color = float3(1.f, 1.f, 0.f);

	outVerts[7].pos = mul(transform.mvp, float4(0.5f, 0.5f, 0.5f, 1.f));
	outVerts[7].color = float3(1.f, 1.f, 1.f);

	
	outIndices[0]  = uint3(0, 2, 1);
	outIndices[1]  = uint3(1, 2, 3);
	outIndices[2]  = uint3(4, 5, 6);
	outIndices[3]  = uint3(5, 7, 6);
	outIndices[4]  = uint3(0, 1, 5);
	outIndices[5]  = uint3(0, 5, 4);
	outIndices[6]  = uint3(2, 6, 7);
	outIndices[7]  = uint3(2, 7, 3);
	outIndices[8]  = uint3(0, 4, 6);
	outIndices[9]  = uint3(0, 6, 2);
	outIndices[10] = uint3(1, 3, 7);
	outIndices[11] = uint3(1, 7, 5);
}
