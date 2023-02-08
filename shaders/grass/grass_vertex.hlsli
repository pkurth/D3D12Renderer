#ifndef GRASS_VERTEX_HLSLI
#define GRASS_VERTEX_HLSLI

#include "grass_rs.hlsli"
#include "random.hlsli"

#define DISABLE_ALL_GRASS_DYNAMICS 0


static float getRelY(uint vertexID, uint numVertices, uint lod)
{
	uint numLevels = numVertices >> 1 >> lod;

	uint vertical = vertexID >> 1 >> lod;
	return (float)vertical * (1.f / numLevels);
}

struct grass_bend_settings
{
	float relTipOffsetZ;
	float controlPointZ;
	float controlPointY;
};

static float2 grassUV(grass_blade blade, uint vertexID, uint numVertices)
{
	uint leftRight = vertexID & 1;
	float relX = (float)leftRight * 2.f - 1.f;
	relX *= (vertexID != numVertices - 1);

	float relY0 = getRelY(vertexID, numVertices, 0);
	float relY1 = getRelY(vertexID, numVertices, 1);

	float relY = lerp(relY0, relY1, blade.lod());

	float2 uv = float2(relX, relY);

	return uv;
}

static float2 grassWind(grass_blade blade, float2 windDirection, float time)
{
	float windStrength = fbm(blade.position.xz * 0.6f + time * 0.3f + 10000.f).x + 0.6f;
#if DISABLE_ALL_GRASS_DYNAMICS
	windStrength = 0.f;
#endif
	return windDirection * windStrength;
}

static float3 grassPosition(grass_blade blade, float2 uv, float height, float halfWidth, grass_bend_settings bend, float2 wind)
{
	float relX = uv.x;
	float relY = uv.y;
	float relY2 = relY * relY;

	float2 yz = float2(bend.controlPointY, bend.controlPointZ) * (2.f * relY - 2.f * relY2) + float2(1.f, bend.relTipOffsetZ) * relY2;
#if DISABLE_ALL_GRASS_DYNAMICS
	yz = float2(relY, 0.f);
#endif

	height = height + (fbm(blade.position.xz * 2.f + 10000.f, 3).x * 0.8f);
	yz *= height;

	float x = relX * halfWidth;
	float y = yz.x;
	float z = yz.y;

	float3 position = float3(
		blade.facing.y * x - blade.facing.x * z,
		y,
		blade.facing.x * x + blade.facing.y * z);


	position.xz += wind * relY;
	position += blade.position;

	return position;
}

static float3 grassNormal(grass_blade blade, float2 uv, grass_bend_settings bend, float2 wind)
{
	float relY = uv.y;

	float2 d_yz = float2(bend.controlPointY, bend.controlPointZ) * (2.f - 4.f * relY) + float2(1.f, bend.relTipOffsetZ) * (2.f * relY);
#if DISABLE_ALL_GRASS_DYNAMICS
	d_yz = float2(1.f, 0.f);
#endif

	float nx = 0.f;
	float ny = -d_yz.y;
	float nz = d_yz.x;

	float3 normal = float3(
		blade.facing.y * nx - blade.facing.x * nz,
		ny,
		blade.facing.x * nx + blade.facing.y * nz);

	normal += float3(-wind.x, 1.f, -wind.y);
	normal = lerp(normal, float3(0.f, 1.f, 0.f), 0.6f);

	return normal;
}

#endif

