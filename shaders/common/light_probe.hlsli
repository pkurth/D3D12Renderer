#ifndef LIGHT_PROBE_HLSLI
#define LIGHT_PROBE_HLSLI


#define LIGHT_PROBE_RESOLUTION	6
#define LIGHT_PROBE_TOTAL_RESOLUTION (LIGHT_PROBE_RESOLUTION + 2)

#define LIGHT_PROBE_DEPTH_RESOLUTION	14
#define LIGHT_PROBE_TOTAL_DEPTH_RESOLUTION (LIGHT_PROBE_DEPTH_RESOLUTION + 2)

#define NUM_RAYS_PER_PROBE 64

#define ENERGY_CONSERVATION 0.95f



static vec3 linearIndexTo3DIndex(uint32 i, uint32 countX, uint32 countY)
{
	uint32 slice = countX * countY;
	uint32 z = i / slice;
	uint32 xy = i % slice;
	uint32 y = xy / countX;
	uint32 x = xy % countX;

	return vec3((float)x, (float)y, (float)z);
}

struct light_probe_grid_cb
{
	vec3 minCorner;
	float cellSize;
	uint32 countX;
	uint32 countY;

	vec3 linearIndexTo3DIndex(uint32 i)
	{
		return ::linearIndexTo3DIndex(i, countX, countY);
	}

	vec3 linearIndexToPosition(uint32 i)
	{
		return linearIndexTo3DIndex(i) * cellSize + minCorner;
	}
};





static float signNotZero(float v)
{
	return (v >= 0.f) ? 1.f : -1.f;
}

static vec2 signNotZero(vec2 v)
{
	return vec2(signNotZero(v.x), signNotZero(v.y));
}

#ifdef HLSL
#define YX(vec) vec.yx
#else
#define YX(vec) vec2(vec.y, vec.x)
#endif


// [A Survey of Efficient Representations for Independent Unit Vectors]
// Maps between 3D direction and encoded vec2 [-1, 1]^2.
static vec2 encodeOctahedral(vec3 dir)
{
	float l1Norm = abs(dir.x) + abs(dir.y) + abs(dir.z);
	vec2 result = (1.f / l1Norm) * dir.xy;

	if (dir.z < 0.f)
	{
		result = (1.f - abs(YX(result))) * signNotZero(result);
	}

	return result;
}

static vec3 decodeOctahedral(vec2 o)
{
	vec3 v = vec3(o, 1.f - abs(o.x) - abs(o.y));
	if (v.z < 0.f)
	{
		v.xy = (1.f - abs(YX(v))) * signNotZero(v.xy);
	}
	return normalize(v);
}


#endif
