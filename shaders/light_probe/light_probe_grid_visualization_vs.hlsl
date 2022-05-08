#include "light_probe_rs.hlsli"

ConstantBuffer<light_probe_visualization_cb> cb : register(b0);

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
	uint slice = cb.countX * cb.countY;
	uint z = IN.instanceID / slice;
	uint xy = IN.instanceID % slice;
	uint y = xy / cb.countX;
	uint x = xy % cb.countX;

	float3 index = float3(x, y, z);

	float radius = 0.1f;

	vs_output OUT;
	OUT.normal = IN.position;
	OUT.position = mul(cb.mvp, float4(IN.position * radius + index * cb.cellSize, 1.f));
	OUT.uvOffset = float2(y * cb.countX + x, z);
	OUT.uvScale = cb.uvScale;
	return OUT;
}
