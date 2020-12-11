#include "outline_rs.hlsli"

struct ps_input
{
	float2 uv				: TEXCOORDS;
	float4 screenPosition	: SV_POSITION;
};

Texture2D<uint2> stencil	: register(t0);

[RootSignature(OUTLINE_DRAWER_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	int2 texCoords = int2(IN.screenPosition.xy);

	int selected = (1 << 1); // TODO: This must be the same as in renderer.

	uint s = 0;
	for (int x = -2; x <= 2; ++x)
	{
		for (int y = -2; y <= 2; ++y)
		{
			s += (stencil.Load(int3(texCoords + int2(x, y), 0)).y & selected) != 0;
		}
	}

	if (s == 25)
	{
		discard;
	}

	return float4(1, 0, 0.f, 1.f);
}
