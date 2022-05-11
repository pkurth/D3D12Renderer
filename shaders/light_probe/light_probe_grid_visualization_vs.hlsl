#include "light_probe_rs.hlsli"

ConstantBuffer<light_probe_grid_visualization_cb> cb : register(b0);

struct vs_input
{
	float3 position		: POSITION;
	uint instanceID		: SV_InstanceID;
};

struct vs_output
{
	float2 uvOffset			: UV_OFFSET;
	float2 uvScale			: UV_SCALE;
	float3 normal			: NORMAL;
	float4 position			: SV_POSITION;
};

vs_output main(vs_input IN)
{
	float3 index = linearIndexTo3DIndex(IN.instanceID, cb.countX, cb.countY);

	float radius = 0.1f;

	vs_output OUT;
	OUT.normal = IN.position;
	OUT.position = mul(cb.mvp, float4(IN.position * radius + index * cb.cellSize, 1.f));
	OUT.uvOffset = float2(index.y * cb.countX + index.x, index.z);
	OUT.uvScale = cb.uvScale;
	return OUT;
}
