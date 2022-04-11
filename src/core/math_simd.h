#pragma once

#include "simd.h"
#include "soa.h"


struct w4_float;
struct w8_float;
struct w16_float;

template <typename simd_t>
union wN_vec2
{
	struct
	{
		simd_t x, y;
	};
	simd_t data[2];

	wN_vec2() {}
	wN_vec2(simd_t v) : wN_vec2(v, v) {}
	wN_vec2(simd_t x, simd_t y) : x(x), y(y) {}
	wN_vec2(soa_vec2 v, uint32 offset) : x(v.x + offset), y(v.y + offset) {}

	void store(float* xDest, float* yDest) { x.store(xDest); y.store(yDest); }

	static wN_vec2 zero() { return wN_vec2<simd_t>(simd_t::zero()); }
};

template <typename simd_t>
union wN_vec3
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
		wN_vec2<simd_t> xy;
		simd_t z;
	};
	simd_t data[3];

	wN_vec3() {}
	wN_vec3(simd_t v) : wN_vec3(v, v, v) {}
	wN_vec3(simd_t x, simd_t y, simd_t z) : x(x), y(y), z(z) {}
	wN_vec3(wN_vec2<simd_t> xy, simd_t z) : x(xy.x), y(xy.y), z(z) {}
	wN_vec3(soa_vec3 v, uint32 offset) : x(v.x + offset), y(v.y + offset), z(v.z + offset) {}

	void store(float* xDest, float* yDest, float* zDest) { x.store(xDest); y.store(yDest); z.store(zDest); }

	static wN_vec3 zero() { return wN_vec3<simd_t>(simd_t::zero()); }
};

template <typename simd_t>
union wN_vec4
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
		wN_vec3<simd_t> xyz;
		simd_t w;
	};
	struct
	{
		wN_vec2<simd_t> xy;
		wN_vec2<simd_t> zw;
	};
	simd_t data[4];

	wN_vec4() {}
	wN_vec4(simd_t v) : wN_vec4(v, v, v, v) {}
	wN_vec4(simd_t x, simd_t y, simd_t z, simd_t w) : x(x), y(y), z(z), w(w) {}
	wN_vec4(wN_vec3<simd_t> xyz, simd_t w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}
	wN_vec4(soa_vec4 v, uint32 offset) : x(v.x + offset), y(v.y + offset), z(v.z + offset), w(v.w + offset) {}

	void store(float* xDest, float* yDest, float* zDest, float* wDest) { x.store(xDest); y.store(yDest); z.store(zDest); w.store(wDest); }

	static wN_vec4 zero() { return wN_vec4<simd_t>(simd_t::zero()); }
};

template <typename simd_t>
union wN_quat
{
	struct
	{
		simd_t x, y, z, w;
	};
	struct
	{
		wN_vec3<simd_t> v;
		simd_t cosHalfAngle;
	};
	wN_vec4<simd_t> v4;
	simd_t data[4];

	wN_quat() {}
	wN_quat(simd_t x, simd_t y, simd_t z, simd_t w) : x(x), y(y), z(z), w(w) {}
	wN_quat(wN_vec3<simd_t> axis, simd_t angle);
	wN_quat(soa_quat v, uint32 offset) : x(v.x + offset), y(v.y + offset), z(v.z + offset), w(v.w + offset) {}

	void store(float* xDest, float* yDest, float* zDest, float* wDest) { x.store(xDest); y.store(yDest); z.store(zDest); w.store(wDest); }

	static wN_quat identity() { return wN_quat<simd_t>(simd_t::zero(), simd_t::zero(), simd_t::zero(), simd_t(1.f)); }
};

template <typename simd_t>
union wN_mat2
{
	struct
	{
		simd_t
			m00, m10,
			m01, m11;
	};
	simd_t m[4];

	wN_mat2() {}
	wN_mat2(
		simd_t m00, simd_t m01,
		simd_t m10, simd_t m11);
};

template <typename simd_t>
union wN_mat3
{
	struct
	{
		simd_t
			m00, m10, m20,
			m01, m11, m21,
			m02, m12, m22;
	};
	simd_t m[9];

	wN_mat3() {}
	wN_mat3(
		simd_t m00, simd_t m01, simd_t m02,
		simd_t m10, simd_t m11, simd_t m12,
		simd_t m20, simd_t m21, simd_t m22);
};

template <typename simd_t>
union wN_mat4
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

	wN_mat4() {}
	wN_mat4(
		simd_t m00, simd_t m01, simd_t m02, simd_t m03,
		simd_t m10, simd_t m11, simd_t m12, simd_t m13,
		simd_t m20, simd_t m21, simd_t m22, simd_t m23,
		simd_t m30, simd_t m31, simd_t m32, simd_t m33);
};



// Vec2 operators.
template <typename simd_t> static wN_vec2<simd_t> operator+(wN_vec2<simd_t> a, wN_vec2<simd_t> b) { wN_vec2 result = { a.x + b.x, a.y + b.y }; return result; }
template <typename simd_t> static wN_vec2<simd_t>& operator+=(wN_vec2<simd_t>& a, wN_vec2<simd_t> b) { a = a + b; return a; }
template <typename simd_t> static wN_vec2<simd_t> operator-(wN_vec2<simd_t> a, wN_vec2<simd_t> b) { wN_vec2 result = { a.x - b.x, a.y - b.y }; return result; }
template <typename simd_t> static wN_vec2<simd_t>& operator-=(wN_vec2<simd_t>& a, wN_vec2<simd_t> b) { a = a - b; return a; }
template <typename simd_t> static wN_vec2<simd_t> operator*(wN_vec2<simd_t> a, wN_vec2<simd_t> b) { wN_vec2 result = { a.x * b.x, a.y * b.y }; return result; }
template <typename simd_t> static wN_vec2<simd_t>& operator*=(wN_vec2<simd_t>& a, wN_vec2<simd_t> b) { a = a * b; return a; }
template <typename simd_t> static wN_vec2<simd_t> operator/(wN_vec2<simd_t> a, wN_vec2<simd_t> b) { wN_vec2 result = { a.x / b.x, a.y / b.y }; return result; }
template <typename simd_t> static wN_vec2<simd_t>& operator/=(wN_vec2<simd_t>& a, wN_vec2<simd_t> b) { a = a / b; return a; }

template <typename simd_t> static wN_vec2<simd_t> operator*(wN_vec2<simd_t> a, simd_t b) { wN_vec2 result = { a.x * b, a.y * b }; return result; }
template <typename simd_t> static wN_vec2<simd_t> operator*(simd_t a, wN_vec2<simd_t> b) { return b * a; }
template <typename simd_t> static wN_vec2<simd_t>& operator*=(wN_vec2<simd_t>& a, simd_t b) { a = a * b; return a; }
template <typename simd_t> static wN_vec2<simd_t> operator/(wN_vec2<simd_t> a, simd_t b) { wN_vec2 result = { a.x / b, a.y / b }; return result; }
template <typename simd_t> static wN_vec2<simd_t>& operator/=(wN_vec2<simd_t>& a, simd_t b) { a = a / b; return a; }

template <typename simd_t> static wN_vec2<simd_t> operator-(wN_vec2<simd_t> a) { return wN_vec2(-a.x, -a.y); }

template <typename simd_t> static auto operator==(wN_vec2<simd_t> a, wN_vec2<simd_t> b) { return a.x == b.x & a.y == b.y; }


// Vec3 operators.
template <typename simd_t> static wN_vec3<simd_t> operator+(wN_vec3<simd_t> a, wN_vec3<simd_t> b) { wN_vec3 result = { a.x + b.x, a.y + b.y, a.z + b.z }; return result; }
template <typename simd_t> static wN_vec3<simd_t>& operator+=(wN_vec3<simd_t>& a, wN_vec3<simd_t> b) { a = a + b; return a; }
template <typename simd_t> static wN_vec3<simd_t> operator-(wN_vec3<simd_t> a, wN_vec3<simd_t> b) { wN_vec3 result = { a.x - b.x, a.y - b.y, a.z - b.z }; return result; }
template <typename simd_t> static wN_vec3<simd_t>& operator-=(wN_vec3<simd_t>& a, wN_vec3<simd_t> b) { a = a - b; return a; }
template <typename simd_t> static wN_vec3<simd_t> operator*(wN_vec3<simd_t> a, wN_vec3<simd_t> b) { wN_vec3 result = { a.x * b.x, a.y * b.y, a.z * b.z }; return result; }
template <typename simd_t> static wN_vec3<simd_t>& operator*=(wN_vec3<simd_t>& a, wN_vec3<simd_t> b) { a = a * b; return a; }
template <typename simd_t> static wN_vec3<simd_t> operator/(wN_vec3<simd_t> a, wN_vec3<simd_t> b) { wN_vec3 result = { a.x / b.x, a.y / b.y, a.z / b.z }; return result; }
template <typename simd_t> static wN_vec3<simd_t>& operator/=(wN_vec3<simd_t>& a, wN_vec3<simd_t> b) { a = a / b; return a; }

template <typename simd_t> static wN_vec3<simd_t> operator*(wN_vec3<simd_t> a, simd_t b) { wN_vec3 result = { a.x * b, a.y * b, a.z * b }; return result; }
template <typename simd_t> static wN_vec3<simd_t> operator*(simd_t a, wN_vec3<simd_t> b) { return b * a; }
template <typename simd_t> static wN_vec3<simd_t>& operator*=(wN_vec3<simd_t>& a, simd_t b) { a = a * b; return a; }
template <typename simd_t> static wN_vec3<simd_t> operator/(wN_vec3<simd_t> a, simd_t b) { wN_vec3 result = { a.x / b, a.y / b, a.z / b }; return result; }
template <typename simd_t> static wN_vec3<simd_t>& operator/=(wN_vec3<simd_t>& a, simd_t b) { a = a / b; return a; }

template <typename simd_t> static wN_vec3<simd_t> operator-(wN_vec3<simd_t> a) { return wN_vec3(-a.x, -a.y, -a.z); }

template <typename simd_t> static auto operator==(wN_vec3<simd_t> a, wN_vec3<simd_t> b) { return a.x == b.x & a.y == b.y & a.z == b.z; }


// Vec4 operators.
template <typename simd_t> static wN_vec4<simd_t> operator+(wN_vec4<simd_t> a, wN_vec4<simd_t> b) { wN_vec4 result = { a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w }; return result; }
template <typename simd_t> static wN_vec4<simd_t>& operator+=(wN_vec4<simd_t>& a, wN_vec4<simd_t> b) { a = a + b; return a; }
template <typename simd_t> static wN_vec4<simd_t> operator-(wN_vec4<simd_t> a, wN_vec4<simd_t> b) { wN_vec4 result = { a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w }; return result; }
template <typename simd_t> static wN_vec4<simd_t>& operator-=(wN_vec4<simd_t>& a, wN_vec4<simd_t> b) { a = a - b; return a; }
template <typename simd_t> static wN_vec4<simd_t> operator*(wN_vec4<simd_t> a, wN_vec4<simd_t> b) { wN_vec4 result = { a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w }; return result; }
template <typename simd_t> static wN_vec4<simd_t>& operator*=(wN_vec4<simd_t>& a, wN_vec4<simd_t> b) { a = a * b; return a; }
template <typename simd_t> static wN_vec4<simd_t> operator/(wN_vec4<simd_t> a, wN_vec4<simd_t> b) { wN_vec4 result = { a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w }; return result; }
template <typename simd_t> static wN_vec4<simd_t> operator/=(wN_vec4<simd_t>& a, wN_vec4<simd_t> b) { a = a / b; return a; }

template <typename simd_t> static wN_vec4<simd_t> operator*(wN_vec4<simd_t> a, simd_t b) { wN_vec4 result = { a.x * b, a.y * b, a.z * b, a.w * b }; return result; }
template <typename simd_t> static wN_vec4<simd_t> operator*(simd_t a, wN_vec4<simd_t> b) { return b * a; }
template <typename simd_t> static wN_vec4<simd_t>& operator*=(wN_vec4<simd_t>& a, simd_t b) { a = a * b; return a; }
template <typename simd_t> static wN_vec4<simd_t> operator/(wN_vec4<simd_t> a, simd_t b) { wN_vec4 result = { a.x / b, a.y / b, a.z / b, a.w / b }; return result; }
template <typename simd_t> static wN_vec4<simd_t>& operator/=(wN_vec4<simd_t>& a, simd_t b) { a = a / b; return a; }

template <typename simd_t> static wN_vec4<simd_t> operator-(wN_vec4<simd_t> a) { return wN_vec4(-a.x, -a.y, -a.z, -a.w); }

template <typename simd_t> static auto operator==(wN_vec4<simd_t> a, wN_vec4<simd_t> b) { return a.x == b.x & a.y == b.y & a.z == b.z & a.w == b.w; }


template <typename simd_t> static simd_t dot(wN_vec2<simd_t> a, wN_vec2<simd_t> b) { simd_t result = fmadd(a.x, b.x, a.y * b.y); return result; }
template <typename simd_t> static simd_t dot(wN_vec3<simd_t> a, wN_vec3<simd_t> b) { simd_t result = fmadd(a.x, b.x, fmadd(a.y, b.y, a.z * b.z)); return result; }
template <typename simd_t> static simd_t dot(wN_vec4<simd_t> a, wN_vec4<simd_t> b) { simd_t result = fmadd(a.x, b.x, fmadd(a.y, b.y, fmadd(a.z, b.z, a.w * b.w))); return result; }

template <typename simd_t> static simd_t cross(wN_vec2<simd_t> a, wN_vec2<simd_t> b) { return fmsub(a.x, b.y, a.y * b.x); }
template <typename simd_t> static wN_vec3<simd_t> cross(wN_vec3<simd_t> a, wN_vec3<simd_t> b) { wN_vec3 result = { fmsub(a.y, b.z, a.z * b.y), fmsub(a.z, b.x, a.x * b.z), fmsub(a.x, b.y, a.y * b.x) }; return result; }

template <typename simd_t> static simd_t squaredLength(wN_vec2<simd_t> a) { return dot(a, a); }
template <typename simd_t> static simd_t squaredLength(wN_vec3<simd_t> a) { return dot(a, a); }
template <typename simd_t> static simd_t squaredLength(wN_vec4<simd_t> a) { return dot(a, a); }

template <typename simd_t> static simd_t length(wN_vec2<simd_t> a) { return sqrt(squaredLength(a)); }
template <typename simd_t> static simd_t length(wN_vec3<simd_t> a) { return sqrt(squaredLength(a)); }
template <typename simd_t> static simd_t length(wN_vec4<simd_t> a) { return sqrt(squaredLength(a)); }

template <typename simd_t> static wN_vec2<simd_t> fmadd(wN_vec2<simd_t> a, wN_vec2<simd_t> b, wN_vec2<simd_t> c) { return { fmadd(a.x, b.x, c.x), fmadd(a.y, b.y, c.y) }; }
template <typename simd_t> static wN_vec3<simd_t> fmadd(wN_vec3<simd_t> a, wN_vec3<simd_t> b, wN_vec3<simd_t> c) { return { fmadd(a.x, b.x, c.x), fmadd(a.y, b.y, c.y), fmadd(a.z, b.z, c.z) }; }
template <typename simd_t> static wN_vec4<simd_t> fmadd(wN_vec4<simd_t> a, wN_vec4<simd_t> b, wN_vec4<simd_t> c) { return { fmadd(a.x, b.x, c.x), fmadd(a.y, b.y, c.y), fmadd(a.z, b.z, c.z), fmadd(a.w, b.w, c.w) }; }

template <typename simd_t, typename cmp_t>
static wN_vec2<simd_t> ifThen(cmp_t cond, wN_vec2<simd_t> ifCase, wN_vec2<simd_t> thenCase)
{ 
	return { ifThen(cond, ifCase.x, thenCase.x), ifThen(cond, ifCase.y, thenCase.y) }; 
}

template <typename simd_t, typename cmp_t>
static wN_vec3<simd_t> ifThen(cmp_t cond, wN_vec3<simd_t> ifCase, wN_vec3<simd_t> thenCase)
{
	return { ifThen(cond, ifCase.x, thenCase.x), ifThen(cond, ifCase.y, thenCase.y), ifThen(cond, ifCase.z, thenCase.z) };
}

template <typename simd_t, typename cmp_t>
static wN_vec4<simd_t> ifThen(cmp_t cond, wN_vec4<simd_t> ifCase, wN_vec4<simd_t> thenCase)
{
	return { ifThen(cond, ifCase.x, thenCase.x), ifThen(cond, ifCase.y, thenCase.y), ifThen(cond, ifCase.z, thenCase.z), ifThen(cond, ifCase.w, thenCase.w) };
}

template <typename simd_t, typename cmp_t>
static wN_quat<simd_t> ifThen(cmp_t cond, wN_quat<simd_t> ifCase, wN_quat<simd_t> thenCase)
{
	return { ifThen(cond, ifCase.x, thenCase.x), ifThen(cond, ifCase.y, thenCase.y), ifThen(cond, ifCase.z, thenCase.z), ifThen(cond, ifCase.w, thenCase.w) };
}

template <typename simd_t> static wN_vec2<simd_t> noz(wN_vec2<simd_t> a) { simd_t sl = squaredLength(a); return ifThen(sl < 1e-8f, wN_vec2<simd_t>::zero(), a * rsqrt(sl)); }
template <typename simd_t> static wN_vec3<simd_t> noz(wN_vec3<simd_t> a) { simd_t sl = squaredLength(a); return ifThen(sl < 1e-8f, wN_vec3<simd_t>::zero(), a * rsqrt(sl)); }
template <typename simd_t> static wN_vec4<simd_t> noz(wN_vec4<simd_t> a) { simd_t sl = squaredLength(a); return ifThen(sl < 1e-8f, wN_vec4<simd_t>::zero(), a * rsqrt(sl)); }

template <typename simd_t> static wN_vec2<simd_t> normalize(wN_vec2<simd_t> a) { simd_t l2 = squaredLength(a); return a * rsqrt(l2); }
template <typename simd_t> static wN_vec3<simd_t> normalize(wN_vec3<simd_t> a) { simd_t l2 = squaredLength(a); return a * rsqrt(l2); }
template <typename simd_t> static wN_vec4<simd_t> normalize(wN_vec4<simd_t> a) { simd_t l2 = squaredLength(a); return a * rsqrt(l2); }

template <typename simd_t> static wN_vec2<simd_t> abs(wN_vec2<simd_t> a) { return wN_vec2(abs(a.x), abs(a.y)); }
template <typename simd_t> static wN_vec3<simd_t> abs(wN_vec3<simd_t> a) { return wN_vec3(abs(a.x), abs(a.y), abs(a.z)); }
template <typename simd_t> static wN_vec4<simd_t> abs(wN_vec4<simd_t> a) { return wN_vec4(abs(a.x), abs(a.y), abs(a.z), abs(a.w)); }

template <typename simd_t> static wN_vec2<simd_t> round(wN_vec2<simd_t> a) { return wN_vec2(round(a.x), round(a.y)); }
template <typename simd_t> static wN_vec3<simd_t> round(wN_vec3<simd_t> a) { return wN_vec3(round(a.x), round(a.y), round(a.z)); }
template <typename simd_t> static wN_vec4<simd_t> round(wN_vec4<simd_t> a) { return wN_vec4(round(a.x), round(a.y), round(a.z), round(a.w)); }

template <typename simd_t> static wN_quat<simd_t> normalize(wN_quat<simd_t> a) { wN_quat<simd_t> result; result.v4 = normalize(a.v4); return result; }
template <typename simd_t> static wN_quat<simd_t> conjugate(wN_quat<simd_t> a) { return { -a.x, -a.y, -a.z, a.w }; }

template <typename simd_t> static wN_quat<simd_t> operator+(wN_quat<simd_t> a, wN_quat<simd_t> b) { wN_quat result; result.v4 = a.v4 + b.v4; return result; }

template <typename simd_t>
static wN_quat<simd_t> operator*(wN_quat<simd_t> a, wN_quat<simd_t> b)
{
	wN_quat<simd_t> result;
	result.w = fmsub(a.w, b.w, dot(a.v, b.v));
	result.v = a.v * b.w + b.v * a.w + cross(a.v, b.v);
	return result;
}

template <typename simd_t> static wN_quat<simd_t> operator*(wN_quat<simd_t> q, simd_t s) { wN_quat result; result.v4 = q.v4 * s;	return result; }
template <typename simd_t> static wN_vec3<simd_t> operator*(wN_quat<simd_t> q, wN_vec3<simd_t> v) { wN_quat p(v.x, v.y, v.z, simd_t::zero()); return (q * p * conjugate(q)).v; }

template <typename simd_t> static auto operator==(wN_quat<simd_t> a, wN_quat<simd_t> b) { return a.x == b.x & a.y == b.y & a.z == b.z & a.w == b.w; }

template <typename simd_t> static wN_vec2<simd_t> lerp(wN_vec2<simd_t> l, wN_vec2<simd_t> u, simd_t t) { return fmadd(t, u - l, l); }
template <typename simd_t> static wN_vec3<simd_t> lerp(wN_vec3<simd_t> l, wN_vec3<simd_t> u, simd_t t) { return fmadd(t, u - l, l); }
template <typename simd_t> static wN_vec4<simd_t> lerp(wN_vec4<simd_t> l, wN_vec4<simd_t> u, simd_t t) { return fmadd(t, u - l, l); }
template <typename simd_t> static wN_quat<simd_t> lerp(wN_quat<simd_t> l, wN_quat<simd_t> u, simd_t t) { wN_quat result; result.v4 = lerp(l.v4, u.v4, t); return normalize(result); }

template <typename simd_t> static wN_vec2<simd_t> exp(wN_vec2<simd_t> v) { return wN_vec2(exp(v.x), exp(v.y)); }
template <typename simd_t> static wN_vec3<simd_t> exp(wN_vec3<simd_t> v) { return wN_vec3(exp(v.x), exp(v.y), exp(v.z)); }
template <typename simd_t> static wN_vec4<simd_t> exp(wN_vec4<simd_t> v) { return wN_vec4(exp(v.x), exp(v.y), exp(v.z), exp(v.w)); }

template <typename simd_t> static wN_vec2<simd_t> pow(wN_vec2<simd_t> v, simd_t e) { return wN_vec2(pow(v.x, e), pow(v.y, e)); }
template <typename simd_t> static wN_vec3<simd_t> pow(wN_vec3<simd_t> v, simd_t e) { return wN_vec3(pow(v.x, e), pow(v.y, e), pow(v.z, e)); }
template <typename simd_t> static wN_vec4<simd_t> pow(wN_vec4<simd_t> v, simd_t e) { return wN_vec4(pow(v.x, e), pow(v.y, e), pow(v.z, e), pow(v.w, e)); }


template <typename simd_t>
static wN_vec2<simd_t> operator*(const wN_mat2<simd_t>& a, wN_vec2<simd_t> b) 
{ 
	wN_vec2<simd_t> result;
	result.x = fmadd(a.m00, b.x, a.m01 * b.y);
	result.y = fmadd(a.m10, b.x, a.m11 * b.y);
	return result;
}

template <typename simd_t>
static wN_vec3<simd_t> operator*(const wN_mat3<simd_t>& a, wN_vec3<simd_t> b)
{
	wN_vec3<simd_t> result;
	result.x = fmadd(a.m00, b.x, fmadd(a.m01, b.y, a.m02 * b.z));
	result.y = fmadd(a.m10, b.x, fmadd(a.m11, b.y, a.m12 * b.z));
	result.z = fmadd(a.m20, b.x, fmadd(a.m21, b.y, a.m22 * b.z));
	return result;
}

template <typename simd_t>
static wN_vec4<simd_t> operator*(const wN_mat4<simd_t>& a, wN_vec4<simd_t> b)
{
	wN_vec4<simd_t> result;
	result.x = fmadd(a.m00, b.x, fmadd(a.m01, b.y, fmadd(a.m02, b.z, a.m03 * b.w)));
	result.y = fmadd(a.m10, b.x, fmadd(a.m11, b.y, fmadd(a.m12, b.z, a.m13 * b.w)));
	result.z = fmadd(a.m20, b.x, fmadd(a.m21, b.y, fmadd(a.m22, b.z, a.m23 * b.w)));
	result.w = fmadd(a.m30, b.x, fmadd(a.m31, b.y, fmadd(a.m32, b.z, a.m33 * b.w)));
	return result;
}

template <typename simd_t>
static wN_mat2<simd_t> operator*(const wN_mat2<simd_t>& a, const wN_mat2<simd_t>& b)
{
	wN_mat2<simd_t> result;

	result.m00 = fmadd(a.m00, b.m00, a.m01 * b.m10);
	result.m01 = fmadd(a.m00, b.m01, a.m01 * b.m11);

	result.m10 = fmadd(a.m10, b.m00, a.m11 * b.m10);
	result.m11 = fmadd(a.m10, b.m01, a.m11 * b.m11);

	return result;
}

template <typename simd_t>
static wN_mat3<simd_t> operator*(const wN_mat3<simd_t>& a, const wN_mat3<simd_t>& b)
{
	wN_mat3<simd_t> result;

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
static wN_mat4<simd_t> operator*(const wN_mat4<simd_t>& a, const wN_mat4<simd_t>& b)
{
	wN_mat4<simd_t> result;

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

template <typename simd_t> static wN_mat2<simd_t> operator*(const wN_mat2<simd_t>& a, simd_t b) { wN_mat2<simd_t> result; for (uint32 i = 0; i < 4; ++i) { result.m[i] = a.m[i] * b; } return result; }
template <typename simd_t> static wN_mat3<simd_t> operator*(const wN_mat3<simd_t>& a, simd_t b) { wN_mat3<simd_t> result; for (uint32 i = 0; i < 9; ++i) { result.m[i] = a.m[i] * b; } return result; }
template <typename simd_t> static wN_mat4<simd_t> operator*(const wN_mat4<simd_t>& a, simd_t b) { wN_mat4<simd_t> result; for (uint32 i = 0; i < 16; ++i) { result.m[i] = a.m[i] * b; } return result; }

template <typename simd_t> static wN_mat2<simd_t> operator*(simd_t b, const wN_mat2<simd_t>& a) { return a * b; }
template <typename simd_t> static wN_mat3<simd_t> operator*(simd_t b, const wN_mat3<simd_t>& a) { return a * b; }
template <typename simd_t> static wN_mat4<simd_t> operator*(simd_t b, const wN_mat4<simd_t>& a) { return a * b; }

template <typename simd_t> static wN_mat2<simd_t> operator+(const wN_mat2<simd_t>& a, const wN_mat2<simd_t>& b) { wN_mat2<simd_t> result; for (uint32 i = 0; i < 4; ++i) { result.m[i] = a.m[i] + b.m[i]; } return result; }
template <typename simd_t> static wN_mat3<simd_t> operator+(const wN_mat3<simd_t>& a, const wN_mat3<simd_t>& b) { wN_mat3<simd_t> result; for (uint32 i = 0; i < 9; ++i) { result.m[i] = a.m[i] + b.m[i]; } return result; }
template <typename simd_t> static wN_mat4<simd_t> operator+(const wN_mat4<simd_t>& a, const wN_mat4<simd_t>& b) { wN_mat4<simd_t> result; for (uint32 i = 0; i < 16; ++i) { result.m[i] = a.m[i] + b.m[i]; } return result; }

template <typename simd_t>
static wN_mat2<simd_t> transpose(const wN_mat2<simd_t>& a)
{
	wN_mat2<simd_t> result;
	result.m00 = a.m00; result.m01 = a.m10;
	result.m10 = a.m01; result.m11 = a.m11;
	return result;
}

template <typename simd_t>
static wN_mat3<simd_t> transpose(const wN_mat3<simd_t>& a)
{
	wN_mat3<simd_t> result;
	result.m00 = a.m00; result.m01 = a.m10; result.m02 = a.m20;
	result.m10 = a.m01; result.m11 = a.m11; result.m12 = a.m21;
	result.m20 = a.m02; result.m21 = a.m12; result.m22 = a.m22;
	return result;
}

template <typename simd_t>
static wN_mat4<simd_t> transpose(const wN_mat4<simd_t>& a)
{
	wN_mat4<simd_t> result;
	result.m00 = a.m00; result.m01 = a.m10; result.m02 = a.m20; result.m03 = a.m30;
	result.m10 = a.m01; result.m11 = a.m11; result.m12 = a.m21; result.m13 = a.m31;
	result.m20 = a.m02; result.m21 = a.m12; result.m22 = a.m22; result.m23 = a.m32;
	result.m30 = a.m03; result.m31 = a.m13; result.m32 = a.m23; result.m33 = a.m32;
	return result;
}

template <typename simd_t>
static wN_mat3<simd_t> getSkewMatrix(wN_vec3<simd_t> r)
{
	simd_t zero = simd_t::zero();
	wN_mat3<simd_t> result;
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
static wN_vec2<simd_t> solveLinearSystem(const wN_mat2<simd_t>& A, wN_vec2<simd_t> b)
{
	wN_vec2<simd_t> ex(A.m00, A.m10);
	wN_vec2<simd_t> ey(A.m01, A.m11);
	simd_t det = cross(ex, ey);
	det = ifThen(det != simd_t::zero(), 1.f / det, det);

	wN_vec2<simd_t> x;
	x.x = det * cross(b, ey);
	x.y = det * cross(ex, b);
	return x;
}

template <typename simd_t>
static wN_vec3<simd_t> solveLinearSystem(const wN_mat3<simd_t>& A, wN_vec3<simd_t> b)
{
	wN_vec3<simd_t> ex(A.m00, A.m10, A.m20);
	wN_vec3<simd_t> ey(A.m01, A.m11, A.m21);
	wN_vec3<simd_t> ez(A.m02, A.m12, A.m22);
	simd_t det = dot(ex, cross(ey, ez));
	det = ifThen(det != simd_t::zero(), 1.f / det, det);

	wN_vec3<simd_t> x;
	x.x = det * dot(b, cross(ey, ez));
	x.y = det * dot(ex, cross(b, ez));
	x.z = det * dot(ex, cross(ey, b));
	return x;
}

template <typename simd_t>
static wN_quat<simd_t> rotateFromTo(wN_vec3<simd_t> _from, wN_vec3<simd_t> _to)
{
	wN_vec3<simd_t> from = normalize(_from);
	wN_vec3<simd_t> to = normalize(_to);

	simd_t zero = simd_t::zero();
	simd_t one = 1.f;

	simd_t d = dot(from, to);
	auto same = d >= 1.f;

	auto largeRotation = d < (1e-6f - 1.f);
	wN_quat<simd_t> q0, q1;

	if (anyTrue(largeRotation))
	{
		// Rotate 180° around some axis.
		wN_vec3<simd_t> axis = cross(wN_vec3<simd_t>(one, zero, zero), from);
		axis = ifThen(squaredLength(axis) == zero, cross(wN_vec3<simd_t>(zero, one, zero), from), axis);
		axis = normalize(axis);
		q0 = normalize(wN_quat<simd_t>(axis, M_PI));
	}

	if (anyFalse(largeRotation))
	{
		simd_t s = sqrt((one + d) * simd_t(2.f));
		simd_t invs = one / s;

		wN_vec3<simd_t> c = cross(from, to);

		q1.x = c.x * invs;
		q1.y = c.y * invs;
		q1.z = c.z * invs;
		q1.w = s * simd_t(0.5f);
		q1 = normalize(q1);
	}

	wN_quat<simd_t> q = ifThen(largeRotation, q0, q1);
	q = ifThen(same, wN_quat<simd_t>(zero, zero, zero, one), q);
	return q;
}

template <typename simd_t>
void getAxisRotation(wN_quat<simd_t> q, wN_vec3<simd_t>& axis, simd_t& angle)
{
	simd_t zero = simd_t::zero();
	simd_t one = 1.f;

	angle = zero;
	axis = wN_vec3<simd_t>(one, zero, zero);

	simd_t sqLength = squaredLength(q.v);
	auto mask = sqLength > zero;
	if (anyTrue(mask))
	{
		simd_t angleOverride = simd_t(2.f) * acos(q.w);
		simd_t invLength = one / sqrt(sqLength);
		wN_vec3<simd_t> axisOverride = q.v * invLength;

		angle = ifThen(mask, angleOverride, angle);
		axis = ifThen(mask, axisOverride, axis);
	}
}

template <typename simd_t>
wN_vec3<simd_t> getTangent(wN_vec3<simd_t> normal)
{
	auto mask = abs(normal.x) > simd_t(0.57735f);
	wN_vec3<simd_t> tangent = ifThen(mask, wN_vec3<simd_t>(normal.y, -normal.x, simd_t::zero()), wN_vec3<simd_t>(simd_t::zero(), normal.z, -normal.y));
	return normalize(tangent);
}

template <typename simd_t>
void getTangents(wN_vec3<simd_t> normal, wN_vec3<simd_t>& outTangent, wN_vec3<simd_t>& outBitangent)
{
	outTangent = getTangent(normal);
	outBitangent = cross(normal, outTangent);
}


template<typename simd_t>
inline wN_quat<simd_t>::wN_quat(wN_vec3<simd_t> axis, simd_t angle)
{
	simd_t h = 0.5f;
	w = cos(angle * h);
	v = axis * sin(angle * h);
}

template<typename simd_t>
inline wN_mat2<simd_t>::wN_mat2(
	simd_t m00, simd_t m01, 
	simd_t m10, simd_t m11)
	:
	m00(m00), m01(m01),
	m10(m10), m11(m11) {}

template<typename simd_t>
inline wN_mat3<simd_t>::wN_mat3(
	simd_t m00, simd_t m01, simd_t m02, 
	simd_t m10, simd_t m11, simd_t m12, 
	simd_t m20, simd_t m21, simd_t m22)
	:
	m00(m00), m01(m01), m02(m02),
	m10(m10), m11(m11), m12(m12),
	m20(m20), m21(m21), m22(m22) {}

template<typename simd_t>
inline wN_mat4<simd_t>::wN_mat4(
	simd_t m00, simd_t m01, simd_t m02, simd_t m03, 
	simd_t m10, simd_t m11, simd_t m12, simd_t m13, 
	simd_t m20, simd_t m21, simd_t m22, simd_t m23, 
	simd_t m30, simd_t m31, simd_t m32, simd_t m33)
	:
	m00(m00), m01(m01), m02(m02), m03(m03),
	m10(m10), m11(m11), m12(m12), m13(m13),
	m20(m20), m21(m21), m22(m22), m23(m23),
	m30(m30), m31(m31), m32(m32), m33(m33) {}


#if defined(SIMD_SSE_2)
typedef wN_vec2<w4_float> w4_vec2;
typedef wN_vec3<w4_float> w4_vec3;
typedef wN_vec4<w4_float> w4_vec4;
typedef wN_quat<w4_float> w4_quat;
typedef wN_mat2<w4_float> w4_mat2;
typedef wN_mat3<w4_float> w4_mat3;
typedef wN_mat4<w4_float> w4_mat4;
#endif

#if defined(SIMD_AVX_2)
typedef wN_vec2<w8_float> w8_vec2;
typedef wN_vec3<w8_float> w8_vec3;
typedef wN_vec4<w8_float> w8_vec4;
typedef wN_quat<w8_float> w8_quat;
typedef wN_mat2<w8_float> w8_mat2;
typedef wN_mat3<w8_float> w8_mat3;
typedef wN_mat4<w8_float> w8_mat4;
#endif

#if defined(SIMD_AVX_512)
typedef wN_vec2<w16_float> w16_vec2;
typedef wN_vec3<w16_float> w16_vec3;
typedef wN_vec4<w16_float> w16_vec4;
typedef wN_quat<w16_float> w16_quat;
typedef wN_mat2<w16_float> w16_mat2;
typedef wN_mat3<w16_float> w16_mat3;
typedef wN_mat4<w16_float> w16_mat4;
#endif
