#include "tree_rs.hlsli"
#include "random.hlsli"
#include "camera.hlsli"

StructuredBuffer<float4x4> transforms	: register(t0);
ConstantBuffer<tree_cb> cb				: register(b1);
ConstantBuffer<camera_cb> camera		: register(b1, space1);

struct vs_input
{
	float3 position		: POSITION;
	float2 uv			: TEXCOORDS;
	float3 normal		: NORMAL;
	float3 tangent		: TANGENT;
	float4 color		: COLOR;

	uint instanceID		: SV_InstanceID;
};

struct vs_output
{
	float2 uv				: TEXCOORDS;
	float3x3 tbn			: TANGENT_FRAME;
	float3 worldPosition	: POSITION;

	float4 position			: SV_POSITION;
};

vs_output main(vs_input IN)
{
	const float2 windDirection = normalize(float2(1.f, 1.f));
	const float3 treeDistances = IN.color.rgb;

	const float movementScale = 15.f;
	const float bend = pow(2, 1.f / 4.f);

	float4x4 m = transforms[IN.instanceID];
	float4 worldPosition = mul(m, float4(IN.position, 1.f));

	float trunkSwing = fbm(cb.time * 0.1f, 2).x * 0.4f + 0.6f;
	float2 trunkMovement = windDirection * (movementScale * trunkSwing * pow(treeDistances.x, bend));

	float branchSwing = fbm(worldPosition.xz * 0.03f + cb.time * 0.3f, 3).x * 0.3f + 0.1f;
	float2 branchMovement = windDirection * (movementScale * branchSwing * pow(treeDistances.y, bend));

	float leafSwing = fbm(worldPosition.xz + cb.time * 3.f, 3).x * 0.3f + 0.1f;
	float2 leafMovement = windDirection * (movementScale * leafSwing * pow(treeDistances.z, 1.f));



	worldPosition.xz += trunkMovement + branchMovement + leafMovement;



	vs_output OUT;
	OUT.position = mul(camera.viewProj, worldPosition);

	OUT.uv = IN.uv;
	OUT.worldPosition = worldPosition.xyz;

	float3 normal = normalize(mul(m, float4(IN.normal, 0.f)).xyz);
	float3 tangent = normalize(mul(m, float4(IN.tangent, 0.f)).xyz);
	float3 bitangent = normalize(cross(normal, tangent));
	OUT.tbn = float3x3(tangent, bitangent, normal);

	return OUT;
}
