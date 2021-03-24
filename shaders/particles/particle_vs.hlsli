#include "particles_rs.hlsli"

StructuredBuffer<particle_data> particles			: register(t0, space1);
StructuredBuffer<uint> aliveList					: register(t1, space1);


vs_output main(vs_input IN, uint instanceID	: SV_InstanceID)
{
	uint index = aliveList[instanceID];
	return vertexShader(IN, particles, index);
}
