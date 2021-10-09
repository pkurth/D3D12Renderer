#pragma once

#include "simd.h"
#include "soa.h"


template <typename simd_t>
union vec2x
{
	struct
	{
		simd_t x, y;
	};
	simd_t data[2];

	vec2x() {}
	vec2x(simd_t v) : vec2x(v, v) {}
	vec2x(simd_t x, simd_t y) : x(x), y(y) {}
	vec2x(soa_vec2 v, uint32 offset) : x(v.x + offset), y(v.y + offset) {}

	void store(float* xDest, float* yDest) { x.store(xDest); y.store(yDest); }

	static vec2x zero() 
	{
		if constexpr (std::is_same_v<simd_t, floatx4>) { return vec2x(floatx4::zero(), floatx4::zero()); }
		else if constexpr (std::is_same_v<simd_t, floatx8>) { return vec2x(floatx8::zero(), floatx8::zero()); }
		else if constexpr (std::is_same_v<simd_t, floatx16>) { return vec2x(floatx16::zero(), floatx16::zero()); }
		else { static_assert(false); }
	}
};

template <typename simd_t>
union vec3x
{
	struct
	{
		simd_t x, y, z;
	};
	struct
	{
		simd_t r, g, b;
	};
	struct
	{
		vec2x<simd_t> xy;
		simd_t z;
	};
	simd_t data[3];

	vec3x() {}
	vec3x(simd_t v) : vec3x(v, v, v) {}
	vec3x(simd_t x, simd_t y, simd_t z) : x(x), y(y), z(z) {}
	vec3x(vec2x<simd_t> xy, simd_t z) : x(xy.x), y(xy.y), z(z) {}
	vec3x(soa_vec3 v, uint32 offset) : x(v.x + offset), y(v.y + offset), z(v.z + offset) {}

	void store(float* xDest, float* yDest, float* zDest) { x.store(xDest); y.store(yDest); z.store(zDest); }

	static vec3x zero()
	{
		if constexpr (std::is_same_v<simd_t, floatx4>) { return vec3x(floatx4::zero(), floatx4::zero(), floatx4::zero()); }
		else if constexpr (std::is_same_v<simd_t, floatx8>) { return vec3x(floatx8::zero(), floatx8::zero(), floatx8::zero()); }
		else if constexpr (std::is_same_v<simd_t, floatx16>) { return vec3x(floatx16::zero(), floatx16::zero(), floatx16::zero()); }
		else { static_assert(false); }
	}
};

template <typename simd_t>
union vec4x
{
	struct
	{
		simd_t x, y, z, w;
	};
	struct
	{
		simd_t r, g, b, a;
	};
	struct
	{
		vec3x<simd_t> xyz;
		simd_t w;
	};
	struct
	{
		vec2x<simd_t> xy;
		vec2x<simd_t> zw;
	};
	simd_t data[4];

	vec4x() {}
	vec4x(simd_t v) : vec4x(v, v, v, v) {}
	vec4x(simd_t x, simd_t y, simd_t z, simd_t w) : x(x), y(y), z(z), w(w) {}
	vec4x(vec3x<simd_t> xyz, simd_t w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}
	vec4x(soa_vec4 v, uint32 offset) : x(v.x + offset), y(v.y + offset), z(v.z + offset), w(v.w + offset) {}

	void store(float* xDest, float* yDest, float* zDest, float* wDest) { x.store(xDest); y.store(yDest); z.store(zDest); w.store(wDest); }

	static vec4x zero()
	{
		if constexpr (std::is_same_v<simd_t, floatx4>) { return vec4x(floatx4::zero(), floatx4::zero(), floatx4::zero(), floatx4::zero()); }
		else if constexpr (std::is_same_v<simd_t, floatx8>) { return vec4x(floatx8::zero(), floatx8::zero(), floatx8::zero(), floatx8::zero()); }
		else if constexpr (std::is_same_v<simd_t, floatx16>) { return vec4x(floatx16::zero(), floatx16::zero(), floatx16::zero(), floatx16::zero()); }
		else { static_assert(false); }
	}
};

template <typename simd_t>
union quatx
{
	struct
	{
		simd_t x, y, z, w;
	};
	struct
	{
		vec3x<simd_t> v;
		simd_t cosHalfAngle;
	};
	vec4x<simd_t> v4;
	simd_t data[4];

	quatx() {}
	quatx(simd_t x, simd_t y, simd_t z, simd_t w) : x(x), y(y), z(z), w(w) {}
	quatx(vec3x<simd_t> axis, simd_t angle);
	quatx(soa_quat v, uint32 offset) : x(v.x + offset), y(v.y + offset), z(v.z + offset), w(v.w + offset) {}

	void store(float* xDest, float* yDest, float* zDest, float* wDest) { x.store(xDest); y.store(yDest); z.store(zDest); w.store(wDest); }

	static quatx identity() { return quatx<simd_t>(simd_t::zero(), simd_t::zero(), simd_t::zero(), simd_t(1.f)); }
};

template <typename simd_t>
union mat2x
{
	struct
	{
		simd_t
			m00, m10,
			m01, m11;
	};
	simd_t m[4];

	mat2x() {}
	mat2x(
		simd_t m00, simd_t m01,
		simd_t m10, simd_t m11);
};

template <typename simd_t>
union mat3x
{
	struct
	{
		simd_t
			m00, m10, m20,
			m01, m11, m21,
			m02, m12, m22;
	};
	simd_t m[9];

	mat3x() {}
	mat3x(
		simd_t m00, simd_t m01, simd_t m02,
		simd_t m10, simd_t m11, simd_t m12,
		simd_t m20, simd_t m21, simd_t m22);
};

template <typename simd_t>
union mat4x
{
	struct
	{
		simd_t
			m00, m10, m20, m30,
			m01, m11, m21, m31,
			m02, m12, m22, m32,
			m03, m13, m23, m33;
	};
	simd_t m[16];

	mat4x() {}
	mat4x(
		simd_t m00, simd_t m01, simd_t m02, simd_t m03,
		simd_t m10, simd_t m11, simd_t m12, simd_t m13,
		simd_t m20, simd_t m21, simd_t m22, simd_t m23,
		simd_t m30, simd_t m31, simd_t m32, simd_t m33);
};



// Vec2 operators.
template <typename simd_t> static vec2x<simd_t> operator+(vec2x<simd_t> a, vec2x<simd_t> b) { vec2x result = { a.x + b.x, a.y + b.y }; return result; }
template <typename simd_t> static vec2x<simd_t>& operator+=(vec2x<simd_t>& a, vec2x<simd_t> b) { a = a + b; return a; }
template <typename simd_t> static vec2x<simd_t> operator-(vec2x<simd_t> a, vec2x<simd_t> b) { vec2x result = { a.x - b.x, a.y - b.y }; return result; }
template <typename simd_t> static vec2x<simd_t>& operator-=(vec2x<simd_t>& a, vec2x<simd_t> b) { a = a - b; return a; }
template <typename simd_t> static vec2x<simd_t> operator*(vec2x<simd_t> a, vec2x<simd_t> b) { vec2x result = { a.x * b.x, a.y * b.y }; return result; }
template <typename simd_t> static vec2x<simd_t>& operator*=(vec2x<simd_t>& a, vec2x<simd_t> b) { a = a * b; return a; }
template <typename simd_t> static vec2x<simd_t> operator/(vec2x<simd_t> a, vec2x<simd_t> b) { vec2x result = { a.x / b.x, a.y / b.y }; return result; }
template <typename simd_t> static vec2x<simd_t>& operator/=(vec2x<simd_t>& a, vec2x<simd_t> b) { a = a / b; return a; }

template <typename simd_t> static vec2x<simd_t> operator*(vec2x<simd_t> a, simd_t b) { vec2x result = { a.x * b, a.y * b }; return result; }
template <typename simd_t> static vec2x<simd_t> operator*(simd_t a, vec2x<simd_t> b) { return b * a; }
template <typename simd_t> static vec2x<simd_t>& operator*=(vec2x<simd_t>& a, simd_t b) { a = a * b; return a; }
template <typename simd_t> static vec2x<simd_t> operator/(vec2x<simd_t> a, simd_t b) { vec2x result = { a.x / b, a.y / b }; return result; }
template <typename simd_t> static vec2x<simd_t>& operator/=(vec2x<simd_t>& a, simd_t b) { a = a / b; return a; }

template <typename simd_t> static vec2x<simd_t> operator-(vec2x<simd_t> a) { return vec2x(-a.x, -a.y); }

template <typename simd_t> static auto operator==(vec2x<simd_t> a, vec2x<simd_t> b) { return a.x == b.x & a.y == b.y; }


// Vec3 operators.
template <typename simd_t> static vec3x<simd_t> operator+(vec3x<simd_t> a, vec3x<simd_t> b) { vec3x result = { a.x + b.x, a.y + b.y, a.z + b.z }; return result; }
template <typename simd_t> static vec3x<simd_t>& operator+=(vec3x<simd_t>& a, vec3x<simd_t> b) { a = a + b; return a; }
template <typename simd_t> static vec3x<simd_t> operator-(vec3x<simd_t> a, vec3x<simd_t> b) { vec3x result = { a.x - b.x, a.y - b.y, a.z - b.z }; return result; }
template <typename simd_t> static vec3x<simd_t>& operator-=(vec3x<simd_t>& a, vec3x<simd_t> b) { a = a - b; return a; }
template <typename simd_t> static vec3x<simd_t> operator*(vec3x<simd_t> a, vec3x<simd_t> b) { vec3x result = { a.x * b.x, a.y * b.y, a.z * b.z }; return result; }
template <typename simd_t> static vec3x<simd_t>& operator*=(vec3x<simd_t>& a, vec3x<simd_t> b) { a = a * b; return a; }
template <typename simd_t> static vec3x<simd_t> operator/(vec3x<simd_t> a, vec3x<simd_t> b) { vec3x result = { a.x / b.x, a.y / b.y, a.z / b.z }; return result; }
template <typename simd_t> static vec3x<simd_t>& operator/=(vec3x<simd_t>& a, vec3x<simd_t> b) { a = a / b; return a; }

template <typename simd_t> static vec3x<simd_t> operator*(vec3x<simd_t> a, simd_t b) { vec3x result = { a.x * b, a.y * b, a.z * b }; return result; }
template <typename simd_t> static vec3x<simd_t> operator*(simd_t a, vec3x<simd_t> b) { return b * a; }
template <typename simd_t> static vec3x<simd_t>& operator*=(vec3x<simd_t>& a, simd_t b) { a = a * b; return a; }
template <typename simd_t> static vec3x<simd_t> operator/(vec3x<simd_t> a, simd_t b) { vec3x result = { a.x / b, a.y / b, a.z / b }; return result; }
template <typename simd_t> static vec3x<simd_t>& operator/=(vec3x<simd_t>& a, simd_t b) { a = a / b; return a; }

template <typename simd_t> static vec3x<simd_t> operator-(vec3x<simd_t> a) { return vec3x(-a.x, -a.y, -a.z); }

template <typename simd_t> static auto operator==(vec3x<simd_t> a, vec3x<simd_t> b) { return a.x == b.x & a.y == b.y & a.z == b.z; }


// Vec4 operators.
template <typename simd_t> static vec4x<simd_t> operator+(vec4x<simd_t> a, vec4x<simd_t> b) { vec4x result = { a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w }; return result; }
template <typename simd_t> static vec4x<simd_t>& operator+=(vec4x<simd_t>& a, vec4x<simd_t> b) { a = a + b; return a; }
template <typename simd_t> static vec4x<simd_t> operator-(vec4x<simd_t> a, vec4x<simd_t> b) { vec4x result = { a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w }; return result; }
template <typename simd_t> static vec4x<simd_t>& operator-=(vec4x<simd_t>& a, vec4x<simd_t> b) { a = a - b; return a; }
template <typename simd_t> static vec4x<simd_t> operator*(vec4x<simd_t> a, vec4x<simd_t> b) { vec4x result = { a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w }; return result; }
template <typename simd_t> static vec4x<simd_t>& operator*=(vec4x<simd_t>& a, vec4x<simd_t> b) { a = a * b; return a; }
template <typename simd_t> static vec4x<simd_t> operator/(vec4x<simd_t> a, vec4x<simd_t> b) { vec4x result = { a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w }; return result; }
template <typename simd_t> static vec4x<simd_t> operator/=(vec4x<simd_t>& a, vec4x<simd_t> b) { a = a / b; return a; }

template <typename simd_t> static vec4x<simd_t> operator*(vec4x<simd_t> a, simd_t b) { vec4x result = { a.x * b, a.y * b, a.z * b, a.w * b }; return result; }
template <typename simd_t> static vec4x<simd_t> operator*(simd_t a, vec4x<simd_t> b) { return b * a; }
template <typename simd_t> static vec4x<simd_t>& operator*=(vec4x<simd_t>& a, simd_t b) { a = a * b; return a; }
template <typename simd_t> static vec4x<simd_t> operator/(vec4x<simd_t> a, simd_t b) { vec4x result = { a.x / b, a.y / b, a.z / b, a.w / b }; return result; }
template <typename simd_t> static vec4x<simd_t>& operator/=(vec4x<simd_t>& a, simd_t b) { a = a / b; return a; }

template <typename simd_t> static vec4x<simd_t> operator-(vec4x<simd_t> a) { return vec4x(-a.x, -a.y, -a.z, -a.w); }

template <typename simd_t> static auto operator==(vec4x<simd_t> a, vec4x<simd_t> b) { return a.x == b.x & a.y == b.y & a.z == b.z & a.w == b.w; }


template <typename simd_t> static simd_t dot(vec2x<simd_t> a, vec2x<simd_t> b) { simd_t result = fmadd(a.x, b.x, a.y * b.y); return result; }
template <typename simd_t> static simd_t dot(vec3x<simd_t> a, vec3x<simd_t> b) { simd_t result = fmadd(a.x, b.x, fmadd(a.y, b.y, a.z * b.z)); return result; }
template <typename simd_t> static simd_t dot(vec4x<simd_t> a, vec4x<simd_t> b) { simd_t result = fmadd(a.x, b.x, fmadd(a.y, b.y, fmadd(a.z, b.z, a.w * b.w))); return result; }

template <typename simd_t> static simd_t cross(vec2x<simd_t> a, vec2x<simd_t> b) { return fmsub(a.x, b.y, a.y * b.x); }
template <typename simd_t> static vec3x<simd_t> cross(vec3x<simd_t> a, vec3x<simd_t> b) { vec3x result = { fmsub(a.y, b.z, a.z * b.y), fmsub(a.z, b.x, a.x * b.z), fmsub(a.x, b.y, a.y * b.x) }; return result; }

template <typename simd_t> static simd_t squaredLength(vec2x<simd_t> a) { return dot(a, a); }
template <typename simd_t> static simd_t squaredLength(vec3x<simd_t> a) { return dot(a, a); }
template <typename simd_t> static simd_t squaredLength(vec4x<simd_t> a) { return dot(a, a); }

template <typename simd_t> static simd_t length(vec2x<simd_t> a) { return sqrt(squaredLength(a)); }
template <typename simd_t> static simd_t length(vec3x<simd_t> a) { return sqrt(squaredLength(a)); }
template <typename simd_t> static simd_t length(vec4x<simd_t> a) { return sqrt(squaredLength(a)); }

template <typename simd_t> static vec2x<simd_t> fmadd(vec2x<simd_t> a, vec2x<simd_t> b, vec2x<simd_t> c) { return { fmadd(a.x, b.x, c.x), fmadd(a.y, b.y, c.y) }; }
template <typename simd_t> static vec3x<simd_t> fmadd(vec3x<simd_t> a, vec3x<simd_t> b, vec3x<simd_t> c) { return { fmadd(a.x, b.x, c.x), fmadd(a.y, b.y, c.y), fmadd(a.z, b.z, c.z) }; }
template <typename simd_t> static vec4x<simd_t> fmadd(vec4x<simd_t> a, vec4x<simd_t> b, vec4x<simd_t> c) { return { fmadd(a.x, b.x, c.x), fmadd(a.y, b.y, c.y), fmadd(a.z, b.z, c.z), fmadd(a.w, b.w, c.w) }; }

template <typename simd_t, typename cmp_t>
static vec2x<simd_t> ifThen(cmp_t cond, vec2x<simd_t> ifCase, vec2x<simd_t> thenCase)
{ 
	return { ifThen(cond, ifCase.x, thenCase.x), ifThen(cond, ifCase.y, thenCase.y) }; 
}

template <typename simd_t, typename cmp_t>
static vec3x<simd_t> ifThen(cmp_t cond, vec3x<simd_t> ifCase, vec3x<simd_t> thenCase)
{
	return { ifThen(cond, ifCase.x, thenCase.x), ifThen(cond, ifCase.y, thenCase.y), ifThen(cond, ifCase.z, thenCase.z) };
}

template <typename simd_t, typename cmp_t>
static vec4x<simd_t> ifThen(cmp_t cond, vec4x<simd_t> ifCase, vec4x<simd_t> thenCase)
{
	return { ifThen(cond, ifCase.x, thenCase.x), ifThen(cond, ifCase.y, thenCase.y), ifThen(cond, ifCase.z, thenCase.z), ifThen(cond, ifCase.w, thenCase.w) };
}

template <typename simd_t, typename cmp_t>
static quatx<simd_t> ifThen(cmp_t cond, quatx<simd_t> ifCase, quatx<simd_t> thenCase)
{
	return { ifThen(cond, ifCase.x, thenCase.x), ifThen(cond, ifCase.y, thenCase.y), ifThen(cond, ifCase.z, thenCase.z), ifThen(cond, ifCase.w, thenCase.w) };
}

template <typename simd_t> static vec2x<simd_t> noz(vec2x<simd_t> a) { simd_t sl = squaredLength(a); return ifThen(sl == 0.f, vec2x<simd_t>::zero(), a * rsqrt(sl)); }
template <typename simd_t> static vec3x<simd_t> noz(vec3x<simd_t> a) { simd_t sl = squaredLength(a); return ifThen(sl == 0.f, vec3x<simd_t>::zero(), a * rsqrt(sl)); }
template <typename simd_t> static vec4x<simd_t> noz(vec4x<simd_t> a) { simd_t sl = squaredLength(a); return ifThen(sl == 0.f, vec4x<simd_t>::zero(), a * rsqrt(sl)); }

template <typename simd_t> static vec2x<simd_t> normalize(vec2x<simd_t> a) { simd_t l2 = squaredLength(a); return a * rsqrt(l2); }
template <typename simd_t> static vec3x<simd_t> normalize(vec3x<simd_t> a) { simd_t l2 = squaredLength(a); return a * rsqrt(l2); }
template <typename simd_t> static vec4x<simd_t> normalize(vec4x<simd_t> a) { simd_t l2 = squaredLength(a); return a * rsqrt(l2); }

template <typename simd_t> static vec2x<simd_t> abs(vec2x<simd_t> a) { return vec2x(abs(a.x), abs(a.y)); }
template <typename simd_t> static vec3x<simd_t> abs(vec3x<simd_t> a) { return vec3x(abs(a.x), abs(a.y), abs(a.z)); }
template <typename simd_t> static vec4x<simd_t> abs(vec4x<simd_t> a) { return vec4x(abs(a.x), abs(a.y), abs(a.z), abs(a.w)); }

template <typename simd_t> static vec2x<simd_t> round(vec2x<simd_t> a) { return vec2x(round(a.x), round(a.y)); }
template <typename simd_t> static vec3x<simd_t> round(vec3x<simd_t> a) { return vec3x(round(a.x), round(a.y), round(a.z)); }
template <typename simd_t> static vec4x<simd_t> round(vec4x<simd_t> a) { return vec4x(round(a.x), round(a.y), round(a.z), round(a.w)); }

template <typename simd_t> static quatx<simd_t> normalize(quatx<simd_t> a) { quatx<simd_t> result; result.v4 = normalize(a.v4); return result; }
template <typename simd_t> static quatx<simd_t> conjugate(quatx<simd_t> a) { return { -a.x, -a.y, -a.z, a.w }; }

template <typename simd_t> static quatx<simd_t> operator+(quatx<simd_t> a, quatx<simd_t> b) { quatx result; result.v4 = a.v4 + b.v4; return result; }

template <typename simd_t>
static quatx<simd_t> operator*(quatx<simd_t> a, quatx<simd_t> b)
{
	quatx<simd_t> result;
	result.w = fmsub(a.w, b.w, dot(a.v, b.v));
	result.v = a.v * b.w + b.v * a.w + cross(a.v, b.v);
	return result;
}

template <typename simd_t> static quatx<simd_t> operator*(quatx<simd_t> q, simd_t s) { quatx result; result.v4 = q.v4 * s;	return result; }
template <typename simd_t> static vec3x<simd_t> operator*(quatx<simd_t> q, vec3x<simd_t> v) { quatx p(v.x, v.y, v.z, simd_t::zero()); return (q * p * conjugate(q)).v; }

template <typename simd_t> static auto operator==(quatx<simd_t> a, quatx<simd_t> b) { return a.x == b.x & a.y == b.y & a.z == b.z & a.w == b.w; }

template <typename simd_t> static vec2x<simd_t> lerp(vec2x<simd_t> l, vec2x<simd_t> u, simd_t t) { return fmadd(t, u - l, l); }
template <typename simd_t> static vec3x<simd_t> lerp(vec3x<simd_t> l, vec3x<simd_t> u, simd_t t) { return fmadd(t, u - l, l); }
template <typename simd_t> static vec4x<simd_t> lerp(vec4x<simd_t> l, vec4x<simd_t> u, simd_t t) { return fmadd(t, u - l, l); }
template <typename simd_t> static quatx<simd_t> lerp(quatx<simd_t> l, quatx<simd_t> u, simd_t t) { quatx result; result.v4 = lerp(l.v4, u.v4, t); return normalize(result); }

template <typename simd_t> static vec2x<simd_t> exp(vec2x<simd_t> v) { return vec2x(exp(v.x), exp(v.y)); }
template <typename simd_t> static vec3x<simd_t> exp(vec3x<simd_t> v) { return vec3x(exp(v.x), exp(v.y), exp(v.z)); }
template <typename simd_t> static vec4x<simd_t> exp(vec4x<simd_t> v) { return vec4x(exp(v.x), exp(v.y), exp(v.z), exp(v.w)); }

template <typename simd_t> static vec2x<simd_t> pow(vec2x<simd_t> v, simd_t e) { return vec2x(pow(v.x, e), pow(v.y, e)); }
template <typename simd_t> static vec3x<simd_t> pow(vec3x<simd_t> v, simd_t e) { return vec3x(pow(v.x, e), pow(v.y, e), pow(v.z, e)); }
template <typename simd_t> static vec4x<simd_t> pow(vec4x<simd_t> v, simd_t e) { return vec4x(pow(v.x, e), pow(v.y, e), pow(v.z, e), pow(v.w, e)); }


template <typename simd_t>
static vec2x<simd_t> operator*(const mat2x<simd_t>& a, vec2x<simd_t> b) 
{ 
	vec2x<simd_t> result;
	result.x = fmadd(a.m00, b.x, a.m01 * b.y);
	result.y = fmadd(a.m10, b.x, a.m11 * b.y);
	return result;
}

template <typename simd_t>
static vec3x<simd_t> operator*(const mat3x<simd_t>& a, vec3x<simd_t> b)
{
	vec3x<simd_t> result;
	result.x = fmadd(a.m00, b.x, fmadd(a.m01, b.y, a.m02 * b.z));
	result.y = fmadd(a.m10, b.x, fmadd(a.m11, b.y, a.m12 * b.z));
	result.z = fmadd(a.m20, b.x, fmadd(a.m21, b.y, a.m22 * b.z));
	return result;
}

template <typename simd_t>
static vec4x<simd_t> operator*(const mat4x<simd_t>& a, vec4x<simd_t> b)
{
	vec4x<simd_t> result;
	result.x = fmadd(a.m00, b.x, fmadd(a.m01, b.y, fmadd(a.m02, b.z, a.m03 * b.w)));
	result.y = fmadd(a.m10, b.x, fmadd(a.m11, b.y, fmadd(a.m12, b.z, a.m13 * b.w)));
	result.z = fmadd(a.m20, b.x, fmadd(a.m21, b.y, fmadd(a.m22, b.z, a.m23 * b.w)));
	result.w = fmadd(a.m30, b.x, fmadd(a.m31, b.y, fmadd(a.m32, b.z, a.m33 * b.w)));
	return result;
}

template <typename simd_t>
static mat2x<simd_t> operator*(const mat2x<simd_t>& a, const mat2x<simd_t>& b)
{
	mat2x<simd_t> result;

	result.m00 = fmadd(a.m00, b.m00, a.m01 * b.m10);
	result.m01 = fmadd(a.m00, b.m01, a.m01 * b.m11);

	result.m10 = fmadd(a.m10, b.m00, a.m11 * b.m10);
	result.m11 = fmadd(a.m10, b.m01, a.m11 * b.m11);

	return result;
}

template <typename simd_t>
static mat3x<simd_t> operator*(const mat3x<simd_t>& a, const mat3x<simd_t>& b)
{
	mat3x<simd_t> result;

	result.m00 = fmadd(a.m00, b.m00, fmadd(a.m01, b.m10, a.m02 * b.m20));
	result.m01 = fmadd(a.m00, b.m01, fmadd(a.m01, b.m11, a.m02 * b.m21));
	result.m02 = fmadd(a.m00, b.m02, fmadd(a.m01, b.m12, a.m02 * b.m22));

	result.m10 = fmadd(a.m10, b.m00, fmadd(a.m11, b.m10, a.m12 * b.m20));
	result.m11 = fmadd(a.m10, b.m01, fmadd(a.m11, b.m11, a.m12 * b.m21));
	result.m12 = fmadd(a.m10, b.m02, fmadd(a.m11, b.m12, a.m12 * b.m22));

	result.m20 = fmadd(a.m20, b.m00, fmadd(a.m21, b.m10, a.m22 * b.m20));
	result.m21 = fmadd(a.m20, b.m01, fmadd(a.m21, b.m11, a.m22 * b.m21));
	result.m22 = fmadd(a.m20, b.m02, fmadd(a.m21, b.m12, a.m22 * b.m22));
	
	return result;
}

template <typename simd_t>
static mat4x<simd_t> operator*(const mat4x<simd_t>& a, const mat4x<simd_t>& b)
{
	mat4x<simd_t> result;

	result.m00 = fmadd(a.m00, b.m00, fmadd(a.m01, b.m10, fmadd(a.m02, b.m20, a.m03 * b.m30)));
	result.m01 = fmadd(a.m00, b.m01, fmadd(a.m01, b.m11, fmadd(a.m02, b.m21, a.m03 * b.m31)));
	result.m02 = fmadd(a.m00, b.m02, fmadd(a.m01, b.m12, fmadd(a.m02, b.m22, a.m03 * b.m32)));
	result.m03 = fmadd(a.m00, b.m03, fmadd(a.m01, b.m13, fmadd(a.m02, b.m23, a.m03 * b.m33)));

	result.m10 = fmadd(a.m10, b.m00, fmadd(a.m11, b.m10, fmadd(a.m12, b.m20, a.m13 * b.m30)));
	result.m11 = fmadd(a.m10, b.m01, fmadd(a.m11, b.m11, fmadd(a.m12, b.m21, a.m13 * b.m31)));
	result.m12 = fmadd(a.m10, b.m02, fmadd(a.m11, b.m12, fmadd(a.m12, b.m22, a.m13 * b.m32)));
	result.m13 = fmadd(a.m10, b.m03, fmadd(a.m11, b.m13, fmadd(a.m12, b.m23, a.m13 * b.m33)));

	result.m20 = fmadd(a.m20, b.m00, fmadd(a.m21, b.m10, fmadd(a.m22, b.m20, a.m23 * b.m30)));
	result.m21 = fmadd(a.m20, b.m01, fmadd(a.m21, b.m11, fmadd(a.m22, b.m21, a.m23 * b.m31)));
	result.m22 = fmadd(a.m20, b.m02, fmadd(a.m21, b.m12, fmadd(a.m22, b.m22, a.m23 * b.m32)));
	result.m23 = fmadd(a.m20, b.m03, fmadd(a.m21, b.m13, fmadd(a.m22, b.m23, a.m23 * b.m33)));

	result.m30 = fmadd(a.m30, b.m00, fmadd(a.m31, b.m10, fmadd(a.m32, b.m20, a.m33 * b.m30)));
	result.m31 = fmadd(a.m30, b.m01, fmadd(a.m31, b.m11, fmadd(a.m32, b.m21, a.m33 * b.m31)));
	result.m32 = fmadd(a.m30, b.m02, fmadd(a.m31, b.m12, fmadd(a.m32, b.m22, a.m33 * b.m32)));
	result.m33 = fmadd(a.m30, b.m03, fmadd(a.m31, b.m13, fmadd(a.m32, b.m23, a.m33 * b.m33)));

	return result;
}

template <typename simd_t> static mat2x<simd_t> operator*(const mat2x<simd_t>& a, simd_t b) { mat2x<simd_t> result; for (uint32 i = 0; i < 4; ++i) { result.m[i] = a.m[i] * b; } return result; }
template <typename simd_t> static mat3x<simd_t> operator*(const mat3x<simd_t>& a, simd_t b) { mat3x<simd_t> result; for (uint32 i = 0; i < 9; ++i) { result.m[i] = a.m[i] * b; } return result; }
template <typename simd_t> static mat4x<simd_t> operator*(const mat4x<simd_t>& a, simd_t b) { mat4x<simd_t> result; for (uint32 i = 0; i < 16; ++i) { result.m[i] = a.m[i] * b; } return result; }

template <typename simd_t> static mat2x<simd_t> operator*(simd_t b, const mat2x<simd_t>& a) { return a * b; }
template <typename simd_t> static mat3x<simd_t> operator*(simd_t b, const mat3x<simd_t>& a) { return a * b; }
template <typename simd_t> static mat4x<simd_t> operator*(simd_t b, const mat4x<simd_t>& a) { return a * b; }

template <typename simd_t> static mat2x<simd_t> operator+(const mat2x<simd_t>& a, const mat2x<simd_t>& b) { mat2x<simd_t> result; for (uint32 i = 0; i < 4; ++i) { result.m[i] = a.m[i] + b.m[i]; } return result; }
template <typename simd_t> static mat3x<simd_t> operator+(const mat3x<simd_t>& a, const mat3x<simd_t>& b) { mat3x<simd_t> result; for (uint32 i = 0; i < 9; ++i) { result.m[i] = a.m[i] + b.m[i]; } return result; }
template <typename simd_t> static mat4x<simd_t> operator+(const mat4x<simd_t>& a, const mat4x<simd_t>& b) { mat4x<simd_t> result; for (uint32 i = 0; i < 16; ++i) { result.m[i] = a.m[i] + b.m[i]; } return result; }

template <typename simd_t>
static mat2x<simd_t> transpose(const mat2x<simd_t>& a)
{
	mat2x<simd_t> result;
	result.m00 = a.m00; result.m01 = a.m10;
	result.m10 = a.m01; result.m11 = a.m11;
	return result;
}

template <typename simd_t>
static mat3x<simd_t> transpose(const mat3x<simd_t>& a)
{
	mat3x<simd_t> result;
	result.m00 = a.m00; result.m01 = a.m10; result.m02 = a.m20;
	result.m10 = a.m01; result.m11 = a.m11; result.m12 = a.m21;
	result.m20 = a.m02; result.m21 = a.m12; result.m22 = a.m22;
	return result;
}

template <typename simd_t>
static mat4x<simd_t> transpose(const mat4x<simd_t>& a)
{
	mat4x<simd_t> result;
	result.m00 = a.m00; result.m01 = a.m10; result.m02 = a.m20; result.m03 = a.m30;
	result.m10 = a.m01; result.m11 = a.m11; result.m12 = a.m21; result.m13 = a.m31;
	result.m20 = a.m02; result.m21 = a.m12; result.m22 = a.m22; result.m23 = a.m32;
	result.m30 = a.m03; result.m31 = a.m13; result.m32 = a.m23; result.m33 = a.m32;
	return result;
}

template <typename simd_t>
static mat3x<simd_t> getSkewMatrix(vec3x<simd_t> r)
{
	simd_t zero = simd_t::zero();
	mat3x<simd_t> result;
	result.m00 = zero;
	result.m01 = -r.z;
	result.m02 = r.y;
	result.m10 = r.z;
	result.m11 = zero;
	result.m12 = -r.x;
	result.m20 = -r.y;
	result.m21 = r.x;
	result.m22 = zero;
	return result;
}

template <typename simd_t>
static vec2x<simd_t> solveLinearSystem(const mat2x<simd_t>& A, vec2x<simd_t> b)
{
	vec2x<simd_t> ex(A.m00, A.m10);
	vec2x<simd_t> ey(A.m01, A.m11);
	simd_t det = cross(ex, ey);
	det = ifThen(det != simd_t::zero(), 1.f / det, det);

	vec2x<simd_t> x;
	x.x = det * cross(b, ey);
	x.y = det * cross(ex, b);
	return x;
}

template <typename simd_t>
static vec3x<simd_t> solveLinearSystem(const mat3x<simd_t>& A, vec3x<simd_t> b)
{
	vec3x<simd_t> ex(A.m00, A.m10, A.m20);
	vec3x<simd_t> ey(A.m01, A.m11, A.m21);
	vec3x<simd_t> ez(A.m02, A.m12, A.m22);
	simd_t det = dot(ex, cross(ey, ez));
	det = ifThen(det != simd_t::zero(), 1.f / det, det);

	vec3x<simd_t> x;
	x.x = det * dot(b, cross(ey, ez));
	x.y = det * dot(ex, cross(b, ez));
	x.z = det * dot(ex, cross(ey, b));
	return x;
}

template <typename simd_t>
static quatx<simd_t> rotateFromTo(vec3x<simd_t> _from, vec3x<simd_t> _to)
{
	vec3x<simd_t> from = normalize(_from);
	vec3x<simd_t> to = normalize(_to);

	simd_t zero = simd_t::zero();
	simd_t one = 1.f;

	simd_t d = dot(from, to);
	auto same = d >= 1.f;

	auto largeRotation = d < (1e-6f - 1.f);
	quatx<simd_t> q0, q1;

	if (anyTrue(largeRotation))
	{
		// Rotate 180° around some axis.
		vec3x<simd_t> axis = cross(vec3x<simd_t>(one, zero, zero), from);
		axis = ifThen(squaredLength(axis) == zero, cross(vec3x<simd_t>(zero, one, zero), from), axis);
		axis = normalize(axis);
		q0 = normalize(quatx<simd_t>(axis, M_PI));
	}

	if (anyFalse(largeRotation))
	{
		simd_t s = sqrt((one + d) * simd_t(2.f));
		simd_t invs = one / s;

		vec3x<simd_t> c = cross(from, to);

		q1.x = c.x * invs;
		q1.y = c.y * invs;
		q1.z = c.z * invs;
		q1.w = s * simd_t(0.5f);
		q1 = normalize(q1);
	}

	quatx<simd_t> q = ifThen(largeRotation, q0, q1);
	q = ifThen(same, quatx<simd_t>(zero, zero, zero, one), q);
	return q;
}

template <typename simd_t>
void getAxisRotation(quatx<simd_t> q, vec3x<simd_t>& axis, simd_t& angle)
{
	simd_t zero = simd_t::zero();
	simd_t one = 1.f;

	angle = zero;
	axis = vec3x<simd_t>(one, zero, zero);

	simd_t sqLength = squaredLength(q.v);
	auto mask = sqLength > zero;
	if (anyTrue(mask))
	{
		simd_t angleOverride = simd_t(2.f) * acos(q.w);
		simd_t invLength = one / sqrt(sqLength);
		vec3x<simd_t> axisOverride = q.v * invLength;

		angle = ifThen(mask, angleOverride, angle);
		axis = ifThen(mask, axisOverride, axis);
	}
}

template <typename simd_t>
vec3x<simd_t> getTangent(vec3x<simd_t> normal)
{
	auto mask = abs(normal.x) > simd_t(0.57735f);
	vec3x<simd_t> tangent = ifThen(mask, vec3x<simd_t>(normal.y, -normal.x, simd_t::zero()), vec3x<simd_t>(simd_t::zero(), normal.z, -normal.y));
	return normalize(tangent);
}

template <typename simd_t>
void getTangents(vec3x<simd_t> normal, vec3x<simd_t>& outTangent, vec3x<simd_t>& outBitangent)
{
	outTangent = getTangent(normal);
	outBitangent = cross(normal, outTangent);
}


template<typename simd_t>
inline quatx<simd_t>::quatx(vec3x<simd_t> axis, simd_t angle)
{
	simd_t h = 0.5f;
	w = cos(angle * h);
	v = axis * sin(angle * h);
}

template<typename simd_t>
inline mat2x<simd_t>::mat2x(
	simd_t m00, simd_t m01, 
	simd_t m10, simd_t m11)
	:
	m00(m00), m01(m01),
	m10(m10), m11(m11) {}

template<typename simd_t>
inline mat3x<simd_t>::mat3x(
	simd_t m00, simd_t m01, simd_t m02, 
	simd_t m10, simd_t m11, simd_t m12, 
	simd_t m20, simd_t m21, simd_t m22)
	:
	m00(m00), m01(m01), m02(m02),
	m10(m10), m11(m11), m12(m12),
	m20(m20), m21(m21), m22(m22) {}

template<typename simd_t>
inline mat4x<simd_t>::mat4x(
	simd_t m00, simd_t m01, simd_t m02, simd_t m03, 
	simd_t m10, simd_t m11, simd_t m12, simd_t m13, 
	simd_t m20, simd_t m21, simd_t m22, simd_t m23, 
	simd_t m30, simd_t m31, simd_t m32, simd_t m33)
	:
	m00(m00), m01(m01), m02(m02), m03(m03),
	m10(m10), m11(m11), m12(m12), m13(m13),
	m20(m20), m21(m21), m22(m22), m23(m23),
	m30(m30), m31(m31), m32(m32), m33(m33) {}
