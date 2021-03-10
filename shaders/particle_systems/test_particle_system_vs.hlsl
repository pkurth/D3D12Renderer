#include "particles_rs.hlsli"
#include "random.hlsli" // TODO: Temporary!
#include "test_particle_system.hlsli"

ConstantBuffer<transform_cb> transform		: register(b0);
StructuredBuffer<particle_data> particles	: register(t0);
StructuredBuffer<uint> aliveList			: register(t1);

struct vs_input
{
	float3 position			: POSITION;
};

struct vs_output
{
	float3 color			: COLOR;
	float4 position			: SV_Position;
};

vs_output main(vs_input IN, uint instanceID	: SV_InstanceID)
{
	uint index = aliveList[instanceID];
	float3 pos = particles[index].position + IN.position;

	vs_output OUT;
	OUT.position = mul(transform.mvp, float4(pos, 1.f));
	OUT.color = IN.position * 0.5 + 0.5;
	return OUT;
}
