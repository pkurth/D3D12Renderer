#include "pch.h"
#include "math.h"
#include <half/half.c>

const half half::minValue = (uint16)0b1111101111111111;
const half half::maxValue = (uint16)0b0111101111111111;

const mat2 mat2::identity =
{
	1.f, 0.f,
	0.f, 1.f,
};

const mat2 mat2::zero =
{
	0.f, 0.f,
	0.f, 0.f,
};

const mat3 mat3::identity =
{
	1.f, 0.f, 0.f,
	0.f, 1.f, 0.f,
	0.f, 0.f, 1.f,
};

const mat3 mat3::zero =
{
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
	0.f, 0.f, 0.f,
};

const mat4 mat4::identity =
{
	1.f, 0.f, 0.f, 0.f,
	0.f, 1.f, 0.f, 0.f,
	0.f, 0.f, 1.f, 0.f,
	0.f, 0.f, 0.f, 1.f,
};

const mat4 mat4::zero =
{
	0.f, 0.f, 0.f, 0.f,
	0.f, 0.f, 0.f, 0.f,
	0.f, 0.f, 0.f, 0.f,
	0.f, 0.f, 0.f, 0.f,
};

const quat quat::identity = { 0.f, 0.f, 0.f, 1.f };

const trs trs::identity = { vec3(0.f, 0.f, 0.f), quat(0.f, 0.f, 0.f, 1.f), vec3(1.f, 1.f, 1.f) };


half::half(float f)
{
	h = half_from_float(*(uint32*)&f);
}

half::half(uint16 i)
{
	h = i;
}

half::operator float()
{
	uint32 f = half_to_float(h);
	return *(float*)&f;
}

half operator+(half a, half b) { half result; result.h = half_add(a.h, b.h); return result; }
half& operator+=(half& a, half b) {	a = a + b; return a; }
half operator-(half a, half b) { half result; result.h = half_sub(a.h, b.h); return result; }
half& operator-=(half& a, half b) {	a = a - b; return a; }
half operator*(half a, half b) { half result; result.h = half_mul(a.h, b.h); return result; }
half& operator*=(half& a, half b) {	a = a * b; return a; }
half operator/(half a, half b) { half result; result.h = half_div(a.h, b.h); return result; }
half& operator/=(half& a, half b) { a = a / b; return a; }


mat2 operator*(const mat2& a, const mat2& b)
{
	vec2 r0 = row(a, 0);
	vec2 r1 = row(a, 1);

	vec2 c0 = col(b, 0);
	vec2 c1 = col(b, 1);

	mat2 result;
	result.m00 = dot(r0, c0); result.m01 = dot(r0, c1);
	result.m10 = dot(r1, c0); result.m11 = dot(r1, c1);
	return result;
}

mat3 operator*(const mat3& a, const mat3& b)
{
	vec3 r0 = row(a, 0);
	vec3 r1 = row(a, 1);
	vec3 r2 = row(a, 2);

	vec3 c0 = col(b, 0);
	vec3 c1 = col(b, 1);
	vec3 c2 = col(b, 2);

	mat3 result;
	result.m00 = dot(r0, c0); result.m01 = dot(r0, c1); result.m02 = dot(r0, c2);
	result.m10 = dot(r1, c0); result.m11 = dot(r1, c1); result.m12 = dot(r1, c2);
	result.m20 = dot(r2, c0); result.m21 = dot(r2, c1); result.m22 = dot(r2, c2);
	return result;
}

mat3 operator+(const mat3& a, const mat3& b)
{
	mat3 result;
	for (uint32 i = 0; i < 9; ++i)
	{
		result.m[i] = a.m[i] + b.m[i];
	}
	return result;
}

mat3& operator+=(mat3& a, const mat3& b)
{
	for (uint32 i = 0; i < 9; ++i)
	{
		a.m[i] += b.m[i];
	}
	return a;
}

mat3 operator-(const mat3& a, const mat3& b)
{
	mat3 result;
	for (uint32 i = 0; i < 9; ++i)
	{
		result.m[i] = a.m[i] - b.m[i];
	}
	return result;
}

mat4 operator*(const mat4& a, const mat4& b)
{
	vec4 r0 = row(a, 0);
	vec4 r1 = row(a, 1);
	vec4 r2 = row(a, 2);
	vec4 r3 = row(a, 3);

	vec4 c0 = col(b, 0);
	vec4 c1 = col(b, 1);
	vec4 c2 = col(b, 2);
	vec4 c3 = col(b, 3);

	mat4 result;
	result.m00 = dot(r0, c0); result.m01 = dot(r0, c1); result.m02 = dot(r0, c2); result.m03 = dot(r0, c3);
	result.m10 = dot(r1, c0); result.m11 = dot(r1, c1); result.m12 = dot(r1, c2); result.m13 = dot(r1, c3);
	result.m20 = dot(r2, c0); result.m21 = dot(r2, c1); result.m22 = dot(r2, c2); result.m23 = dot(r2, c3);
	result.m30 = dot(r3, c0); result.m31 = dot(r3, c1); result.m32 = dot(r3, c2); result.m33 = dot(r3, c3);
	return result;
}


mat2 operator*(const mat2& a, float b) { mat2 result; for (uint32 i = 0; i < 4; ++i) { result.m[i] = a.m[i] * b; } return result; }
mat3 operator*(const mat3& a, float b) { mat3 result; for (uint32 i = 0; i < 9; ++i) { result.m[i] = a.m[i] * b; } return result; }

#if ROW_MAJOR
mat4 operator*(const mat4& a, float b) { mat4 result; for (uint32 i = 0; i < 4; ++i) { result.rows[i] = a.rows[i] * b; } return result; }
#else
mat4 operator*(const mat4& a, float b) { mat4 result; for (uint32 i = 0; i < 4; ++i) { result.cols[i] = a.cols[i] * b; } return result; }
#endif

mat2 operator*(float a, const mat2& b) { return b * a; }
mat3 operator*(float a, const mat3& b) { return b * a; }
mat4 operator*(float a, const mat4& b) { return b * a; }

mat2& operator*=(mat2& a, float b) { a = a * b; return a; }
mat3& operator*=(mat3& a, float b) { a = a * b; return a; }
mat4& operator*=(mat4& a, float b) { a = a * b; return a; }


mat2 transpose(const mat2& a)
{
	mat2 result;
	result.m00 = a.m00; result.m01 = a.m10;
	result.m10 = a.m01; result.m11 = a.m11;
	return result;
}

mat3 transpose(const mat3& a)
{
	mat3 result;
	result.m00 = a.m00; result.m01 = a.m10; result.m02 = a.m20;
	result.m10 = a.m01; result.m11 = a.m11; result.m12 = a.m21;
	result.m20 = a.m02; result.m21 = a.m12; result.m22 = a.m22;
	return result;
}

mat4 transpose(const mat4& a)
{
	mat4 result = a;
	_MM_TRANSPOSE4_PS(result.f40.f, result.f41.f, result.f42.f, result.f43.f);
	return result;
}

mat3 invert(const mat3& m)
{
	mat3 inv;

	inv.m00 = m.m11 * m.m22 - m.m21 * m.m12;
	inv.m01 = m.m02 * m.m21 - m.m22 * m.m01;
	inv.m02 = m.m01 * m.m12 - m.m11 * m.m02;

	inv.m10 = m.m12 * m.m20 - m.m22 * m.m10;
	inv.m11 = m.m00 * m.m22 - m.m20 * m.m02;
	inv.m12 = m.m02 * m.m10 - m.m12 * m.m00;

	inv.m20 = m.m10 * m.m21 - m.m20 * m.m11;
	inv.m21 = m.m01 * m.m20 - m.m21 * m.m00;
	inv.m22 = m.m00 * m.m11 - m.m10 * m.m01;

	float det = m.m00 * (m.m11 * m.m22 - m.m21 * m.m12)
		- m.m01 * (m.m10 * m.m22 - m.m20 * m.m12)
		+ m.m02 * (m.m10 * m.m21 - m.m20 * m.m11);

	if (det == 0.f)
	{
		return mat3();
	}

	det = 1.f / det;

	inv *= det;

	return inv;
}

mat4 invert(const mat4& m)
{
	mat4 inv;

	inv.m00 = m.m11 * m.m22 * m.m33 -
		m.m11 * m.m32 * m.m23 -
		m.m12 * m.m21 * m.m33 +
		m.m12 * m.m31 * m.m23 +
		m.m13 * m.m21 * m.m32 -
		m.m13 * m.m31 * m.m22;

	inv.m01 = -m.m01 * m.m22 * m.m33 +
		m.m01 * m.m32 * m.m23 +
		m.m02 * m.m21 * m.m33 -
		m.m02 * m.m31 * m.m23 -
		m.m03 * m.m21 * m.m32 +
		m.m03 * m.m31 * m.m22;

	inv.m02 = m.m01 * m.m12 * m.m33 -
		m.m01 * m.m32 * m.m13 -
		m.m02 * m.m11 * m.m33 +
		m.m02 * m.m31 * m.m13 +
		m.m03 * m.m11 * m.m32 -
		m.m03 * m.m31 * m.m12;

	inv.m03 = -m.m01 * m.m12 * m.m23 +
		m.m01 * m.m22 * m.m13 +
		m.m02 * m.m11 * m.m23 -
		m.m02 * m.m21 * m.m13 -
		m.m03 * m.m11 * m.m22 +
		m.m03 * m.m21 * m.m12;

	inv.m10 = -m.m10 * m.m22 * m.m33 +
		m.m10 * m.m32 * m.m23 +
		m.m12 * m.m20 * m.m33 -
		m.m12 * m.m30 * m.m23 -
		m.m13 * m.m20 * m.m32 +
		m.m13 * m.m30 * m.m22;

	inv.m11 = m.m00 * m.m22 * m.m33 -
		m.m00 * m.m32 * m.m23 -
		m.m02 * m.m20 * m.m33 +
		m.m02 * m.m30 * m.m23 +
		m.m03 * m.m20 * m.m32 -
		m.m03 * m.m30 * m.m22;

	inv.m12 = -m.m00 * m.m12 * m.m33 +
		m.m00 * m.m32 * m.m13 +
		m.m02 * m.m10 * m.m33 -
		m.m02 * m.m30 * m.m13 -
		m.m03 * m.m10 * m.m32 +
		m.m03 * m.m30 * m.m12;

	inv.m13 = m.m00 * m.m12 * m.m23 -
		m.m00 * m.m22 * m.m13 -
		m.m02 * m.m10 * m.m23 +
		m.m02 * m.m20 * m.m13 +
		m.m03 * m.m10 * m.m22 -
		m.m03 * m.m20 * m.m12;

	inv.m20 = m.m10 * m.m21 * m.m33 -
		m.m10 * m.m31 * m.m23 -
		m.m11 * m.m20 * m.m33 +
		m.m11 * m.m30 * m.m23 +
		m.m13 * m.m20 * m.m31 -
		m.m13 * m.m30 * m.m21;

	inv.m21 = -m.m00 * m.m21 * m.m33 +
		m.m00 * m.m31 * m.m23 +
		m.m01 * m.m20 * m.m33 -
		m.m01 * m.m30 * m.m23 -
		m.m03 * m.m20 * m.m31 +
		m.m03 * m.m30 * m.m21;

	inv.m22 = m.m00 * m.m11 * m.m33 -
		m.m00 * m.m31 * m.m13 -
		m.m01 * m.m10 * m.m33 +
		m.m01 * m.m30 * m.m13 +
		m.m03 * m.m10 * m.m31 -
		m.m03 * m.m30 * m.m11;

	inv.m23 = -m.m00 * m.m11 * m.m23 +
		m.m00 * m.m21 * m.m13 +
		m.m01 * m.m10 * m.m23 -
		m.m01 * m.m20 * m.m13 -
		m.m03 * m.m10 * m.m21 +
		m.m03 * m.m20 * m.m11;

	inv.m30 = -m.m10 * m.m21 * m.m32 +
		m.m10 * m.m31 * m.m22 +
		m.m11 * m.m20 * m.m32 -
		m.m11 * m.m30 * m.m22 -
		m.m12 * m.m20 * m.m31 +
		m.m12 * m.m30 * m.m21;

	inv.m31 = m.m00 * m.m21 * m.m32 -
		m.m00 * m.m31 * m.m22 -
		m.m01 * m.m20 * m.m32 +
		m.m01 * m.m30 * m.m22 +
		m.m02 * m.m20 * m.m31 -
		m.m02 * m.m30 * m.m21;

	inv.m32 = -m.m00 * m.m11 * m.m32 +
		m.m00 * m.m31 * m.m12 +
		m.m01 * m.m10 * m.m32 -
		m.m01 * m.m30 * m.m12 -
		m.m02 * m.m10 * m.m31 +
		m.m02 * m.m30 * m.m11;

	inv.m33 = m.m00 * m.m11 * m.m22 -
		m.m00 * m.m21 * m.m12 -
		m.m01 * m.m10 * m.m22 +
		m.m01 * m.m20 * m.m12 +
		m.m02 * m.m10 * m.m21 -
		m.m02 * m.m20 * m.m11;

	float det = m.m00 * inv.m00 + m.m10 * inv.m01 + m.m20 * inv.m02 + m.m30 * inv.m03;

	if (det == 0.f)
	{
		return mat4();
	}

	det = 1.f / det;

	inv *= det;

	return inv;
}

float determinant(const mat3& m)
{
	return m.m00 * (m.m11 * m.m22 - m.m21 * m.m12)
		- m.m01 * (m.m10 * m.m22 - m.m20 * m.m12)
		+ m.m02 * (m.m10 * m.m21 - m.m20 * m.m11);
}

float determinant(const mat4& m)
{
	return
		m.m03 * m.m12 * m.m21 * m.m30 - m.m02 * m.m13 * m.m21 * m.m30 -
		m.m03 * m.m11 * m.m22 * m.m30 + m.m01 * m.m13 * m.m22 * m.m30 +
		m.m02 * m.m11 * m.m23 * m.m30 - m.m01 * m.m12 * m.m23 * m.m30 -
		m.m03 * m.m12 * m.m20 * m.m31 + m.m02 * m.m13 * m.m20 * m.m31 +
		m.m03 * m.m10 * m.m22 * m.m31 - m.m00 * m.m13 * m.m22 * m.m31 -
		m.m02 * m.m10 * m.m23 * m.m31 + m.m00 * m.m12 * m.m23 * m.m31 +
		m.m03 * m.m11 * m.m20 * m.m32 - m.m01 * m.m13 * m.m20 * m.m32 -
		m.m03 * m.m10 * m.m21 * m.m32 + m.m00 * m.m13 * m.m21 * m.m32 +
		m.m01 * m.m10 * m.m23 * m.m32 - m.m00 * m.m11 * m.m23 * m.m32 -
		m.m02 * m.m11 * m.m20 * m.m33 + m.m01 * m.m12 * m.m20 * m.m33 +
		m.m02 * m.m10 * m.m21 * m.m33 - m.m00 * m.m12 * m.m21 * m.m33 -
		m.m01 * m.m10 * m.m22 * m.m33 + m.m00 * m.m11 * m.m22 * m.m33;
}

float trace(const mat3& m)
{
	return m.m00 + m.m11 + m.m22;
}

float trace(const mat4& m)
{
	return m.m00 + m.m11 + m.m22 + m.m33;
}

trs operator*(trs a, trs b)
{
	trs result;
	result.rotation = a.rotation * b.rotation;
	result.position = a.rotation * (a.scale * b.position) + a.position;
	result.scale = a.scale * b.scale;
	return result;
}

vec3 transformPosition(const mat4& m, vec3 pos)
{
	return (m * vec4(pos, 1.f)).xyz;
}

vec3 transformDirection(const mat4& m, vec3 dir)
{
	return (m * vec4(dir, 0.f)).xyz;
}

vec3 transformPosition(const trs& m, vec3 pos)
{
	return m.rotation * (m.scale * pos) + m.position;
}

vec3 transformDirection(const trs& m, vec3 dir)
{
	return m.rotation * dir;
}

vec3 inverseTransformPosition(const trs& m, vec3 pos)
{
	return (conjugate(m.rotation) * (pos - m.position)) / m.scale;
}

vec3 inverseTransformDirection(const trs& m, vec3 dir)
{
	return conjugate(m.rotation) * dir;
}

quat rotateFromTo(quat from, quat to)
{
	quat invFrom = conjugate(from);
	quat result = to * invFrom;
	return normalize(result);
}

quat rotateFromTo(vec3 _from, vec3 _to)
{
	vec3 from = normalize(_from);
	vec3 to = normalize(_to);

	float d = dot(from, to);
	if (d >= 1.f)
	{
		return quat(0.f, 0.f, 0.f, 1.f);
	}

	quat q;
	if (d < (1e-6f - 1.f))
	{
		// Rotate 180° around some axis.
		vec3 axis = cross(vec3(1.f, 0.f, 0.f), from);
		if (squaredLength(axis) == 0.f) // Pick another if colinear.
		{
			axis = cross(vec3(0.f, 1.f, 0.f), from);
		}
		axis = normalize(axis);
		q = normalize(quat(axis, M_PI));
	}
	else
	{
		float s = sqrt((1.f + d) * 2.f);
		float invs = 1.f / s;

		vec3 c = cross(from, to);

		q.x = c.x * invs;
		q.y = c.y * invs;
		q.z = c.z * invs;
		q.w = s * 0.5f;
		q = normalize(q);
	}
	return q;
}

void getAxisRotation(quat q, vec3& axis, float& angle)
{
	float sqLength = squaredLength(q.v);
	if (sqLength > 0.f)
	{
		angle = 2.f * acos(q.w);
		float invLength = 1.f / sqrt(sqLength);
		axis = q.v * invLength;
	}
	else
	{
		// Angle is 0 (mod 2*pi), so any axis will do.
		angle = 0.f;
		axis = vec3(1.f, 0.f, 0.f);
	}
}

void decomposeQuaternionIntoTwistAndSwing(quat q, vec3 normalizedTwistAxis, quat& twist, quat& swing)
{
	vec3 axis(q.x, q.y, q.z);
	vec3 proj = dot(axis, normalizedTwistAxis) * normalizedTwistAxis; // This assumes that twistAxis is normalized.
	twist = normalize(quat(proj.x, proj.y, proj.z, q.w));
	swing = q * conjugate(twist);
}

quat slerp(quat from, quat to, float t)
{
	float d = dot(from.v4, to.v4);
	float absDot = d < 0.f ? -d : d;
	float scale0 = 1.f - t;
	float scale1 = t;

	if ((1.f - absDot) > 0.1f)
	{

		float angle = acosf(absDot);
		float invSinTheta = 1.f / sinf(angle);
		scale0 = (sinf((1.f - t) * angle) * invSinTheta);
		scale1 = (sinf((t * angle)) * invSinTheta);
	}

	if (d < 0.f)
	{
		scale1 = -scale1;
	}
	float newX = (scale0 * from.x) + (scale1 * to.x);
	float newY = (scale0 * from.y) + (scale1 * to.y);
	float newZ = (scale0 * from.z) + (scale1 * to.z);
	float newW = (scale0 * from.w) + (scale1 * to.w);
	return normalize(quat(newX, newY, newZ, newW));
}

quat nlerp(quat* qs, float* weights, uint32 count)
{
	vec4 v0 = qs[0].v4;
	vec4 result = v0 * weights[0];

	for (uint32 i = 1; i < count; ++i)
	{
		vec4 v1 = qs[i].v4;
		if (dot(v0, v1) < 0.f) { v1 = -v1; }
		result += v1 * weights[i];
	}

	return { result.f4 };
}

mat3 quaternionToMat3(quat q)
{
	if (q.w == 1.f)
	{
		return mat3::identity;
	}

	float qxx = q.x * q.x;
	float qyy = q.y * q.y;
	float qzz = q.z * q.z;
	float qxz = q.x * q.z;
	float qxy = q.x * q.y;
	float qyz = q.y * q.z;
	float qwx = q.w * q.x;
	float qwy = q.w * q.y;
	float qwz = q.w * q.z;

	mat3 result;

	result.m00 = 1.f - 2.f * (qyy + qzz);
	result.m10 = 2.f * (qxy + qwz);
	result.m20 = 2.f * (qxz - qwy);

	result.m01 = 2.f * (qxy - qwz);
	result.m11 = 1.f - 2.f * (qxx + qzz);
	result.m21 = 2.f * (qyz + qwx);

	result.m02 = 2.f * (qxz + qwy);
	result.m12 = 2.f * (qyz - qwx);
	result.m22 = 1.f - 2.f * (qxx + qyy);

	return result;
}

quat mat3ToQuaternion(const mat3& m)
{
#if 1
	float tr = m.m00 + m.m11 + m.m22;

	quat result;
	if (tr > 0.f)
	{
		float s = sqrtf(tr + 1.f) * 2.f; // S=4*qw 
		result.w = 0.25f * s;
		result.x = (m.m21 - m.m12) / s;
		result.y = (m.m02 - m.m20) / s;
		result.z = (m.m10 - m.m01) / s;
	}
	else if ((m.m00 > m.m11) && (m.m00 > m.m22))
	{
		float s = sqrtf(1.f + m.m00 - m.m11 - m.m22) * 2.f; // S=4*qx 
		result.w = (m.m21 - m.m12) / s;
		result.x = 0.25f * s;
		result.y = (m.m01 + m.m10) / s;
		result.z = (m.m02 + m.m20) / s;
	}
	else if (m.m11 > m.m22)
	{
		float s = sqrtf(1.f + m.m11 - m.m00 - m.m22) * 2.f; // S=4*qy
		result.w = (m.m02 - m.m20) / s;
		result.x = (m.m01 + m.m10) / s;
		result.y = 0.25f * s;
		result.z = (m.m12 + m.m21) / s;
	}
	else
	{
		float s = sqrtf(1.f + m.m22 - m.m00 - m.m11) * 2.f; // S=4*qz
		result.w = (m.m10 - m.m01) / s;
		result.x = (m.m02 + m.m20) / s;
		result.y = (m.m12 + m.m21) / s;
		result.z = 0.25f * s;
	}
#else
	quat result;
	result.w = sqrt(1.f + m.m00 + m.m11 + m.m22) * 0.5f;
	float w4 = 1.f / (4.f * result.w);
	result.x = (m.m21 - m.m12) * w4;
	result.y = (m.m02 - m.m20) * w4;
	result.z = (m.m10 - m.m02) * w4;
#endif
	return normalize(result);
}

vec3 quatToEuler(quat q)
{
	float roll, pitch, yaw;

	// Roll (x-axis rotation).
	float sinr_cosp = 2.f * (q.w * q.x + q.y * q.z);
	float cosr_cosp = 1.f - 2.f * (q.x * q.x + q.y * q.y);
	yaw = atan2(sinr_cosp, cosr_cosp);

	// Pitch (y-axis rotation).
	float sinp = 2.f * (q.w * q.y - q.z * q.x);
	if (abs(sinp) >= 1.f)
	{
		roll = copysign(M_PI / 2, sinp); // Use 90 degrees if out of range.
	}
	else
	{
		roll = asin(sinp);
	}

	// Yaw (z-axis rotation).
	float siny_cosp = 2 * (q.w * q.z + q.x * q.y);
	float cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
	pitch = atan2(siny_cosp, cosy_cosp);

	return vec3(pitch, yaw, roll);
}

quat eulerToQuat(vec3 euler)
{
	float pitch = euler.x;
	float yaw = euler.y;
	float roll = euler.z;

	// Abbreviations for the various angular functions
	float cy = cos(pitch * 0.5f);
	float sy = sin(pitch * 0.5f);
	float cp = cos(roll * 0.5f);
	float sp = sin(roll * 0.5f);
	float cr = cos(yaw * 0.5f);
	float sr = sin(yaw * 0.5f);

	quat q;
	q.w = cr * cp * cy + sr * sp * sy;
	q.x = sr * cp * cy - cr * sp * sy;
	q.y = cr * sp * cy + sr * cp * sy;
	q.z = cr * cp * sy - sr * sp * cy;

	return q;
}

mat3 outerProduct(vec3 a, vec3 b)
{
	vec3 col0 = a * b.x;
	vec3 col1 = a * b.y;
	vec3 col2 = a * b.z;

	mat3 result;
	result.m00 = col0.x;
	result.m10 = col0.y;
	result.m20 = col0.z;
	result.m01 = col1.x;
	result.m11 = col1.y;
	result.m21 = col1.z;
	result.m02 = col2.x;
	result.m12 = col2.y;
	result.m22 = col2.z;
	return result;
}

mat3 getSkewMatrix(vec3 r)
{
	mat3 result;
	result.m00 = 0.f;
	result.m01 = -r.z;
	result.m02 = r.y;
	result.m10 = r.z;
	result.m11 = 0.f;
	result.m12 = -r.x;
	result.m20 = -r.y;
	result.m21 = r.x;
	result.m22 = 0.f;
	return result;
}

mat4 createPerspectiveProjectionMatrix(float fov, float aspect, float nearPlane, float farPlane)
{
	mat4 result = mat4::identity;
	result.m11 = 1.f / tan(0.5f * fov);
	result.m00 = result.m11 / aspect;
	if (farPlane > 0.f)
	{
#if DIRECTX_COORDINATE_SYSTEM
		result.m22 = -farPlane / (farPlane - nearPlane);
		result.m23 = result.m22 * nearPlane;
#else
		result.m22 = -(farPlane + nearPlane) / (farPlane - nearPlane);
		result.m23 = -2.f * farPlane * nearPlane / (farPlane - nearPlane);
#endif
	}
	else
	{
		result.m22 = -1.f;
#if DIRECTX_COORDINATE_SYSTEM
		result.m23 = -nearPlane;
#else
		result.m23 = -2.f * nearPlane;
#endif
	}
	result.m32 = -1.f;
	result.m33 = 0.f;

	return result;
}

mat4 createPerspectiveProjectionMatrix(float width, float height, float fx, float fy, float cx, float cy, float nearPlane, float farPlane)
{
	mat4 result;

	result.m00 = 2.f * fx / width;
	result.m10 = 0.f;
	result.m20 = 0.f;
	result.m30 = 0.f;
	
	result.m01 = 0.f;
	result.m11 = 2.f * fy / height;
	result.m21 = 0.f;
	result.m31 = 0.f;
	
	result.m02 = 1.f - 2.f * cx / width;
	result.m12 = 2.f * cy / height - 1.f;
	result.m32 = -1.f;
	
	result.m03 = 0.f;
	result.m13 = 0.f;
	result.m33 = 0.f;

	if (farPlane > 0.f)
	{
#if DIRECTX_COORDINATE_SYSTEM
		result.m22 = -farPlane / (farPlane - nearPlane);
		result.m23 = result.m22 * nearPlane;
#else
		result.m22 = -(farPlane + nearPlane) / (farPlane - nearPlane);
		result.m23 = -2.f * farPlane * nearPlane / (farPlane - nearPlane);
#endif
	}
	else
	{
		result.m22 = -1.f;
#if DIRECTX_COORDINATE_SYSTEM
		result.m23 = -nearPlane;
#else
		result.m23 = -2.f * nearPlane;
#endif
	}

	return result;
}

mat4 createPerspectiveProjectionMatrix(float r, float l, float t, float b, float nearPlane, float farPlane)
{
	mat4 result;
	result.m00 = (2.f * nearPlane) / (r - l);
	result.m01 = 0.f; 
	result.m02 = (r + l) / (r - l);
	result.m03 = 0.f;

	result.m10 = 0.f; 
	result.m11 = (2.f * nearPlane) / (t - b);
	result.m12 = (t + b) / (t - b);
	result.m13 = 0.f;

	result.m20 = 0; result.m21 = 0;
	if (farPlane > 0.f)
	{
#if DIRECTX_COORDINATE_SYSTEM
		result.m22 = -farPlane / (farPlane - nearPlane);
		result.m23 = result.m22 * nearPlane;
#else
		result.m22 = -(farPlane + nearPlane) / (farPlane - nearPlane);
		result.m23 = -2.f * farPlane * nearPlane / (farPlane - nearPlane);
#endif
	}
	else
	{
		result.m22 = -1.f;
#if DIRECTX_COORDINATE_SYSTEM
		result.m23 = -nearPlane;
#else
		result.m23 = -2.f * nearPlane;
#endif
	}

	result.m30 = 0.f; 
	result.m31 = 0.f; 
	result.m32 = -1.f; 
	result.m33 = 0.f;

	return result;
}

mat4 createOrthographicProjectionMatrix(float r, float l, float t, float b, float nearPlane, float farPlane)
{
	mat4 result = mat4::identity;
	result.m00 = 2.f / (r - l);
	result.m03 = -(r + l) / (r - l);
	result.m11 = 2.f / (t - b);
	result.m13 = -(t + b) / (t - b);

#if DIRECTX_COORDINATE_SYSTEM
	result.m22 = -1.f / (farPlane - nearPlane);
	result.m23 = result.m22 * nearPlane;
#else
	result.m22 = -2.f / (farPlane - nearPlane);
	result.m23 = -(farPlane + nearPlane) / (farPlane - nearPlane);
#endif

	return result;
}

mat4 invertPerspectiveProjectionMatrix(const mat4& m)
{
	mat4 inv = mat4::identity;
	inv.m00 = 1.f / m.m00;
	inv.m11 = 1.f / m.m11;
	inv.m22 = 0.f;
	inv.m23 = -1.f;
	inv.m32 = 1.f / m.m23;
	inv.m33 = m.m22 / m.m23;
	return inv;
}

mat4 invertOrthographicProjectionMatrix(const mat4& m)
{
	mat4 inv;
	inv.m00 = 1.f / m.m00;
	inv.m03 = -m.m03 / m.m00;
	inv.m11 = 1.f / m.m11;
	inv.m13 = -m.m13 / m.m11;
	inv.m22 = 1.f / m.m22;
	inv.m23 = -m.m23 / m.m22;
	return inv;
}

mat4 createModelMatrix(vec3 position, quat rotation, vec3 scale)
{
	mat4 result;
	result.m03 = position.x;
	result.m13 = position.y;
	result.m23 = position.z;
	mat3 rot = quaternionToMat3(rotation);
	result.m00 = rot.m00 * scale.x;
	result.m01 = rot.m01 * scale.y;
	result.m02 = rot.m02 * scale.z;
	result.m10 = rot.m10 * scale.x;
	result.m11 = rot.m11 * scale.y;
	result.m12 = rot.m12 * scale.z;
	result.m20 = rot.m20 * scale.x;
	result.m21 = rot.m21 * scale.y;
	result.m22 = rot.m22 * scale.z;
	result.m30 = result.m31 = result.m32 = 0.f;
	result.m33 = 1.f;
	return result;
}

mat4 trsToMat4(const trs& transform)
{
	return createModelMatrix(transform.position, transform.rotation, transform.scale);
}

mat4 createViewMatrix(vec3 eye, float pitch, float yaw)
{
	float cosPitch = cosf(pitch);
	float sinPitch = sinf(pitch);
	float cosYaw = cosf(yaw);
	float sinYaw = sinf(yaw);

	vec3 xAxis(cosYaw, 0, -sinYaw);
	vec3 yAxis(sinYaw * sinPitch, cosPitch, cosYaw * sinPitch);
	vec3 zAxis(sinYaw * cosPitch, -sinPitch, cosPitch * cosYaw);

	mat4 result;
	result.m00 = xAxis.x; result.m10 = yAxis.x; result.m20 = zAxis.x; result.m30 = 0.f;
	result.m01 = xAxis.y; result.m11 = yAxis.y; result.m21 = zAxis.y; result.m31 = 0.f;
	result.m02 = xAxis.z; result.m12 = yAxis.z; result.m22 = zAxis.z; result.m32 = 0.f;
	result.m03 = -dot(xAxis, eye); result.m13 = -dot(yAxis, eye); result.m23 = -dot(zAxis, eye); result.m33 = 1.f;

	return result;
}

mat4 createSkyViewMatrix(const mat4& v)
{
	mat4 result = v;
	result.m03 = 0.f; result.m13 = 0.f; result.m23 = 0.f;
	return result;
}

mat4 lookAt(vec3 eye, vec3 target, vec3 up)
{
	vec3 zAxis = normalize(eye - target);
	vec3 xAxis = normalize(cross(up, zAxis));
	vec3 yAxis = normalize(cross(zAxis, xAxis));

	mat4 result;
	result.m00 = xAxis.x; result.m10 = yAxis.x; result.m20 = zAxis.x; result.m30 = 0.f;
	result.m01 = xAxis.y; result.m11 = yAxis.y; result.m21 = zAxis.y; result.m31 = 0.f;
	result.m02 = xAxis.z; result.m12 = yAxis.z; result.m22 = zAxis.z; result.m32 = 0.f;
	result.m03 = -dot(xAxis, eye); result.m13 = -dot(yAxis, eye); result.m23 = -dot(zAxis, eye); result.m33 = 1.f;

	return result;
}

quat lookAtQuaternion(vec3 forward, vec3 up)
{
	vec3 zAxis = -normalize(forward);
	vec3 xAxis = normalize(cross(up, zAxis));
	vec3 yAxis = normalize(cross(zAxis, xAxis));

	mat3 m;
	m.m00 = xAxis.x; m.m01 = yAxis.x; m.m02 = zAxis.x;
	m.m10 = xAxis.y; m.m11 = yAxis.y; m.m12 = zAxis.y;
	m.m20 = xAxis.z; m.m21 = yAxis.z; m.m22 = zAxis.z;

	return mat3ToQuaternion(m);
}

mat4 createViewMatrix(vec3 position, quat rotation)
{
	vec3 target = position + rotation * vec3(0.f, 0.f, -1.f);
	vec3 up = rotation * vec3(0.f, 1.f, 0.f);
	return lookAt(position, target, up);
}

mat4 invertedAffine(const mat4& m)
{
	vec3 xAxis(m.m00, m.m10, m.m20);
	vec3 yAxis(m.m01, m.m11, m.m21);
	vec3 zAxis(m.m02, m.m12, m.m22);
	vec3 pos(m.m03, m.m13, m.m23);

	vec3 invXAxis = xAxis / squaredLength(xAxis);
	vec3 invYAxis = yAxis / squaredLength(yAxis);
	vec3 invZAxis = zAxis / squaredLength(zAxis);
	vec3 invPos(-dot(invXAxis, pos), -dot(invYAxis, pos), -dot(invZAxis, pos));

	mat4 result;
	result.m00 = invXAxis.x; result.m10 = invYAxis.x; result.m20 = invZAxis.x; result.m30 = 0.f;
	result.m01 = invXAxis.y; result.m11 = invYAxis.y; result.m21 = invZAxis.y; result.m31 = 0.f;
	result.m02 = invXAxis.z; result.m12 = invYAxis.z; result.m22 = invZAxis.z; result.m32 = 0.f;
	result.m03 = invPos.x; result.m13 = invPos.y; result.m23 = invPos.z; result.m33 = 1.f;
	return result;
}

mat2::mat2(float m00_, float m01_, float m10_, float m11_)
{
	m00 = m00_; m01 = m01_;
	m10 = m10_; m11 = m11_;
}

mat3::mat3(float m00_, float m01_, float m02_, float m10_, float m11_, float m12_, float m20_, float m21_, float m22_)
{
	m00 = m00_; m01 = m01_; m02 = m02_;
	m10 = m10_; m11 = m11_; m12 = m12_;
	m20 = m20_; m21 = m21_; m22 = m22_;
}

mat4::mat4(float m00_, float m01_, float m02_, float m03_, float m10_, float m11_, float m12_, float m13_, float m20_, float m21_, float m22_, float m23_, float m30_, float m31_, float m32_, float m33_)
{
	m00 = m00_; m01 = m01_; m02 = m02_; m03 = m03_;
	m10 = m10_; m11 = m11_; m12 = m12_; m13 = m13_;
	m20 = m20_; m21 = m21_; m22 = m22_; m23 = m23_;
	m30 = m30_; m31 = m31_; m32 = m32_; m33 = m33_;
}

quat::quat(vec3 axis, float angle)
{
	w = cos(angle * 0.5f);
	v = axis * sin(angle * 0.5f);
}

bool pointInTriangle(vec3 point, vec3 triA, vec3 triB, vec3& triC)
{
	vec3 e10 = triB - triA;
	vec3 e20 = triC - triA;
	float a = dot(e10, e10);
	float b = dot(e10, e20);
	float c = dot(e20, e20);
	float ac_bb = (a * c) - (b * b);
	vec3 vp = point - triA;
	float d = dot(vp, e10);
	float e = dot(vp, e20);
	float x = (d * c) - (e * b);
	float y = (e * a) - (d * b);
	float z = x + y - ac_bb;
#define uintCast(a) ((uint32&) a)
	return ((uintCast(z) & ~(uintCast(x) | uintCast(y))) & 0x80000000) != 0;
#undef uintCast
}

bool pointInRectangle(vec2 p, vec2 topLeft, vec2 bottomRight)
{
	return p.x >= topLeft.x && p.y >= topLeft.y && p.x <= bottomRight.x && p.y <= bottomRight.y;
}

vec2 directionToPanoramaUV(vec3 dir)
{
	static const float invPI = 0.31830988618379067153776752674503f;
	static const float inv2PI = 0.15915494309189533576888376337251f;
	static const vec2 invAtan = vec2(inv2PI, invPI);

	vec2 panoUV = vec2(atan2(-dir.x, -dir.z), acos(dir.y)) * invAtan;

	while (panoUV.x < 0.f) { panoUV.x += 1.f; }
	while (panoUV.y < 0.f) { panoUV.y += 1.f; }
	while (panoUV.x > 1.f) { panoUV.x -= 1.f; }
	while (panoUV.y > 1.f) { panoUV.y -= 1.f; }

	return panoUV;
}

float angleToZeroToTwoPi(float angle)
{
	while (angle < 0)
	{
		angle += M_TAU;
	}
	while (angle > M_TAU)
	{
		angle -= M_TAU;
	}
	return angle;
}

float angleToNegPiToPi(float angle)
{
	while (angle < -M_PI)
	{
		angle += M_TAU;
	}
	while (angle > M_PI)
	{
		angle -= M_TAU;
	}
	return angle;
}

vec2 solveLinearSystem(const mat2& A, vec2 b)
{
	float a11 = A.m00, a12 = A.m01, a21 = A.m10, a22 = A.m11;
	float det = a11 * a22 - a12 * a21;
	if (det != 0.f)
	{
		det = 1.f / det;
	}
	vec2 x;
	x.x = det * (a22 * b.x - a12 * b.y);
	x.y = det * (a11 * b.y - a21 * b.x);
	return x;
}

vec3 solveLinearSystem(const mat3& A, vec3 b)
{
	vec3 ex(A.m00, A.m10, A.m20);
	vec3 ey(A.m01, A.m11, A.m21);
	vec3 ez(A.m02, A.m12, A.m22);
	float det = dot(ex, cross(ey, ez));
	if (det != 0.f)
	{
		det = 1.f / det;
	}
	vec3 x;
	x.x = det * dot(b, cross(ey, ez));
	x.y = det * dot(ex, cross(b, ez));
	x.z = det * dot(ex, cross(ey, b));
	return x;
}

vec3 getBarycentricCoordinates(vec2 a, vec2 b, vec2 c, vec2 p)
{
	vec2 v0 = b - a, v1 = c - a, v2 = p - a;
	float d00 = dot(v0, v0);
	float d01 = dot(v0, v1);
	float d11 = dot(v1, v1);
	float d20 = dot(v2, v0);
	float d21 = dot(v2, v1);
	float denom = d00 * d11 - d01 * d01;

	float v = (d11 * d20 - d01 * d21) / denom;
	float w = (d00 * d21 - d01 * d20) / denom;
	float u = 1.0f - v - w;

	return vec3(u, v, w);
}

vec3 getBarycentricCoordinates(vec3 a, vec3 b, vec3 c, vec3 p)
{
	vec3 v0 = b - a, v1 = c - a, v2 = p - a;
	float d00 = dot(v0, v0);
	float d01 = dot(v0, v1);
	float d11 = dot(v1, v1);
	float d20 = dot(v2, v0);
	float d21 = dot(v2, v1);
	float denom = d00 * d11 - d01 * d01;

	denom = (abs(denom) < EPSILON) ? 1.f : denom;

	float v = (d11 * d20 - d01 * d21) / denom;
	float w = (d00 * d21 - d01 * d20) / denom;
	float u = 1.0f - v - w;

	return vec3(u, v, w);
}

bool insideTriangle(vec3 barycentrics)
{
	return barycentrics.x >= 0.f
		&& barycentrics.y >= 0.f
		&& barycentrics.z >= 0.f;
}

void getTangents(vec3 normal, vec3& outTangent, vec3& outBitangent)
{
	if (abs(normal.x) >= 0.57735f)
	{
		outTangent = vec3(normal.y, -normal.x, 0.f);
	}
	else
	{
		outTangent = vec3(0.f, normal.z, -normal.y);
	}

	outTangent = normalize(outTangent);
	outBitangent = cross(normal, outTangent);
}

trs::trs(const mat4& m)
{
	vec3 c0(m.m00, m.m10, m.m20);
	vec3 c1(m.m01, m.m11, m.m21);
	vec3 c2(m.m02, m.m12, m.m22);
	scale.x = sqrt(dot(c0, c0));
	scale.y = sqrt(dot(c1, c1));
	scale.z = sqrt(dot(c2, c2));


	vec3 invScale = 1.f / scale;

	position.x = m.m03;
	position.y = m.m13;
	position.z = m.m23;

	mat3 R;

	R.m00 = m.m00 * invScale.x;
	R.m10 = m.m10 * invScale.x;
	R.m20 = m.m20 * invScale.x;
			
	R.m01 = m.m01 * invScale.y;
	R.m11 = m.m11 * invScale.y;
	R.m21 = m.m21 * invScale.y;
			
	R.m02 = m.m02 * invScale.z;
	R.m12 = m.m12 * invScale.z;
	R.m22 = m.m22 * invScale.z;

	rotation = mat3ToQuaternion(R);
}
