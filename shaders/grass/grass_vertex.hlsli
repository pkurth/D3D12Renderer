#ifndef GRASS_VERTEX_HLSLI
#define GRASS_VERTEX_HLSLI

#include "grass_rs.hlsli"
#include "random.hlsli"

#define DISABLE_FACING				0
#define DISABLE_BEND				0
#define DISABLE_WIND				0
#define DISABLE_ALL_GRASS_DYNAMICS  0


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
#if DISABLE_ALL_GRASS_DYNAMICS || DISABLE_WIND
	windStrength = 0.f;
#endif
	return windDirection * windStrength;
}

static float grassHeight(grass_blade blade, float baseHeight)
{
	return baseHeight + (fbm(blade.position.xz * 2.f + 10000.f, 3).x * 0.8f);
}

static float3 grassPosition(grass_blade blade, float2 uv, float height, float halfWidth, grass_bend_settings bend, float2 wind)
{
	float relX = uv.x;
	float relY = uv.y;
	float relY2 = relY * relY;

	float2 zy = float2(bend.controlPointZ, bend.controlPointY) * (2.f * relY - 2.f * relY2) + float2(bend.relTipOffsetZ, 1.f) * relY2;
#if DISABLE_ALL_GRASS_DYNAMICS || DISABLE_BEND
	zy = float2(0.f, relY);
#endif

	zy *= height;

	float x = relX * halfWidth;
	float y = zy.y;
	float z = zy.x;

	float2 facing = blade.facing;
#if DISABLE_ALL_GRASS_DYNAMICS || DISABLE_FACING
	facing = float2(0.f, 1.f);
#endif
	float3 position = float3(
		facing.y * x - facing.x * z,
		y,
		facing.x * x + facing.y * z);


	position.xz += wind * relY;
	position += blade.position;

	return position;
}

static float3 grassNormal(grass_blade blade, float2 uv, float height, grass_bend_settings bend, float2 wind)
{
	float relY = uv.y;

	float2 d_zy = float2(bend.controlPointZ, bend.controlPointY) * (2.f - 4.f * relY) + float2(bend.relTipOffsetZ, 1.f) * (2.f * relY);
#if DISABLE_ALL_GRASS_DYNAMICS || DISABLE_BEND
	d_zy = float2(0.f, 1.f);
#endif

	d_zy *= height;


	float2 facing = blade.facing;
#if DISABLE_ALL_GRASS_DYNAMICS || DISABLE_FACING
	facing = float2(0.f, 1.f);
#endif

	float z = d_zy.x;
	float y = d_zy.y;

	float d_x = height * (-facing.x * z) + wind.x;
	float d_y = height * y;
	float d_z = height * (facing.y * z) + wind.y;

	float3 tangent = float3(facing.y, 0.f, facing.x);
	float3 normal = cross(tangent, float3(d_x, d_y, d_z));

	return normal;
}

#endif

