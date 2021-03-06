/* 
This file (as well as the actual mesh and amplification shader files)
is a HLSL port of the Vulkan shader written by Humus.
The original note follows:

* * * * * * * * * * * * Author's note * * * * * * * * * * * *\
*   _       _   _       _   _       _   _       _     _ _ _ _   *
*  |_|     |_| |_|     |_| |_|_   _|_| |_|     |_|  _|_|_|_|_|  *
*  |_|_ _ _|_| |_|     |_| |_|_|_|_|_| |_|     |_| |_|_ _ _     *
*  |_|_|_|_|_| |_|     |_| |_| |_| |_| |_|     |_|   |_|_|_|_   *
*  |_|     |_| |_|_ _ _|_| |_|     |_| |_|_ _ _|_|  _ _ _ _|_|  *
*  |_|     |_|   |_|_|_|   |_|     |_|   |_|_|_|   |_|_|_|_|    *
*                                                               *
*                     http://www.humus.name                     *
*                                                                *
* This file is a part of the work done by Humus. You are free to   *
* use the code in any way you like, modified, unmodified or copied   *
* into your own work. However, I expect you to respect these points:  *
*  - If you use this file and its contents unmodified, or use a major *
*    part of this file, please credit the author and leave this note. *
*  - For use in anything commercial, please request my approval.     *
*  - Share your work and ideas too as much as you can.             *
*                                                                *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


static const uint DEFAULT_SHIFT = 7; // 128x128x128 cubes by default. (1 << 7) == 128
static const uint DEFAULT_BALL_COUNT = 32;
static const uint MAX_BALL_COUNT = 128;

static const uint BALL_COUNT = DEFAULT_BALL_COUNT;
static const uint SHIFT = DEFAULT_SHIFT;

#define GRID_SIZE (1 << SHIFT)
#define STEP_SIZE (1.0f / float(GRID_SIZE))

struct constant_cb
{
	float4 balls[MAX_BALL_COUNT];
}; 

struct marching_cubes_lookup
{
	uint indices[4];
	uint vertices[3];
	uint triangleAndVertexCount;
};


struct mesh_payload
{
	uint meshletIDs[64];
};



ConstantBuffer<constant_cb> constants : register(b0);



// The isosurface is defined where this function returns zero.
float field(vec3 pos)
{
	// Compute metaballs
	float sum = -1.f;
	for (int i = 0; i < BALL_COUNT; i++)
	{
		float3 d = constants.balls[i].xyz - pos;
		sum += constants.balls[i].w / dot(d, d);
	}

	// Occasionally some balls swing across the outer edge of the volume, causing a visible cut.
	// For more visually pleasing results, we manipulate the field a bit with a fade gradient near the edge, so that the balls flatten against the edge instead.
	vec3 d = abs(pos - 0.5f.xxx);
	float edge_dist = max(d.x, max(d.y, d.z));
	sum = sum * min(0.5f * (0.5f - edge_dist), 1.f) - 0.001f;

	return sum;
}
