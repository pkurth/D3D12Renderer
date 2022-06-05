#ifndef MATH_H
#define MATH_H


#define M_PI 3.14159265359f
#define M_PI_OVER_2 (M_PI * 0.5f)
#define M_PI_OVER_180 (M_PI / 180.f)
#define M_180_OVER_PI (180.f / M_PI)
#define M_SQRT_PI 1.77245385090f
#define M_TAU 6.28318530718f

#define M_INV_PI 0.3183098861837906f
#define M_INV_2_PI 0.1591549430918953f
#define M_INV_ATAN float2(M_INV_2_PI, M_INV_PI)


static float inverseLerp(float l, float u, float v) { return (v - l) / (u - l); }
static float remap(float v, float oldL, float oldU, float newL, float newU) { return lerp(newL, newU, inverseLerp(oldL, oldU, v)); }

static float solidAngleOfSphere(float radius, float distance)
{
	// The angular radius of a sphere is p = arcsin(radius / d). 
	// The solid angle of a circular cap (projection of sphere) is 2pi * (1 - cos(p)).
	// cos(arcsin(x)) = sqrt(1 - x*x)

	float s = radius / distance;
	return 2.f * M_PI * (1.f - sqrt(max(0.f, 1.f - s * s)));
}

static float square(float v) { return v * v; }
static float pow3(float v) { return v * v * v; }

static float maxComponent(float3 a) { return max(a.x, max(a.y, a.z)); }

static uint flatten2D(uint2 coord, uint2 dim)
{
	return coord.x + coord.y * dim.x;
}

static uint flatten2D(uint2 coord, uint width)
{
	return coord.x + coord.y * width;
}

// Flattened array index to 2D array index.
static uint2 unflatten2D(uint idx, uint2 dim)
{
	return uint2(idx % dim.x, idx / dim.x);
}

static uint2 unflatten2D(uint idx, uint width)
{
	return uint2(idx % width, idx / width);
}

inline bool isSaturated(float a) { return a == saturate(a); }
inline bool isSaturated(float2 a) { return isSaturated(a.x) && isSaturated(a.y); }
inline bool isSaturated(float3 a) { return isSaturated(a.x) && isSaturated(a.y) && isSaturated(a.z); }
inline bool isSaturated(float4 a) { return isSaturated(a.x) && isSaturated(a.y) && isSaturated(a.z) && isSaturated(a.w); }


static float2 quadFromVertexID(uint vertexID)
{
	return float2((vertexID << 1) & 2, vertexID & 2) - 1.f;
}


#define pack_float	4
#define pack_vec4	1
#define pack_float4	1

#define pack(num, data_type) num / pack_##data_type

// maxNumPoints must be a multiple of 4!
// data_type can currently be either float or vec4.
// The values are internally stored packed. This is because HLSL packs arrays in constant buffers
// in float4s. See https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules.
// In order to conform to C++'s packing rules, we declare an array of float4s and cast it to an array of floats.
#define spline(data_type, maxNumPoints) catmull_rom_spline_##data_type##_##maxNumPoints

#define defineSpline(data_type, maxNumPoints)															\
struct spline(data_type, maxNumPoints)																	\
{																										\
	float4 packedTs[maxNumPoints / 4];																	\
	float4 packedValues[pack(maxNumPoints, data_type)];													\
																										\
	inline data_type evaluate(int numActualPoints, float t)												\
	{																									\
		float ts[maxNumPoints] = (float[maxNumPoints])packedTs;											\
		data_type values[maxNumPoints] = (data_type[maxNumPoints])packedValues;							\
																										\
		int k = 0;																						\
		[unroll(maxNumPoints)]																			\
		while (ts[k] < t)																				\
		{																								\
			++k;																						\
		}																								\
																										\
		const float h1 = inverseLerp(ts[k - 1], ts[k], t);												\
		const float h2 = h1 * h1;																		\
		const float h3 = h2 * h1;																		\
		const float4 h = float4(h3, h2, h1, 1.f);														\
																										\
		data_type result = (data_type)0;																\
																										\
		int m = numActualPoints - 1;																	\
		result += values[clamp(k - 2, 0, m)] * dot(float4(-1, 2, -1, 0), h);							\
		result += values[k - 1] * dot(float4(3, -5, 0, 2), h);											\
		result += values[k] * dot(float4(-3, 4, 1, 0), h);												\
		result += values[clamp(k + 1, 0, m)] * dot(float4(1, -1, 0, 0), h);								\
																										\
		result *= 0.5f;																					\
																										\
		return result;																					\
	}																									\
};

static inline uint packHalfs(float left, float right)
{
	return (f32tof16(left) << 16) | f32tof16(right);
}

static inline float unpackHalfsLeft(uint x)
{
	return f16tof32(x >> 16);
}

static inline float unpackHalfsRight(uint x)
{
	return f16tof32(x);
}

static inline void unpackHalfs(uint x, out float left, out float right)
{
	left = unpackHalfsLeft(x);
	right = unpackHalfsRight(x);
}

static inline float cross2(float2 a, float2 b)
{ 
	return a.x * b.y - a.y * b.x; 
}

#endif
