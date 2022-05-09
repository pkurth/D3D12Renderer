#ifndef LIGHT_PROBE_RS_H
#define LIGHT_PROBE_RS_H


#define LIGHT_PROBE_RESOLUTION	6
#define LIGHT_PROBE_TOTAL_RESOLUTION (LIGHT_PROBE_RESOLUTION + 2)

#define LIGHT_PROBE_DEPTH_RESOLUTION	14
#define LIGHT_PROBE_TOTAL_DEPTH_RESOLUTION (LIGHT_PROBE_DEPTH_RESOLUTION + 2)





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


struct light_probe_visualization_cb
{
	mat4 mvp;
	vec2 uvScale;
	float cellSize;
	uint32 countX;
	uint32 countY;
};

#define LIGHT_PROBE_GRID_VISUALIZATION_RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	"DENY_HULL_SHADER_ROOT_ACCESS |" \
	"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
	"DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
	"RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX), " \
	"DescriptorTable(SRV(t0, numDescriptors=1), visibility=SHADER_VISIBILITY_PIXEL), " \
	"StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR," \
        "visibility=SHADER_VISIBILITY_PIXEL)"

#define LIGHT_PROBE_GRID_VISUALIZATION_RS_CB			0
#define LIGHT_PROBE_GRID_VISUALIZATION_RS_IRRADIANCE	1




struct light_probe_trace_cb
{
	vec3 minCorner;
	float cellSize;
	uint32 countX;
};
