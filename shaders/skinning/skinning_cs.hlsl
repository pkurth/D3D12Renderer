#include "cs.hlsli"
#include "skinning_rs.hlsli"


#define BLOCK_SIZE 512

struct mesh_vertex
{
	float3 position;
	float2 uv;
	float3 normal;
	float3 tangent;
};

struct skinned_mesh_vertex
{
	float3 position;
	float2 uv;
	float3 normal;
	float3 tangent;
	uint skinIndices;
	uint skinWeights;
};

ConstantBuffer<skinning_cb> skinningCB				: register(b0);

StructuredBuffer<skinned_mesh_vertex> inputVertices	: register(t0);

StructuredBuffer<float4x4> skinningMatrices			: register(t1);
RWStructuredBuffer<mesh_vertex> outputVertices		: register(u0);


[RootSignature(SKINNING_RS)]
[numthreads(BLOCK_SIZE, 1, 1)]
void main(cs_input IN)
{
	uint index = IN.dispatchThreadID.x;

	if (index >= skinningCB.numVertices)
	{
		return;
	}

	skinned_mesh_vertex vertex = inputVertices[skinningCB.firstVertex + index];

	uint4 skinIndices = uint4(
		vertex.skinIndices >> 24,
		(vertex.skinIndices >> 16) & 0xFF,
		(vertex.skinIndices >> 8) & 0xFF,
		vertex.skinIndices & 0xFF
		);

	float4 skinWeights = float4(
		vertex.skinWeights >> 24,
		(vertex.skinWeights >> 16) & 0xFF,
		(vertex.skinWeights >> 8) & 0xFF,
		vertex.skinWeights & 0xFF
		) * (1.f / 255.f);

	skinWeights /= dot(skinWeights, (float4)1.f);

	skinIndices += skinningCB.firstJoint.xxxx;

	float4x4 s =
		skinningMatrices[skinIndices.x] * skinWeights.x +
		skinningMatrices[skinIndices.y] * skinWeights.y +
		skinningMatrices[skinIndices.z] * skinWeights.z +
		skinningMatrices[skinIndices.w] * skinWeights.w;

	float3 position = mul(s, float4(vertex.position, 1.f)).xyz;
	float3 normal = mul(s, float4(vertex.normal, 0.f)).xyz;
	float3 tangent = mul(s, float4(vertex.tangent, 0.f)).xyz;


	mesh_vertex output = { position, vertex.uv, normal, tangent };
	outputVertices[skinningCB.writeOffset + index] = output;
}
