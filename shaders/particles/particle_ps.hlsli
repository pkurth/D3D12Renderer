#include "particles_rs.hlsli"

[RootSignature(PARTICLES_RENDERING_RS)]
float4 main(vs_output IN) : SV_TARGET
{
	return pixelShader(IN);
}
