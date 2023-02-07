#include "grass_rs.hlsli"
#include "camera.hlsli"
#include "random.hlsli"

ConstantBuffer<grass_cb> cb				: register(b0);
ConstantBuffer<camera_cb> camera		: register(b1);
StructuredBuffer<grass_blade> blades	: register(t0);

struct vs_output
{
	float2 uv		: TEXCOORDS;
	float4 position : SV_POSITION;
};

static float getRelY(uint vertexID, uint lod)
{
	uint numLevels = cb.numVertices >> 1 >> lod;

	uint vertical = vertexID >> 1 >> lod;
	return (float)vertical * (1.f / numLevels);
}

vs_output main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
	grass_blade blade = blades[instanceID];

	uint leftRight = vertexID & 1;
	float relX = (float)leftRight * 2.f - 1.f;

	float relY0 = getRelY(vertexID, 0);
	float relY1 = getRelY(vertexID, 1);

	float relY = lerp(relY0, relY1, blade.lod());

	float2 uv = float2(relX, relY);


	float w = cb.halfWidth * (vertexID != cb.numVertices - 1);
	float x = relX * w;
	float y = relY * cb.height;

	float3 pos = float3(
		blade.facing.y * x,
		y, 
		blade.facing.x * x);


#if 0
	float swing = cos(cb.time - relY * 1.3f + fbm(blade.position.xz * 0.6f + 10000.f).x) * 0.7f + 0.2f;
	float3 xzOffset = cb.windDirection * swing * relY;
	pos += xzOffset;
#else
	float windStrength = fbm(blade.position.xz * 0.6f + cb.time * 0.3f + 10000.f).x + 0.6f;
	float3 xzOffset = cb.windDirection * windStrength * relY;
	pos += xzOffset;
#endif

	pos += blade.position;

	vs_output OUT;
	OUT.uv = uv;
	OUT.position = mul(camera.viewProj, float4(pos, 1.f));
	return OUT;
}
