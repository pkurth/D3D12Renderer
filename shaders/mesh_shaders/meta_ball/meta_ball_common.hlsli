

static const uint DEFAULT_SHIFT = 7;
static const uint DEFAULT_BALL_COUNT = 32;
static const uint MAX_BALL_COUNT = 128;

static const uint BALL_COUNT = DEFAULT_BALL_COUNT;
static const uint SHIFT = DEFAULT_SHIFT;

#define GRID_SIZE (1 << SHIFT)
#define STEP_SIZE (1.f / float(GRID_SIZE))

struct constant_cb
{
	float4 balls[MAX_BALL_COUNT];
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

