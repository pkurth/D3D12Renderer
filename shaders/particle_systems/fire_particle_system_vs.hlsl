#include "particles_rs.hlsli"
#include "random.hlsli" // TODO: Temporary!
#include "fire_particle_system.hlsli"
#include "camera.hlsli"

ConstantBuffer<particle_atlas_cb> atlas				: register(b0);
ConstantBuffer<camera_cb> camera					: register(b1);
StructuredBuffer<particle_data> particles			: register(t0);
StructuredBuffer<uint> aliveList					: register(t1);

struct vs_input
{
	float3 position			: POSITION;
};

struct vs_output
{
	float relLife			: RELLIFE;
	float2 uv				: TEXCOORDS;
	float4 position			: SV_Position;
};

vs_output main(vs_input IN, uint instanceID	: SV_InstanceID)
{
	uint index = aliveList[instanceID];
	float3 pos = particles[index].position;

	float relLife = 1.f - saturate(particles[index].life / particles[index].maxLife);
	relLife = sqrt(relLife);
	uint atlasIndex = relLife * atlas.total;
	uint x = atlasIndex % atlas.cols;
	uint y = atlasIndex / atlas.cols;

	float2 localPosition = IN.position.xy / saturate(smoothstep(relLife, 0.f, 0.3f) + 0.8f);
	pos += localPosition.x * camera.right.xyz + localPosition.y * camera.up.xyz;

	float2 uv0 = float2(x * atlas.invCols, y * atlas.invRows);
	float2 uv1 = float2((x + 1) * atlas.invCols, (y + 1) * atlas.invRows);


	vs_output OUT;
	OUT.position = mul(camera.viewProj, float4(pos, 1.f));
	OUT.uv = lerp(uv0, uv1, IN.position.xy * 0.5f + 0.5f);
	OUT.relLife = relLife;
	return OUT;
}
