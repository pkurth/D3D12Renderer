#pragma once


#define ROW_MAJOR 0
#define DIRECTX_COORDINATE_SYSTEM 1


#include <cmath>
#include <cfloat>

#include "simd.h"

#define M_PI 3.14159265359f
#define M_PI_OVER_180 (M_PI / 180.f)
#define M_180_OVER_PI (180.f / M_PI)
#define M_TAU 6.28318530718f

#define EPSILON 1e-6f

#define deg2rad(deg) ((deg) * M_PI_OVER_180)
#define rad2deg(rad) ((rad) * M_180_OVER_PI)

static float lerp(float l, float u, float t) { return l + t * (u - l); }
static float inverseLerp(float l, float u, float v) { return (v - l) / (u - l); }
static float remap(float v, float oldL, float oldU, float newL, float newU) { return lerp(newL, newU, inverseLerp(oldL, oldU, v)); }
static float clamp(float v, float l, float u) { return min(u, max(l, v)); }
static uint32 clamp(uint32 v, uint32 l, uint32 u) { return min(u, max(l, v)); }
static int32 clamp(int32 v, int32 l, int32 u) { return min(u, max(l, v)); }
static float clamp01(float v) { return clamp(v, 0.f, 1.f); }
static uint32 bucketize(uint32 problemSize, uint32 bucketSize) { return (problemSize + bucketSize - 1) / bucketSize; }

static constexpr bool isPowerOfTwo(uint32 i)
{
	return (i & (i - 1)) == 0;
}

// Constexpr-version of _BitScanForward.
static constexpr uint32 indexOfLeastSignificantSetBit(uint32 mask)
{
	uint32 count = 0;

	if (mask != 0)
	{
		while ((mask & 1) == 0)
		{
			mask >>= 1;
			++count;
		}
	}
	return count;
}


struct half
{
	uint16 h;

	half() {}
	half(uint16 in) : h(in) {}
	half(float f);

	operator float();
};

half operator+(half a, half b);
half& operator+=(half& a, half b);

half operator-(half a, half b);
half& operator-=(half& a, half b);

half operator*(half a, half b);
half& operator*=(half& a, half b);

half operator/(half a, half b);
half& operator/=(half& a, half b);



struct vec2
{
	float x, y;

	vec2() {}
	vec2(float v) : vec2(v, v) {}
	vec2(float x_, float y_) { x = x_; y = y_; }
};

union vec3
{
	struct
	{
		float x, y, z;
	};
	struct
	{
		float r, g, b;
	};
	struct
	{
		vec2 xy;
		float z;
	};
	struct
	{
		float data[3];
	};

	vec3() {}
	vec3(float v) : vec3(v, v, v) {}
	vec3(float x_, float y_, float z_) { x = x_; y = y_; z = z_; }
	vec3(vec2 v2, float z_) { x = v2.x; y = v2.y; z = z_; }
};

union vec4
{
	struct
	{
		float x, y, z, w;
	};
	struct
	{
		float r, g, b, a;
	};
	struct
	{
		vec3 xyz;
		float w;
	};
	struct
	{
		vec2 xy;
		vec2 zw;
	};
	floatx4 f4;
	float data[4];

	vec4() {}
	vec4(float v) : vec4(v, v, v, v) {}
	vec4(float x_, float y_, float z_, float w_) { x = x_; y = y_; z = z_; w = w_; }
	vec4(floatx4 f4_) { f4 = f4_; }
	vec4(vec3 v3, float w_) { x = v3.x; y = v3.y; z = v3.z; w = w_; }
};

union quat
{
	struct
	{
		float x, y, z, w;
	};
	struct
	{
		vec3 v;
		float cosHalfAngle;
	};
	vec4 v4;
	floatx4 f4;

	quat() {}
	quat(float x_, float y_, float z_, float w_) { x = x_; y = y_; z = z_; w = w_; }
	quat(floatx4 f4_) { f4 = f4_; }
	quat(vec3 axis, float angle);

	static const quat identity;
};

#if ROW_MAJOR
union mat2
{
	struct
	{
		float
			m00, m01,
			m10, m11;
	};
	struct
	{
		vec2 row0;
		vec2 row1;
	};
	vec2 rows[2];
	float m[4];

	mat2() {}
	mat2(float m00_, float m01_,
		float m10_, float m11_);

	static const mat2 identity;
	static const mat2 zero;
};

union mat3
{
	struct
	{
		float
			m00, m01, m02,
			m10, m11, m12,
			m20, m21, m22;
	};
	struct
	{
		vec3 row0;
		vec3 row1;
		vec3 row2;
	};
	vec3 rows[3];
	float m[9];

	mat3() {}
	mat3(float m00_, float m01_, float m02_,
		float m10_, float m11_, float m12_,
		float m20_, float m21_, float m22_);

	static const mat3 identity;
	static const mat3 zero;
};

union mat4
{
	struct
	{
		float
			m00, m01, m02, m03,
			m10, m11, m12, m13,
			m20, m21, m22, m23,
			m30, m31, m32, m33;
	};
	struct
	{
		vec4 row0;
		vec4 row1;
		vec4 row2;
		vec4 row3;
	};
	struct
	{
		floatx4 f40;
		floatx4 f41;
		floatx4 f42;
		floatx4 f43;
	};
	vec4 rows[4];
	float m[16];

	mat4() {}
	mat4(float m00_, float m01_, float m02_, float m03_,
		float m10_, float m11_, float m12_, float m13_,
		float m20_, float m21_, float m22_, float m23_,
		float m30_, float m31_, float m32_, float m33_);

	static const mat4 identity;
	static const mat4 zero;
};

static vec2 row(mat2 a, uint32 r) { return a.rows[r]; }
static vec3 row(mat3 a, uint32 r) { return a.rows[r]; }
static vec4 row(mat4 a, uint32 r) { return a.rows[r]; }

static vec2 col(mat2 a, uint32 c) { return { a.m[c], a.m[c + 2] }; }
static vec3 col(mat3 a, uint32 c) { return { a.m[c], a.m[c + 3], a.m[c + 6] }; }
static vec4 col(mat4 a, uint32 c) { return { a.m[c], a.m[c + 4], a.m[c + 8], a.m[c + 12] }; }

#else

union mat2
{
	struct
	{
		float
			m00, m10,
			m01, m11;
	};
	struct
	{
		vec2 col0;
		vec2 col1;
	};
	vec2 cols[2];
	float m[4];

	mat2() {}
	mat2(float m00_, float m01_,
		float m10_, float m11_);

	static const mat2 identity;
	static const mat2 zero;
};

union mat3
{
	struct
	{
		float
			m00, m10, m20,
			m01, m11, m21,
			m02, m12, m22;
	};
	struct
	{
		vec3 col0;
		vec3 col1;
		vec3 col2;
	};
	vec3 cols[3];
	float m[9];

	mat3() {}
	mat3(float m00_, float m01_, float m02_,
		float m10_, float m11_, float m12_,
		float m20_, float m21_, float m22_);

	static const mat3 identity;
	static const mat3 zero;
};

union mat4
{
	struct
	{
		float
			m00, m10, m20, m30,
			m01, m11, m21, m31,
			m02, m12, m22, m32,
			m03, m13, m23, m33;
	};
	struct
	{
		vec4 col0;
		vec4 col1;
		vec4 col2;
		vec4 col3;
	};
	struct
	{
		floatx4 f40;
		floatx4 f41;
		floatx4 f42;
		floatx4 f43;
	};
	vec4 cols[4];
	float m[16];

	mat4() {}
	mat4(float m00_, float m01_, float m02_, float m_03,
		float m10_, float m11_, float m12_, float m_13,
		float m20_, float m21_, float m22_, float m_23,
		float m30_, float m31_, float m32_, float m_33);

	static const mat4 identity;
	static const mat4 zero;
};

static_assert(sizeof(mat4) == 16 * sizeof(float), "");

static vec2 row(const mat2& a, uint32 r) { return { a.m[r], a.m[r + 2] }; }
static vec3 row(const mat3& a, uint32 r) { return { a.m[r], a.m[r + 3], a.m[r + 6] }; }
static vec4 row(const mat4& a, uint32 r) { return { a.m[r], a.m[r + 4], a.m[r + 8], a.m[r + 12] }; }

static vec2 col(const mat2& a, uint32 c) { return a.cols[c]; }
static vec3 col(const mat3& a, uint32 c) { return a.cols[c]; }
static vec4 col(const mat4& a, uint32 c) { return a.cols[c]; }

#endif

struct trs
{
	quat rotation;
	vec3 position;
	vec3 scale;

	trs() {}
	trs(vec3 position_, quat rotation_, vec3 scale_ = { 1.f, 1.f, 1.f }) { position = position_; rotation = rotation_; scale = scale_; }
	trs(const mat4& m);

	static const trs identity;
};







// Vec2 operators.
static vec2 operator+(vec2 a, vec2 b) { vec2 result = { a.x + b.x, a.y + b.y }; return result; }
static vec2& operator+=(vec2& a, vec2 b) { a = a + b; return a; }
static vec2 operator-(vec2 a, vec2 b) { vec2 result = { a.x - b.x, a.y - b.y }; return result; }
static vec2& operator-=(vec2& a, vec2 b) { a = a - b; return a; }
static vec2 operator*(vec2 a, vec2 b) { vec2 result = { a.x * b.x, a.y * b.y }; return result; }
static vec2& operator*=(vec2& a, vec2 b) { a = a * b; return a; }
static vec2 operator/(vec2 a, vec2 b) { vec2 result = { a.x / b.x, a.y / b.y }; return result; }
static vec2& operator/=(vec2& a, vec2 b) { a = a / b; return a; }

static vec2 operator*(vec2 a, float b) { vec2 result = { a.x * b, a.y * b }; return result; }
static vec2 operator*(float a, vec2 b) { return b * a; }
static vec2& operator*=(vec2& a, float b) { a = a * b; return a; }
static vec2 operator/(vec2 a, float b) { vec2 result = { a.x / b, a.y / b }; return result; }
static vec2& operator/=(vec2& a, float b) { a = a / b; return a; }

static vec2 operator-(vec2 a) { return vec2(-a.x, -a.y); }

static bool operator==(vec2 a, vec2 b) { return a.x == b.x && a.y == b.y; }


// Vec3 operators.
static vec3 operator+(vec3 a, vec3 b) { vec3 result = { a.x + b.x, a.y + b.y, a.z + b.z }; return result; }
static vec3& operator+=(vec3& a, vec3 b) { a = a + b; return a; }
static vec3 operator-(vec3 a, vec3 b) { vec3 result = { a.x - b.x, a.y - b.y, a.z - b.z }; return result; }
static vec3& operator-=(vec3& a, vec3 b) { a = a - b; return a; }
static vec3 operator*(vec3 a, vec3 b) { vec3 result = { a.x * b.x, a.y * b.y, a.z * b.z }; return result; }
static vec3& operator*=(vec3& a, vec3 b) { a = a * b; return a; }
static vec3 operator/(vec3 a, vec3 b) { vec3 result = { a.x / b.x, a.y / b.y, a.z / b.z }; return result; }
static vec3& operator/=(vec3& a, vec3 b) { a = a / b; return a; }

static vec3 operator*(vec3 a, float b) { vec3 result = { a.x * b, a.y * b, a.z * b }; return result; }
static vec3 operator*(float a, vec3 b) { return b * a; }
static vec3& operator*=(vec3& a, float b) { a = a * b; return a; }
static vec3 operator/(vec3 a, float b) { vec3 result = { a.x / b, a.y / b, a.z / b }; return result; }
static vec3& operator/=(vec3& a, float b) { a = a / b; return a; }

static vec3 operator-(vec3 a) { return vec3(-a.x, -a.y, -a.z); }

static bool operator==(vec3 a, vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }


// Vec4 operators.
static vec4 operator+(vec4 a, vec4 b) { vec4 result = { a.f4 + b.f4 }; return result; }
static vec4& operator+=(vec4& a, vec4 b) { a = a + b; return a; }
static vec4 operator-(vec4 a, vec4 b) { vec4 result = { a.f4 - b.f4 }; return result; }
static vec4& operator-=(vec4& a, vec4 b) { a = a - b; return a; }
static vec4 operator*(vec4 a, vec4 b) { vec4 result = { a.f4 * b.f4 }; return result; }
static vec4& operator*=(vec4& a, vec4 b) { a = a * b; return a; }
static vec4 operator/(vec4 a, vec4 b) { vec4 result = { a.f4 / b.f4 }; return result; }
static vec4 operator/=(vec4& a, vec4 b) { a = a / b; return a; }

static vec4 operator*(vec4 a, float b) { vec4 result = { a.f4 * floatx4(b) }; return result; }
static vec4 operator*(float a, vec4 b) { return b * a; }
static vec4& operator*=(vec4& a, float b) { a = a * b; return a; }
static vec4 operator/(vec4 a, float b) { vec4 result = { a.f4 / floatx4(b) }; return result; }
static vec4& operator/=(vec4& a, float b) { a = a / b; return a; }

static vec4 operator-(vec4 a) { return vec4(-a.f4); }

static bool operator==(vec4 a, vec4 b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }


static float dot(vec2 a, vec2 b) { float result = a.x * b.x + a.y * b.y; return result; }
static float dot(vec3 a, vec3 b) { float result = a.x * b.x + a.y * b.y + a.z * b.z; return result; }
static float dot(vec4 a, vec4 b) { floatx4 m = a.f4 * b.f4; return addElements(m); }


static bool operator==(mat4 a, mat4 b) 
{ 
	for (uint32 i = 0; i < 16; ++i)
	{
		if (a.m[i] != b.m[i])
		{
			return false;
		}
	}
	return true;
}

static vec3 cross(vec3 a, vec3 b) { vec3 result = { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; return result; }

static float squaredLength(vec2 a) { return dot(a, a); }
static float squaredLength(vec3 a) { return dot(a, a); }
static float squaredLength(vec4 a) { return dot(a, a); }

static float length(vec2 a) { return sqrt(squaredLength(a)); }
static float length(vec3 a) { return sqrt(squaredLength(a)); }
static float length(vec4 a) { return sqrt(squaredLength(a)); }

static vec2 noz(vec2 a) { float sl = squaredLength(a); return (sl == 0.f) ? vec2(0.f, 0.f) : (a * (1.f / sqrt(sl))); }
static vec3 noz(vec3 a) { float sl = squaredLength(a); return (sl == 0.f) ? vec3(0.f, 0.f, 0.f) : (a * (1.f / sqrt(sl))); }
static vec4 noz(vec4 a) { float sl = squaredLength(a); return (sl == 0.f) ? vec4(0.f, 0.f, 0.f, 0.f) : (a * (1.f / sqrt(sl))); }

static vec2 normalize(vec2 a) { float l = length(a); return a * (1.f / l); }
static vec3 normalize(vec3 a) { float l = length(a); return a * (1.f / l); }
static vec4 normalize(vec4 a) { float l = length(a); return a * (1.f / l); }

static vec2 abs(vec2 a) { return vec2(abs(a.x), abs(a.y)); }
static vec3 abs(vec3 a) { return vec3(abs(a.x), abs(a.y), abs(a.z)); }
static vec4 abs(vec4 a) { return vec4(abs(a.f4)); }

static vec4 round(vec4 a) { return vec4(round(a.f4)); }

static quat normalize(quat a) { return { normalize(a.v4).f4 }; }
static quat conjugate(quat a) { return { -a.x, -a.y, -a.z, a.w }; }

static quat operator+(quat a, quat b) { quat result = { a.f4 + b.f4 }; return result; }

static quat operator*(quat a, quat b)
{
	quat result;
	result.w = a.w * b.w - dot(a.v, b.v);
	result.v = a.v * b.w + b.v * a.w + cross(a.v, b.v);
	return result;
}

static quat operator*(quat q, float s)
{
	quat result;
	result.v4 = q.v4 * s;
	return result;
}

static vec3 operator*(quat q, vec3 v)
{
	quat p(v.x, v.y, v.z, 0.f);
	return (q * p * conjugate(q)).v;
}


static vec2 operator*(mat2 a, vec2 b) { vec2 result = { dot(row(a, 0), b), dot(row(a, 1), b) }; return result; }
static vec3 operator*(mat3 a, vec3 b) { vec3 result = { dot(row(a, 0), b), dot(row(a, 1), b), dot(row(a, 2), b) }; return result; }

#if ROW_MAJOR
static vec4 operator*(mat4 a, vec4 b) { vec4 result = { dot(row(a, 0), b), dot(row(a, 1), b), dot(row(a, 2), b), dot(row(a, 3), b) }; return result; }
#else
static vec4 operator*(mat4 a, vec4 b) { vec4 result = col(a, 0) * b.x + col(a, 1) * b.y + col(a, 2) * b.z + col(a, 3) * b.w; return result; }
#endif



static vec2 lerp(vec2 l, vec2 u, float t) { return l + t * (u - l); }
static vec3 lerp(vec3 l, vec3 u, float t) { return l + t * (u - l); }
static vec4 lerp(vec4 l, vec4 u, float t) { return l + t * (u - l); }
static quat lerp(quat l, quat u, float t) { quat result; result.v4 = lerp(l.v4, u.v4, t); return normalize(result); }


mat2 operator*(const mat2& a, const mat2& b);
mat3 operator*(const mat3& a, const mat3& b);
mat3 operator+(const mat3& a, const mat3& b);
mat3& operator+=(mat3& a, const mat3& b);
mat3 operator-(const mat3& a, const mat3& b);
mat4 operator*(const mat4& a, const mat4& b);
mat2 operator*(const mat2& a, float b);
mat3 operator*(const mat3& a, float b);
mat4 operator*(const mat4& a, float b);
mat2 operator*(float a, const mat2& b);
mat3 operator*(float a, const mat3& b);
mat4 operator*(float a, const mat4& b);
mat2& operator*=(mat2& a, float b);
mat3& operator*=(mat3& a, float b);
mat4& operator*=(mat4& a, float b);
trs operator*(trs a, trs b);

mat2 transpose(const mat2& a);
mat3 transpose(const mat3& a);
mat4 transpose(const mat4& a);

mat3 invert(const mat3& m);
mat4 invert(const mat4& m);

vec3 transformPosition(const mat4& m, vec3 pos);
vec3 transformDirection(const mat4& m, vec3 dir);
vec3 transformPosition(const trs& m, vec3 pos);
vec3 transformDirection(const trs& m, vec3 dir);
vec3 inverseTransformPosition(const trs& m, vec3 pos);
vec3 inverseTransformDirection(const trs& m, vec3 dir);

quat rotateFromTo(quat from, quat to);
quat rotateFromTo(vec3 from, vec3 to);
quat lookAtQuaternion(vec3 forward, vec3 up);
void getAxisRotation(quat q, vec3& axis, float& angle);
void decomposeQuaternionIntoTwistAndSwing(quat q, vec3 normalizedTwistAxis, quat& twist, quat& swing);

quat slerp(quat from, quat to, float t);
quat nlerp(quat from, quat to, float t);

mat3 quaternionToMat3(quat q);
quat mat3ToQuaternion(const mat3& m);

vec3 quatToEuler(quat q);
quat eulerToQuat(vec3 euler);

mat4 trsToMat4(const trs& transform);

mat4 createPerspectiveProjectionMatrix(float fov, float aspect, float nearPlane, float farPlane);
mat4 createPerspectiveProjectionMatrix(float width, float height, float fx, float fy, float cx, float cy, float nearPlane, float farPlane);
mat4 createPerspectiveProjectionMatrix(float r, float l, float t, float b, float nearPlane, float farPlane);
mat4 createOrthographicProjectionMatrix(float r, float l, float t, float b, float nearPlane, float farPlane);
mat4 invertPerspectiveProjectionMatrix(const mat4& m);
mat4 invertOrthographicProjectionMatrix(const mat4& m);
mat4 createModelMatrix(vec3 position, quat rotation, vec3 scale = vec3(1.f, 1.f, 1.f));
mat4 createViewMatrix(vec3 eye, float pitch, float yaw);
mat4 createSkyViewMatrix(const mat4& v);
mat4 lookAt(vec3 eye, vec3 target, vec3 up);
mat4 createViewMatrix(vec3 position, quat rotation);
mat4 invertedAffine(const mat4& m);

bool pointInTriangle(vec3 point, vec3 triA, vec3 triB, vec3& triC);
bool pointInRectangle(vec2 p, vec2 topLeft, vec2 bottomRight);

vec2 directionToPanoramaUV(vec3 dir);

float angleToZeroToTwoPi(float angle);
float angleToNegPiToPi(float angle);

vec3 getBarycentricCoordinates(vec2 a, vec2 b, vec2 c, vec2 p);
vec3 getBarycentricCoordinates(vec3 a, vec3 b, vec3 c, vec3 p);
bool insideTriangle(vec3 barycentrics);

inline std::ostream& operator<<(std::ostream& s, vec2 v)
{
	s << "[" << v.x << ", " << v.y << "]";
	return s;
}

inline std::ostream& operator<<(std::ostream& s, vec3 v)
{
	s << "[" << v.x << ", " << v.y << ", " << v.z << "]";
	return s;
}

inline std::ostream& operator<<(std::ostream& s, vec4 v)
{
	s << "[" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << "]";
	return s;
}

inline std::ostream& operator<<(std::ostream& s, quat v)
{
	s << "[" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << "]";
	return s;
}

