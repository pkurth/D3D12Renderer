
static const uint DEFAULT_SHIFT = 7; // 128x128x128 cubes by default. (1 << 7) == 128
static const uint DEFAULT_BALL_COUNT = 32;
static const uint MAX_BALL_COUNT = 128;

static const uint BALL_COUNT = DEFAULT_BALL_COUNT;
static const uint SHIFT = DEFAULT_SHIFT;

#define GRID_SIZE (1 << SHIFT)
#define STEP_SIZE (1.0f / float(GRID_SIZE))

struct constant_cb
{
	vec4 balls[MAX_BALL_COUNT];
}; 

struct marching_cubes_lookup
{
	uint32 indices[4];
	uint32 vertices[3];
	uint32 triangleAndVertexCount;
};


struct mesh_payload
{
	uint meshletIDs[64];
};



ConstantBuffer<constant_cb> constants : register(b0);



// The isosurface is define where this function returns zero
float field(vec3 pos)
{
	// Compute metaballs
	float sum = -1.0f;
	for (int i = 0; i < BALL_COUNT; i++)
	{
		vec3 d = constants.balls[i].xyz - pos;
		sum += constants.balls[i].w / dot(d, d);
	}

	// Occasionally some balls swing across the outer edge of the volume, causing a visible cut.
	// For more visually pleasing results, we manipulate the field a bit with a fade gradient near the edge, so that the balls flatten against the edge instead.
	vec3 d = abs(pos - 0.5f.xxx);
	float edge_dist = max(d.x, max(d.y, d.z));
	sum = sum * min(0.5f * (0.5f - edge_dist), 1.0f) - 0.001f;

	return sum;
}
