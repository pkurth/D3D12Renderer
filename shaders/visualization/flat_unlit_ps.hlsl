#include "visualization_rs.hlsli"

ConstantBuffer<visualization_cb> cb		: register(b1);

[RootSignature(FLAT_UNLIT_RS)]
float4 main() : SV_TARGET
{
	return cb.color;
}
