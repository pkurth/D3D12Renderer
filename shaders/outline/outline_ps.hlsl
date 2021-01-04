#include "outline_rs.hlsli"

struct ps_input
{
	float2 uv				: TEXCOORDS;
	float4 screenPosition	: SV_POSITION;
};

ConstantBuffer<outline_drawer_cb> outline : register(b0);

Texture2D<uint2> stencil	: register(t0);


static const int selected = (1 << 0); // TODO: This must be the same as in renderer.

static uint sampleAt(int2 texCoords)
{
	if (texCoords.x < 0 || texCoords.y < 0 || texCoords.x >= outline.width || texCoords.y >= outline.height)
	{
		return 1;
	}
	return (stencil.Load(int3(texCoords, 0)).y & selected) != 0;
}

[RootSignature(OUTLINE_DRAWER_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	int2 texCoords = int2(IN.screenPosition.xy);

	uint s = 0;

	[unroll]
	for (int x = -2; x <= 2; ++x)
	{
		for (int y = -2; y <= 2; ++y)
		{
			s += sampleAt(texCoords + int2(x, y));
		}
	}

	if (s == 25)
	{
		discard;
	}

	return float4(1, 0, 0.f, 1.f);
}
