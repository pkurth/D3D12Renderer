#include "cs.hlsli"
#include "skinning_rs.hlsli"


#define BLOCK_SIZE 512


struct mesh_position
{
	float3 position;
};

struct mesh_others
{
	float2 uv;
	float3 normal;
	float3 tangent;
};

struct skinned_mesh_position
{
	float3 position;
};

struct skinned_mesh_others
{
	float2 uv;
	float3 normal;
	float3 tangent;
	uint skinIndices;
	uint skinWeights;
};

ConstantBuffer<skinning_cb> skinningCB					: register(b0);

StructuredBuffer<skinned_mesh_position> inputPositions	: register(t0);
StructuredBuffer<skinned_mesh_others> inputOthers		: register(t1);

StructuredBuffer<float4x4> skinningMatrices				: register(t2);
RWStructuredBuffer<mesh_position> outputPositions		: register(u0);
RWStructuredBuffer<mesh_others> outputOthers			: register(u1);


[RootSignature(SKINNING_RS)]
[numthreads(BLOCK_SIZE, 1, 1)]
void main(cs_input IN)
{
	uint index = IN.dispatchThreadID.x;

	if (index >= skinningCB.numVertices)
	{
		return;
	}

	float3 inputPosition = inputPositions[skinningCB.firstVertex + index].position;
	skinned_mesh_others inputOther = inputOthers[skinningCB.firstVertex + index];

	uint4 skinIndices = uint4(
		inputOther.skinIndices >> 24,
		(inputOther.skinIndices >> 16) & 0xFF,
		(inputOther.skinIndices >> 8) & 0xFF,
		inputOther.skinIndices & 0xFF
		);

	float4 skinWeights = float4(
		inputOther.skinWeights >> 24,
		(inputOther.skinWeights >> 16) & 0xFF,
		(inputOther.skinWeights >> 8) & 0xFF,
		inputOther.skinWeights & 0xFF
		) * (1.f / 255.f);

	skinWeights /= dot(skinWeights, (float4)1.f);

	skinIndices += skinningCB.firstJoint.xxxx;

	float4x4 s =
		skinningMatrices[skinIndices.x] * skinWeights.x +
		skinningMatrices[skinIndices.y] * skinWeights.y +
		skinningMatrices[skinIndices.z] * skinWeights.z +
		skinningMatrices[skinIndices.w] * skinWeights.w;

	float3 position = mul(s, float4(inputPosition, 1.f)).xyz;
	float3 normal = mul(s, float4(inputOther.normal, 0.f)).xyz;
	float3 tangent = mul(s, float4(inputOther.tangent, 0.f)).xyz;


	mesh_position outputPosition = { position };
	mesh_others outputOther = { inputOther.uv, normal, tangent };
	outputPositions[skinningCB.writeOffset + index] = outputPosition;
	outputOthers[skinningCB.writeOffset + index] = outputOther;
}
