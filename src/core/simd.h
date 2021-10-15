#pragma once


#include <emmintrin.h>
#include <immintrin.h>

#define SIMD_SSE_2 // All x64 processors support SSE2.

#if defined(__AVX__)
#if defined(__AVX512F__)
#define SIMD_AVX_512
#define SIMD_AVX_2
#elif defined(__AVX2__)
#define SIMD_AVX_2
#else
#error Vanilla AVX not supported.
#endif
#endif


#define POLY0(x, c0) (c0)
#define POLY1(x, c0, c1) fmadd(POLY0(x, c1), x, (c0))
#define POLY2(x, c0, c1, c2) fmadd(POLY1(x, c1, c2), x, (c0))
#define POLY3(x, c0, c1, c2, c3) fmadd(POLY2(x, c1, c2, c3), x, (c0))
#define POLY4(x, c0, c1, c2, c3, c4) fmadd(POLY3(x, c1, c2, c3, c4), x, (c0))


template <typename float_t>
static float_t cosInternal(float_t x)
{
	const float_t tp = 1.f / (2.f * 3.14159265359f);
	const float_t q = 0.25f;
	const float_t h = 0.5f;
	const float_t o = 1.f;
	const float_t s = 16.f;
	const float_t v = 0.225f;

	x *= tp;
	x -= q + floor(x + q);
	x *= s * (abs(x) - h);
	x += v * x * (abs(x) - o);
	return x;
}

template <typename float_t> 
static float_t sinInternal(float_t x) 
{ 
	return cosInternal(x - (3.14159265359f * 0.5f)); 
}

template <typename float_t, typename int_t>
static float_t exp2Internal(float_t x)
{
	x = minimum(x, 129.00000f);
	x = maximum(x, -126.99999f);

	int_t ipart = convert(x - 0.5f);
	float_t fpart = x - convert(ipart);
	float_t expipart = reinterpret((ipart + 127) << 23);
	float_t expfpart = POLY3(fpart, 9.9992520e-1f, 6.9583356e-1f, 2.2606716e-1f, 7.8024521e-2f);
	return expipart * expfpart;
}

template <typename float_t, typename int_t>
static float_t log2Internal(float_t x)
{
	int_t exp = 0x7F800000;
	int_t mant = 0x007FFFFF;

	float_t one = 1;
	int_t i = reinterpret(x);

	float_t e = convert(((i & exp) >> 23) - 127);
	float_t m = reinterpret(i & mant) | one;
	float_t p = POLY4(m, 2.8882704548164776201f, -2.52074962577807006663f, 1.48116647521213171641f, -0.465725644288844778798f, 0.0596515482674574969533f);

	return fmadd(p, m - one, e);
}

template <typename float_t, typename int_t>
static float_t powInternal(float_t x, float_t y)
{
	return exp2Internal<float_t, int_t>(log2Internal<float_t, int_t>(x) * y);
}

template <typename float_t, typename int_t>
static float_t expInternal(float_t x)
{
	float_t a = 12102203.f; // (1 << 23) / log(2).
	int_t b = 127 * (1 << 23) - 298765;
	int_t t = convert(a * x) + b;
	return reinterpret(t);
}

template <typename float_t>
static float_t tanhInternal(float_t x)
{
	float_t a = exp(x);
	float_t b = exp(-x);
	return (a - b) / (a + b);
}

template <typename float_t, typename int_t>
static float_t atanInternal(float_t x)
{
	const int_t sign_mask = 0x80000000;
	const float_t b = 0.596227f;

	// Extract the sign bit.
	int_t ux_s = sign_mask & reinterpret(x);

	// Calculate the arctangent in the first quadrant.
	float_t bx_a = abs(b * x);
	float_t num = fmadd(x, x, bx_a);
	float_t atan_1q = num / (1.f + bx_a + num);

	// Restore the sign bit.
	int_t atan_2q = ux_s | reinterpret(atan_1q);
	return reinterpret(atan_2q) * float_t(3.14159265359f * 0.5f);
}

template <typename float_t, typename int_t>
static float_t atan2Internal(float_t y, float_t x)
{
	const int_t sign_mask = 0x80000000;
	const float_t b = 0.596227f;

	// Extract the sign bits.
	int_t ux_s = sign_mask & reinterpret(x);
	int_t uy_s = sign_mask & reinterpret(y);

	// Determine the quadrant offset.
	float_t q = convert(((~ux_s & uy_s) >> 29) | (ux_s >> 30));

	// Calculate the arctangent in the first quadrant.
	float_t bxy_a = abs(b * x * y);
	float_t num = fmadd(y, y, bxy_a);
	float_t atan_1q = num / (fmadd(x, x, bxy_a + num));

	// Translate it to the proper quadrant.
	int_t uatan_2q = (ux_s ^ uy_s) | reinterpret(atan_1q);

	float_t result04 = q + reinterpret(uatan_2q); // In the [0, 4) range for the 4 quadrants.

	auto negQuadrant = result04 >= 2.f;
	float_t result = ifThen(negQuadrant, result04 - 4.f, result04);
	return result * float_t(3.14159265359f * 0.5f);
}

template <typename float_t>
static float_t acosInternal(float_t x)
{
	// https://developer.download.nvidia.com/cg/acos.html

	float_t negate = ifThen(x < 0.f, 1.f, 0.f);
	x = abs(x);
	float_t ret = -0.0187293f;
	ret = fmadd(ret, x, 0.0742610f);
	ret = fmadd(ret, x, -0.2121144f);
	ret = fmadd(ret, x, 1.5707288f);
	ret = ret * sqrt(1.f - x);
	ret = ret - negate * ret * float_t(2.f);
	return fmadd(negate, 3.14159265359f, ret);
}


#if defined(SIMD_SSE_2)

struct floatx4
{
	__m128 f;

	floatx4() {}
	floatx4(float f_) { f = _mm_set1_ps(f_); }
	floatx4(__m128 f_) { f = f_; }
	floatx4(float a, float b, float c, float d) { f = _mm_setr_ps(a, b, c, d); }
	floatx4(const float* f_) { f = _mm_loadu_ps(f_); }

#if defined(SIMD_AVX_2)
	floatx4(const float* baseAddress, __m128i indices) { f = _mm_i32gather_ps(baseAddress, indices, 4); }
	floatx4(const float* baseAddress, int a, int b, int c, int d) : floatx4(baseAddress, _mm_setr_epi32(a, b, c, d)) {}
#else
	floatx4(const float* baseAddress, int a, int b, int c, int d) { f = _mm_setr_ps(baseAddress[a], baseAddress[b], baseAddress[c], baseAddress[d]);  }
	floatx4(const float* baseAddress, __m128i indices) : floatx4(baseAddress, indices.m128i_i32[0], indices.m128i_i32[1], indices.m128i_i32[2], indices.m128i_i32[3]) {}
#endif

	operator __m128() { return f; }

	void store(float* f_) const { _mm_storeu_ps(f_, f); }

#if defined(SIMD_AVX_512)
	void scatter(float* baseAddress, __m128i indices) { _mm_i32scatter_ps(baseAddress, indices, f, 4); }
	void scatter(float* baseAddress, int a, int b, int c, int d) { scatter(baseAddress, _mm_setr_epi32(a, b, c, d)); }
#else
	void scatter(float* baseAddress, int a, int b, int c, int d) const
	{
		baseAddress[a] = this->f.m128_f32[0];
		baseAddress[b] = this->f.m128_f32[1];
		baseAddress[c] = this->f.m128_f32[2];
		baseAddress[d] = this->f.m128_f32[3];
	}

	void scatter(float* baseAddress, __m128i indices) const
	{
		baseAddress[indices.m128i_i32[0]] = this->f.m128_f32[0];
		baseAddress[indices.m128i_i32[1]] = this->f.m128_f32[1];
		baseAddress[indices.m128i_i32[2]] = this->f.m128_f32[2];
		baseAddress[indices.m128i_i32[3]] = this->f.m128_f32[3];
	}
#endif

	static floatx4 allOnes() { const float nnan = (const float&)0xFFFFFFFF; return nnan; }
	static floatx4 zero() { return _mm_setzero_ps(); }
};

struct intx4
{
	__m128i i;

	intx4() {}
	intx4(int i_) { i = _mm_set1_epi32(i_); }
	intx4(__m128i i_) { i = i_; }
	intx4(int a, int b, int c, int d) { i = _mm_setr_epi32(a, b, c, d); }
	intx4(int* i_) { i = _mm_loadu_si128((const __m128i*)i_); }

#if defined(SIMD_AVX_2)
	intx4(const int* baseAddress, __m128i indices) { i = _mm_i32gather_epi32(baseAddress, indices, 4); }
	intx4(const int* baseAddress, int a, int b, int c, int d) : intx4(baseAddress, _mm_setr_epi32(a, b, c, d)) {}
#else
	intx4(const int* baseAddress, int a, int b, int c, int d) { i = _mm_setr_epi32(baseAddress[a], baseAddress[b], baseAddress[c], baseAddress[d]); }
	intx4(const int* baseAddress, __m128i indices) : intx4(baseAddress, indices.m128i_i32[0], indices.m128i_i32[1], indices.m128i_i32[2], indices.m128i_i32[3]) {}
#endif

	operator __m128i() { return i; }

	void store(int* i_) const { _mm_storeu_si128((__m128i*)i_, i); }

#if defined(SIMD_AVX_512)
	void scatter(int* baseAddress, __m128i indices) { _mm_i32scatter_epi32(baseAddress, indices, i, 4); }
	void scatter(int* baseAddress, int a, int b, int c, int d) { scatter(baseAddress, _mm_setr_epi32(a, b, c, d)); }
#else
	void scatter(int* baseAddress, int a, int b, int c, int d) const
	{
		baseAddress[a] = this->i.m128i_i32[0];
		baseAddress[b] = this->i.m128i_i32[1];
		baseAddress[c] = this->i.m128i_i32[2];
		baseAddress[d] = this->i.m128i_i32[3];
	}

	void scatter(int* baseAddress, __m128i indices) const
	{
		baseAddress[indices.m128i_i32[0]] = this->i.m128i_i32[0];
		baseAddress[indices.m128i_i32[1]] = this->i.m128i_i32[1];
		baseAddress[indices.m128i_i32[2]] = this->i.m128i_i32[2];
		baseAddress[indices.m128i_i32[3]] = this->i.m128i_i32[3];
	}
#endif

	static intx4 allOnes() { return UINT32_MAX; }
	static intx4 zero() { return _mm_setzero_si128(); }
};

static floatx4 convert(intx4 i) { return _mm_cvtepi32_ps(i); }
static intx4 convert(floatx4 f) { return _mm_cvtps_epi32(f); }
static floatx4 reinterpret(intx4 i) { return _mm_castsi128_ps(i); }
static intx4 reinterpret(floatx4 f) { return _mm_castps_si128(f); }


// Int operators.
static intx4 andNot(intx4 a, intx4 b) { return _mm_andnot_si128(a, b); }

static intx4 operator+(intx4 a, intx4 b) { return _mm_add_epi32(a, b); }
static intx4& operator+=(intx4& a, intx4 b) { a = a + b; return a; }
static intx4 operator-(intx4 a, intx4 b) { return _mm_sub_epi32(a, b); }
static intx4& operator-=(intx4& a, intx4 b) { a = a - b; return a; }
static intx4 operator*(intx4 a, intx4 b) { return _mm_mul_epi32(a, b); }
static intx4& operator*=(intx4& a, intx4 b) { a = a * b; return a; }
static intx4 operator/(intx4 a, intx4 b) { return _mm_div_epi32(a, b); }
static intx4& operator/=(intx4& a, intx4 b) { a = a / b; return a; }
static intx4 operator&(intx4 a, intx4 b) { return _mm_and_si128(a, b); }
static intx4& operator&=(intx4& a, intx4 b) { a = a & b; return a; }
static intx4 operator|(intx4 a, intx4 b) { return _mm_or_si128(a, b); }
static intx4& operator|=(intx4& a, intx4 b) { a = a | b; return a; }
static intx4 operator^(intx4 a, intx4 b) { return _mm_xor_si128(a, b); }
static intx4& operator^=(intx4& a, intx4 b) { a = a ^ b; return a; }

static intx4 operator~(intx4 a) { a = andNot(a, intx4::allOnes()); return a; }

static intx4 operator>>(intx4 a, int b) { return _mm_srli_epi32(a, b); }
static intx4& operator>>=(intx4& a, int b) { a = a >> b; return a; }
static intx4 operator<<(intx4 a, int b) { return _mm_slli_epi32(a, b); }
static intx4& operator<<=(intx4& a, int b) { a = a << b; return a; }

static intx4 operator-(intx4 a) { return _mm_sub_epi32(intx4::zero(), a); }



// Float operators.
static floatx4 andNot(floatx4 a, floatx4 b) { return _mm_andnot_ps(a, b); }

static floatx4 operator+(floatx4 a, floatx4 b) { return _mm_add_ps(a, b); }
static floatx4& operator+=(floatx4& a, floatx4 b) { a = a + b; return a; }
static floatx4 operator-(floatx4 a, floatx4 b) { return _mm_sub_ps(a, b); }
static floatx4& operator-=(floatx4& a, floatx4 b) { a = a - b; return a; }
static floatx4 operator*(floatx4 a, floatx4 b) { return _mm_mul_ps(a, b); }
static floatx4& operator*=(floatx4& a, floatx4 b) { a = a * b; return a; }
static floatx4 operator/(floatx4 a, floatx4 b) { return _mm_div_ps(a, b); }
static floatx4& operator/=(floatx4& a, floatx4 b) { a = a / b; return a; }
static floatx4 operator&(floatx4 a, floatx4 b) { return _mm_and_ps(a, b); }
static floatx4& operator&=(floatx4& a, floatx4 b) { a = a & b; return a; }
static floatx4 operator|(floatx4 a, floatx4 b) { return _mm_or_ps(a, b); }
static floatx4& operator|=(floatx4& a, floatx4 b) { a = a | b; return a; }
static floatx4 operator^(floatx4 a, floatx4 b) { return _mm_xor_ps(a, b); }
static floatx4& operator^=(floatx4& a, floatx4 b) { a = a ^ b; return a; }

static floatx4 operator~(floatx4 a) { a = andNot(a, floatx4::allOnes()); return a; }


static intx4 operator==(intx4 a, intx4 b) { return _mm_cmpeq_epi32(a, b); }
static intx4 operator!=(intx4 a, intx4 b) { return ~(a == b); }
static intx4 operator>(intx4 a, intx4 b) { return _mm_cmpgt_epi32(a, b); }
static intx4 operator>=(intx4 a, intx4 b) { return (a > b) | (a == b); }
static intx4 operator<(intx4 a, intx4 b) { return _mm_cmplt_epi32(a, b); }
static intx4 operator<=(intx4 a, intx4 b) { return (a < b) | (a == b); }

static floatx4 operator==(floatx4 a, floatx4 b) { return _mm_cmpeq_ps(a, b); }
static floatx4 operator!=(floatx4 a, floatx4 b) { return _mm_cmpneq_ps(a, b); }
static floatx4 operator>(floatx4 a, floatx4 b) { return _mm_cmpgt_ps(a, b); }
static floatx4 operator>=(floatx4 a, floatx4 b) { return _mm_cmpge_ps(a, b); }
static floatx4 operator<(floatx4 a, floatx4 b) { return _mm_cmplt_ps(a, b); }
static floatx4 operator<=(floatx4 a, floatx4 b) { return _mm_cmple_ps(a, b); }

static floatx4 operator>>(floatx4 a, int b) { return reinterpret(reinterpret(a) >> b); }
static floatx4& operator>>=(floatx4& a, int b) { a = a >> b; return a; }
static floatx4 operator<<(floatx4 a, int b) { return reinterpret(reinterpret(a) << b); }
static floatx4& operator<<=(floatx4& a, int b) { a = a << b; return a; }

static floatx4 operator-(floatx4 a) { return _mm_xor_ps(a, reinterpret(intx4(1 << 31))); }




static float addElements(floatx4 a) { __m128 aa = _mm_hadd_ps(a, a); aa = _mm_hadd_ps(aa, aa); return aa.m128_f32[0]; }

static floatx4 fmadd(floatx4 a, floatx4 b, floatx4 c) { return _mm_fmadd_ps(a, b, c); }
static floatx4 fmsub(floatx4 a, floatx4 b, floatx4 c) { return _mm_fmsub_ps(a, b, c); }

static floatx4 sqrt(floatx4 a) { return _mm_sqrt_ps(a); }
static floatx4 rsqrt(floatx4 a) { return _mm_rsqrt_ps(a); }

static floatx4 ifThen(floatx4 cond, floatx4 ifCase, floatx4 elseCase) { return _mm_blendv_ps(elseCase, ifCase, cond); }
static intx4 ifThen(intx4 cond, intx4 ifCase, intx4 elseCase) { return reinterpret(ifThen(reinterpret(cond), reinterpret(ifCase), reinterpret(elseCase))); }

static int toBitMask(floatx4 a) { return _mm_movemask_ps(a); }
static int toBitMask(intx4 a) { return toBitMask(reinterpret(a)); }

static bool allTrue(floatx4 a) { return toBitMask(a) == (1 << 4) - 1; }
static bool allFalse(floatx4 a) { return toBitMask(a) == 0; }
static bool anyTrue(floatx4 a) { return toBitMask(a) > 0; }
static bool anyFalse(floatx4 a) { return !allTrue(a); }

static bool allTrue(intx4 a) { return allTrue(reinterpret(a)); }
static bool allFalse(intx4 a) { return allFalse(reinterpret(a)); }
static bool anyTrue(intx4 a) { return anyTrue(reinterpret(a)); }
static bool anyFalse(intx4 a) { return anyFalse(reinterpret(a)); }

static floatx4 abs(floatx4 a) { floatx4 result = andNot(-0.f, a); return result; }
static floatx4 floor(floatx4 a) { return _mm_floor_ps(a); }
static floatx4 round(floatx4 a) { return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC); }
static floatx4 minimum(floatx4 a, floatx4 b) { return _mm_min_ps(a, b); }
static floatx4 maximum(floatx4 a, floatx4 b) { return _mm_max_ps(a, b); }

static floatx4 lerp(floatx4 l, floatx4 u, floatx4 t) { return fmadd(t, u - l, l); }
static floatx4 inverseLerp(floatx4 l, floatx4 u, floatx4 v) { return (v - l) / (u - l); }
static floatx4 remap(floatx4 v, floatx4 oldL, floatx4 oldU, floatx4 newL, floatx4 newU) { return lerp(newL, newU, inverseLerp(oldL, oldU, v)); }
static floatx4 clamp(floatx4 v, floatx4 l, floatx4 u) { return minimum(u, maximum(l, v)); }
static floatx4 clamp01(floatx4 v) { return clamp(v, 0.f, 1.f); }

static floatx4 signOf(floatx4 f) { floatx4 z = floatx4::zero(); return ifThen(f < z, floatx4(-1), ifThen(f == z, z, floatx4(1))); }
static floatx4 signbit(floatx4 f) { return (f & -0.f) >> 31; }

static floatx4 cos(floatx4 x) { return cosInternal(x); }
static floatx4 sin(floatx4 x) { return sinInternal(x); }
static floatx4 exp2(floatx4 x) { return exp2Internal<floatx4, intx4>(x); }
static floatx4 log2(floatx4 x) { return log2Internal<floatx4, intx4>(x); }
static floatx4 pow(floatx4 x, floatx4 y) { return powInternal<floatx4, intx4>(x, y); }
static floatx4 exp(floatx4 x) { return expInternal<floatx4, intx4>(x); }
static floatx4 tanh(floatx4 x) { return tanhInternal(x); }
static floatx4 atan(floatx4 x) { return atanInternal<floatx4, intx4>(x); }
static floatx4 atan2(floatx4 y, floatx4 x) { return atan2Internal<floatx4, intx4>(y, x); }
static floatx4 acos(floatx4 x) { return acosInternal(x); }

static intx4 fillWithFirstLane(intx4 a)
{
	intx4 first = _mm_shuffle_epi32(a, _MM_SHUFFLE(0, 0, 0, 0));
	return first;
}

static void transpose(floatx4& out0, floatx4& out1, floatx4& out2, floatx4& out3)
{
	_MM_TRANSPOSE4_PS(out0.f, out1.f, out2.f, out3.f);
}

static void load4(const float* baseAddress, const uint16* indices, uint32 stride,
	floatx4& out0, floatx4& out1, floatx4& out2, floatx4& out3)
{
	const uint32 strideInFloats = stride / sizeof(float);

	out0 = floatx4(baseAddress + strideInFloats * indices[0]);
	out1 = floatx4(baseAddress + strideInFloats * indices[1]);
	out2 = floatx4(baseAddress + strideInFloats * indices[2]);
	out3 = floatx4(baseAddress + strideInFloats * indices[3]);

	transpose(out0, out1, out2, out3);
}

static void store4(float* baseAddress, const uint16* indices, uint32 stride,
	floatx4 in0, floatx4 in1, floatx4 in2, floatx4 in3)
{
	const uint32 strideInFloats = stride / sizeof(float);

	transpose(in0, in1, in2, in3);

	in0.store(baseAddress + strideInFloats * indices[0]);
	in1.store(baseAddress + strideInFloats * indices[1]);
	in2.store(baseAddress + strideInFloats * indices[2]);
	in3.store(baseAddress + strideInFloats * indices[3]);
}

static void load8(const float* baseAddress, const uint16* indices, uint32 stride,
	floatx4& out0, floatx4& out1, floatx4& out2, floatx4& out3, floatx4& out4, floatx4& out5, floatx4& out6, floatx4& out7)
{
	const uint32 strideInFloats = stride / sizeof(float);

	out0 = floatx4(baseAddress + strideInFloats * indices[0]);
	out1 = floatx4(baseAddress + strideInFloats * indices[1]);
	out2 = floatx4(baseAddress + strideInFloats * indices[2]);
	out3 = floatx4(baseAddress + strideInFloats * indices[3]);
	out4 = floatx4(baseAddress + strideInFloats * indices[0] + 4);
	out5 = floatx4(baseAddress + strideInFloats * indices[1] + 4);
	out6 = floatx4(baseAddress + strideInFloats * indices[2] + 4);
	out7 = floatx4(baseAddress + strideInFloats * indices[3] + 4);

	transpose(out0, out1, out2, out3);
	transpose(out4, out5, out6, out7);
}

static void store8(float* baseAddress, const uint16* indices, uint32 stride,
	floatx4 in0, floatx4 in1, floatx4 in2, floatx4 in3, floatx4 in4, floatx4 in5, floatx4 in6, floatx4 in7)
{
	const uint32 strideInFloats = stride / sizeof(float);

	transpose(in0, in1, in2, in3);
	transpose(in4, in5, in6, in7);

	in0.store(baseAddress + strideInFloats * indices[0]);
	in1.store(baseAddress + strideInFloats * indices[1]);
	in2.store(baseAddress + strideInFloats * indices[2]);
	in3.store(baseAddress + strideInFloats * indices[3]);
	in4.store(baseAddress + strideInFloats * indices[0] + 4);
	in5.store(baseAddress + strideInFloats * indices[1] + 4);
	in6.store(baseAddress + strideInFloats * indices[2] + 4);
	in7.store(baseAddress + strideInFloats * indices[3] + 4);
}

#endif

#if defined(SIMD_AVX_2)

struct floatx8
{
	__m256 f;

	floatx8() {}
	floatx8(float f_) { f = _mm256_set1_ps(f_); }
	floatx8(__m256 f_) { f = f_; }
	floatx8(float a, float b, float c, float d, float e, float f, float g, float h) { this->f = _mm256_setr_ps(a, b, c, d, e, f, g, h); }
	floatx8(const float* f_) { f = _mm256_loadu_ps(f_); }

	floatx8(const float* baseAddress, __m256i indices) { f = _mm256_i32gather_ps(baseAddress, indices, 4); }
	floatx8(const float* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h) : floatx8(baseAddress, _mm256_setr_epi32(a, b, c, d, e, f, g, h)) {}

	operator __m256() { return f; }

	void store(float* f_) const { _mm256_storeu_ps(f_, f); }

#if defined(SIMD_AVX_512)
	void scatter(float* baseAddress, __m256i indices) { _mm256_i32scatter_ps(baseAddress, indices, f, 4); }
	void scatter(float* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h) { scatter(baseAddress, _mm256_setr_epi32(a, b, c, d, e, f, g, h)); }
#else
	void scatter(float* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h) const
	{
		baseAddress[a] = this->f.m256_f32[0];
		baseAddress[b] = this->f.m256_f32[1];
		baseAddress[c] = this->f.m256_f32[2];
		baseAddress[d] = this->f.m256_f32[3];
		baseAddress[e] = this->f.m256_f32[4];
		baseAddress[f] = this->f.m256_f32[5];
		baseAddress[g] = this->f.m256_f32[6];
		baseAddress[h] = this->f.m256_f32[7];
	}

	void scatter(float* baseAddress, __m256i indices) const
	{
		baseAddress[indices.m256i_i32[0]] = this->f.m256_f32[0];
		baseAddress[indices.m256i_i32[1]] = this->f.m256_f32[1];
		baseAddress[indices.m256i_i32[2]] = this->f.m256_f32[2];
		baseAddress[indices.m256i_i32[3]] = this->f.m256_f32[3];
		baseAddress[indices.m256i_i32[4]] = this->f.m256_f32[4];
		baseAddress[indices.m256i_i32[5]] = this->f.m256_f32[5];
		baseAddress[indices.m256i_i32[6]] = this->f.m256_f32[6];
		baseAddress[indices.m256i_i32[7]] = this->f.m256_f32[7];
	}
#endif

	static floatx8 allOnes() { const float nnan = (const float&)0xFFFFFFFF; return nnan; }
	static floatx8 zero() { return _mm256_setzero_ps(); }
};

struct intx8
{
	__m256i i;

	intx8() {}
	intx8(int i_) { i = _mm256_set1_epi32(i_); }
	intx8(__m256i i_) { i = i_; }
	intx8(int a, int b, int c, int d, int e, int f, int g, int h) { this->i = _mm256_setr_epi32(a, b, c, d, e, f, g, h); }
	intx8(const int* i_) { i = _mm256_loadu_epi32(i_); }

	intx8(const int* baseAddress, __m256i indices) { i = _mm256_i32gather_epi32(baseAddress, indices, 4); }
	intx8(const int* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h) : intx8(baseAddress, _mm256_setr_epi32(a, b, c, d, e, f, g, h)) {}

	operator __m256i() { return i; }

	void store(int* i_) const { _mm256_storeu_epi32(i_, i); }

#if defined(SIMD_AVX_512)
	void scatter(int* baseAddress, __m256i indices) { _mm256_i32scatter_epi32(baseAddress, indices, i, 4); }
	void scatter(int* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h) { scatter(baseAddress, _mm256_setr_epi32(a, b, c, d, e, f, g, h)); }
#else
	void scatter(int* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h) const
	{
		baseAddress[a] = this->i.m256i_i32[0];
		baseAddress[b] = this->i.m256i_i32[1];
		baseAddress[c] = this->i.m256i_i32[2];
		baseAddress[d] = this->i.m256i_i32[3];
		baseAddress[e] = this->i.m256i_i32[4];
		baseAddress[f] = this->i.m256i_i32[5];
		baseAddress[g] = this->i.m256i_i32[6];
		baseAddress[h] = this->i.m256i_i32[7];
	}

	void scatter(int* baseAddress, __m256i indices) const
	{
		baseAddress[indices.m256i_i32[0]] = this->i.m256i_i32[0];
		baseAddress[indices.m256i_i32[1]] = this->i.m256i_i32[1];
		baseAddress[indices.m256i_i32[2]] = this->i.m256i_i32[2];
		baseAddress[indices.m256i_i32[3]] = this->i.m256i_i32[3];
		baseAddress[indices.m256i_i32[4]] = this->i.m256i_i32[4];
		baseAddress[indices.m256i_i32[5]] = this->i.m256i_i32[5];
		baseAddress[indices.m256i_i32[6]] = this->i.m256i_i32[6];
		baseAddress[indices.m256i_i32[7]] = this->i.m256i_i32[7];
	}
#endif

	static intx8 allOnes() { return UINT32_MAX; }
	static intx8 zero() { return _mm256_setzero_si256(); }
};

static floatx8 convert(intx8 i) { return _mm256_cvtepi32_ps(i); }
static intx8 convert(floatx8 f) { return _mm256_cvtps_epi32(f); }
static floatx8 reinterpret(intx8 i) { return _mm256_castsi256_ps(i); }
static intx8 reinterpret(floatx8 f) { return _mm256_castps_si256(f); }


// Int operators.
static intx8 andNot(intx8 a, intx8 b) { return _mm256_andnot_si256(a, b); }

static intx8 operator+(intx8 a, intx8 b) { return _mm256_add_epi32(a, b); }
static intx8& operator+=(intx8& a, intx8 b) { a = a + b; return a; }
static intx8 operator-(intx8 a, intx8 b) { return _mm256_sub_epi32(a, b); }
static intx8& operator-=(intx8& a, intx8 b) { a = a - b; return a; }
static intx8 operator*(intx8 a, intx8 b) { return _mm256_mul_epi32(a, b); }
static intx8& operator*=(intx8& a, intx8 b) { a = a * b; return a; }
static intx8 operator/(intx8 a, intx8 b) { return _mm256_div_epi32(a, b); }
static intx8& operator/=(intx8& a, intx8 b) { a = a / b; return a; }
static intx8 operator&(intx8 a, intx8 b) { return _mm256_and_si256(a, b); }
static intx8& operator&=(intx8& a, intx8 b) { a = a & b; return a; }
static intx8 operator|(intx8 a, intx8 b) { return _mm256_or_si256(a, b); }
static intx8& operator|=(intx8& a, intx8 b) { a = a | b; return a; }
static intx8 operator^(intx8 a, intx8 b) { return _mm256_xor_si256(a, b); }
static intx8& operator^=(intx8& a, intx8 b) { a = a ^ b; return a; }

static intx8 operator~(intx8 a) { a = andNot(a, intx8::allOnes()); return a; }

static intx8 operator>>(intx8 a, int b) { return _mm256_srli_epi32(a, b); }
static intx8& operator>>=(intx8& a, int b) { a = a >> b; return a; }
static intx8 operator<<(intx8 a, int b) { return _mm256_slli_epi32(a, b); }
static intx8& operator<<=(intx8& a, int b) { a = a << b; return a; }

static intx8 operator-(intx8 a) { return _mm256_sub_epi32(intx8::zero(), a); }



// Float operators.
static floatx8 andNot(floatx8 a, floatx8 b) { return _mm256_andnot_ps(a, b); }

static floatx8 operator+(floatx8 a, floatx8 b) { return _mm256_add_ps(a, b); }
static floatx8& operator+=(floatx8& a, floatx8 b) { a = a + b; return a; }
static floatx8 operator-(floatx8 a, floatx8 b) { return _mm256_sub_ps(a, b); }
static floatx8& operator-=(floatx8& a, floatx8 b) { a = a - b; return a; }
static floatx8 operator*(floatx8 a, floatx8 b) { return _mm256_mul_ps(a, b); }
static floatx8& operator*=(floatx8& a, floatx8 b) { a = a * b; return a; }
static floatx8 operator/(floatx8 a, floatx8 b) { return _mm256_div_ps(a, b); }
static floatx8& operator/=(floatx8& a, floatx8 b) { a = a / b; return a; }
static floatx8 operator&(floatx8 a, floatx8 b) { return _mm256_and_ps(a, b); }
static floatx8& operator&=(floatx8& a, floatx8 b) { a = a & b; return a; }
static floatx8 operator|(floatx8 a, floatx8 b) { return _mm256_or_ps(a, b); }
static floatx8& operator|=(floatx8& a, floatx8 b) { a = a | b; return a; }
static floatx8 operator^(floatx8 a, floatx8 b) { return _mm256_xor_ps(a, b); }
static floatx8& operator^=(floatx8& a, floatx8 b) { a = a ^ b; return a; }

static floatx8 operator~(floatx8 a) { a = andNot(a, floatx8::allOnes()); return a; }


#if defined(SIMD_AVX_512)
static uint8 operator==(intx8 a, intx8 b) { return _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_EQ); }
static uint8 operator!=(intx8 a, intx8 b) { return _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_NE); }
static uint8 operator>(intx8 a, intx8 b) { return _mm256_cmp_epi32_mask(b, a, _MM_CMPINT_LT); }
static uint8 operator>=(intx8 a, intx8 b) { return _mm256_cmp_epi32_mask(b, a, _MM_CMPINT_LE); }
static uint8 operator<(intx8 a, intx8 b) { return _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_LT); }
static uint8 operator<=(intx8 a, intx8 b) { return _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_LE); }
#else
static intx8 operator==(intx8 a, intx8 b) { return _mm256_cmpeq_epi32(a, b); }
static intx8 operator!=(intx8 a, intx8 b) { return ~(a == b); }
static intx8 operator>(intx8 a, intx8 b) { return _mm256_cmpgt_epi32(a, b); }
static intx8 operator>=(intx8 a, intx8 b) { return (a > b) | (a == b); }
static intx8 operator<(intx8 a, intx8 b) { return _mm256_cmpgt_epi32(b, a); }
static intx8 operator<=(intx8 a, intx8 b) { return (a < b) | (a == b); }
#endif


#if defined(SIMD_AVX_512)
static uint8 operator==(floatx8 a, floatx8 b) { return _mm256_cmp_ps_mask(a, b, _CMP_EQ_OQ); }
static uint8 operator!=(floatx8 a, floatx8 b) { return _mm256_cmp_ps_mask(a, b, _CMP_NEQ_OQ); }
static uint8 operator>(floatx8 a, floatx8 b) { return _mm256_cmp_ps_mask(a, b, _CMP_GT_OQ); }
static uint8 operator>=(floatx8 a, floatx8 b) { return _mm256_cmp_ps_mask(a, b, _CMP_GE_OQ); }
static uint8 operator<(floatx8 a, floatx8 b) { return _mm256_cmp_ps_mask(a, b, _CMP_LT_OQ); }
static uint8 operator<=(floatx8 a, floatx8 b) { return _mm256_cmp_ps_mask(a, b, _CMP_LE_OQ); }
#else
static floatx8 operator==(floatx8 a, floatx8 b) { return _mm256_cmp_ps(a, b, _CMP_EQ_OQ); }
static floatx8 operator!=(floatx8 a, floatx8 b) { return _mm256_cmp_ps(a, b, _CMP_NEQ_OQ); }
static floatx8 operator>(floatx8 a, floatx8 b) { return _mm256_cmp_ps(a, b, _CMP_GT_OQ); }
static floatx8 operator>=(floatx8 a, floatx8 b) { return _mm256_cmp_ps(a, b, _CMP_GE_OQ); }
static floatx8 operator<(floatx8 a, floatx8 b) { return _mm256_cmp_ps(a, b, _CMP_LT_OQ); }
static floatx8 operator<=(floatx8 a, floatx8 b) { return _mm256_cmp_ps(a, b, _CMP_LE_OQ); }
#endif

static floatx8 operator>>(floatx8 a, int b) { return reinterpret(reinterpret(a) >> b); }
static floatx8& operator>>=(floatx8& a, int b) { a = a >> b; return a; }
static floatx8 operator<<(floatx8 a, int b) { return reinterpret(reinterpret(a) << b); }
static floatx8& operator<<=(floatx8& a, int b) { a = a << b; return a; }

static floatx8 operator-(floatx8 a) { return _mm256_xor_ps(a, reinterpret(intx8(1 << 31))); }




static float addElements(floatx8 a) { __m256 aa = _mm256_hadd_ps(a, a); aa = _mm256_hadd_ps(aa, aa); return aa.m256_f32[0] + aa.m256_f32[4]; }

static floatx8 fmadd(floatx8 a, floatx8 b, floatx8 c) { return _mm256_fmadd_ps(a, b, c); }
static floatx8 fmsub(floatx8 a, floatx8 b, floatx8 c) { return _mm256_fmsub_ps(a, b, c); }

static floatx8 sqrt(floatx8 a) { return _mm256_sqrt_ps(a); }
static floatx8 rsqrt(floatx8 a) { return _mm256_rsqrt_ps(a); }

static int toBitMask(floatx8 a) { return _mm256_movemask_ps(a); }
static int toBitMask(intx8 a) { return toBitMask(reinterpret(a)); }

#if defined(SIMD_AVX_512)
static floatx8 ifThen(uint8 cond, floatx8 ifCase, floatx8 elseCase) { return _mm256_mask_blend_ps(cond, elseCase, ifCase); }
static intx8 ifThen(uint8 cond, intx8 ifCase, intx8 elseCase) { return reinterpret(ifThen(cond, reinterpret(ifCase), reinterpret(elseCase))); }

static int toBitMask(uint8 a) { return a; }

static bool allTrue(uint8 a) { return a == (1 << 8) - 1; }
static bool allFalse(uint8 a) { return a == 0; }
static bool anyTrue(uint8 a) { return a > 0; }
static bool anyFalse(uint8 a) { return !allTrue(a); }
#else
static floatx8 ifThen(floatx8 cond, floatx8 ifCase, floatx8 elseCase) { return _mm256_blendv_ps(elseCase, ifCase, cond); }
static intx8 ifThen(intx8 cond, intx8 ifCase, intx8 elseCase) { return reinterpret(ifThen(reinterpret(cond), reinterpret(ifCase), reinterpret(elseCase))); }

static bool allTrue(floatx8 a) { return toBitMask(a) == (1 << 8) - 1; }
static bool allFalse(floatx8 a) { return toBitMask(a) == 0; }
static bool anyTrue(floatx8 a) { return toBitMask(a) > 0; }
static bool anyFalse(floatx8 a) { return !allTrue(a); }

static bool allTrue(intx8 a) { return allTrue(reinterpret(a)); }
static bool allFalse(intx8 a) { return allFalse(reinterpret(a)); }
static bool anyTrue(intx8 a) { return anyTrue(reinterpret(a)); }
static bool anyFalse(intx8 a) { return anyFalse(reinterpret(a)); }
#endif


static floatx8 abs(floatx8 a) { floatx8 result = andNot(-0.f, a); return result; }
static floatx8 floor(floatx8 a) { return _mm256_floor_ps(a); }
static floatx8 minimum(floatx8 a, floatx8 b) { return _mm256_min_ps(a, b); }
static floatx8 maximum(floatx8 a, floatx8 b) { return _mm256_max_ps(a, b); }

static floatx8 lerp(floatx8 l, floatx8 u, floatx8 t) { return fmadd(t, u - l, l); }
static floatx8 inverseLerp(floatx8 l, floatx8 u, floatx8 v) { return (v - l) / (u - l); }
static floatx8 remap(floatx8 v, floatx8 oldL, floatx8 oldU, floatx8 newL, floatx8 newU) { return lerp(newL, newU, inverseLerp(oldL, oldU, v)); }
static floatx8 clamp(floatx8 v, floatx8 l, floatx8 u) { return minimum(u, maximum(l, v)); }
static floatx8 clamp01(floatx8 v) { return clamp(v, 0.f, 1.f); }

static floatx8 signOf(floatx8 f) { floatx8 z = floatx8::zero(); return ifThen(f < z, floatx8(-1), ifThen(f == z, z, floatx8(1))); }
static floatx8 signbit(floatx8 f) { return (f & -0.f) >> 31; }

static floatx8 cos(floatx8 x) { return cosInternal(x); }
static floatx8 sin(floatx8 x) { return sinInternal(x); }
static floatx8 exp2(floatx8 x) { return exp2Internal<floatx8, intx8>(x); }
static floatx8 log2(floatx8 x) { return log2Internal<floatx8, intx8>(x); }
static floatx8 pow(floatx8 x, floatx8 y) { return powInternal<floatx8, intx8>(x, y); }
static floatx8 exp(floatx8 x) { return expInternal<floatx8, intx8>(x); }
static floatx8 tanh(floatx8 x) { return tanhInternal(x); }
static floatx8 atan(floatx8 x) { return atanInternal<floatx8, intx8>(x); }
static floatx8 atan2(floatx8 y, floatx8 x) { return atan2Internal<floatx8, intx8>(y, x); }
static floatx8 acos(floatx8 x) { return acosInternal(x); }

static floatx8 concat(floatx4 a, floatx4 b)
{
	return _mm256_insertf128_ps(_mm256_castps128_ps256(a), b, 1);
}

static intx8 concat(intx4 a, intx4 b)
{
	return _mm256_inserti128_si256(_mm256_castsi128_si256(a), b, 1);
}

static floatx8 concatLow(floatx8 a, floatx8 b)
{
	return _mm256_permute2f128_ps(a, b, 0 | (0 << 4));
}

static intx8 concatLow(intx8 a, intx8 b)
{
	return _mm256_permute2x128_si256(a, b, 0 | (0 << 4));
}

static intx8 fillWithFirstLane(intx8 a)
{
	intx8 first = _mm256_shuffle_epi32(a, _MM_SHUFFLE(0, 0, 0, 0));
	first = concatLow(first, first);
	return first;
}

static void transpose32(floatx8& out0, floatx8& out1, floatx8& out2, floatx8& out3)
{
	floatx8 t0 = _mm256_unpacklo_ps(out0, out1);
	floatx8 t1 = _mm256_unpacklo_ps(out2, out3);
	floatx8 t2 = _mm256_unpackhi_ps(out0, out1);
	floatx8 t3 = _mm256_unpackhi_ps(out2, out3);
	out0 = _mm256_shuffle_ps(t0, t1, _MM_SHUFFLE(1, 0, 1, 0));
	out1 = _mm256_shuffle_ps(t0, t1, _MM_SHUFFLE(3, 2, 3, 2));
	out2 = _mm256_shuffle_ps(t2, t3, _MM_SHUFFLE(1, 0, 1, 0));
	out3 = _mm256_shuffle_ps(t2, t3, _MM_SHUFFLE(3, 2, 3, 2));
}

static void transpose(floatx8& out0, floatx8& out1, floatx8& out2, floatx8& out3, floatx8& out4, floatx8& out5, floatx8& out6, floatx8& out7)
{
	floatx8 tmp0 = reinterpret(_mm256_permute2x128_si256(reinterpret(out0), reinterpret(out4), 0 | (2 << 4)));
	floatx8 tmp1 = reinterpret(_mm256_permute2x128_si256(reinterpret(out1), reinterpret(out5), 0 | (2 << 4)));
	floatx8 tmp2 = reinterpret(_mm256_permute2x128_si256(reinterpret(out2), reinterpret(out6), 0 | (2 << 4)));
	floatx8 tmp3 = reinterpret(_mm256_permute2x128_si256(reinterpret(out3), reinterpret(out7), 0 | (2 << 4)));
	floatx8 tmp4 = reinterpret(_mm256_permute2x128_si256(reinterpret(out0), reinterpret(out4), 1 | (3 << 4)));
	floatx8 tmp5 = reinterpret(_mm256_permute2x128_si256(reinterpret(out1), reinterpret(out5), 1 | (3 << 4)));
	floatx8 tmp6 = reinterpret(_mm256_permute2x128_si256(reinterpret(out2), reinterpret(out6), 1 | (3 << 4)));
	floatx8 tmp7 = reinterpret(_mm256_permute2x128_si256(reinterpret(out3), reinterpret(out7), 1 | (3 << 4)));

	out0 = tmp0;
	out1 = tmp1;
	out2 = tmp2;
	out3 = tmp3;
	out4 = tmp4;
	out5 = tmp5;
	out6 = tmp6;
	out7 = tmp7;

	transpose32(out0, out1, out2, out3);
	transpose32(out4, out5, out6, out7);
}

static void load4(const float* baseAddress, const uint16* indices, uint32 stride,
	floatx8& out0, floatx8& out1, floatx8& out2, floatx8& out3)
{
	const uint32 strideInFloats = stride / sizeof(float);

	floatx4 tmp0(baseAddress + strideInFloats * indices[0]);
	floatx4 tmp1(baseAddress + strideInFloats * indices[1]);
	floatx4 tmp2(baseAddress + strideInFloats * indices[2]);
	floatx4 tmp3(baseAddress + strideInFloats * indices[3]);
	floatx4 tmp4(baseAddress + strideInFloats * indices[4]);
	floatx4 tmp5(baseAddress + strideInFloats * indices[5]);
	floatx4 tmp6(baseAddress + strideInFloats * indices[6]);
	floatx4 tmp7(baseAddress + strideInFloats * indices[7]);

	out0 = concat(tmp0, tmp4);
	out1 = concat(tmp1, tmp5);
	out2 = concat(tmp2, tmp6);
	out3 = concat(tmp3, tmp7);

	transpose32(out0, out1, out2, out3);
}

static void load8(const float* baseAddress, const uint16* indices, uint32 stride,
	floatx8& out0, floatx8& out1, floatx8& out2, floatx8& out3, floatx8& out4, floatx8& out5, floatx8& out6, floatx8& out7)
{
	const uint32 strideInFloats = stride / sizeof(float);

	out0 = floatx8(baseAddress + strideInFloats * indices[0]);
	out1 = floatx8(baseAddress + strideInFloats * indices[1]);
	out2 = floatx8(baseAddress + strideInFloats * indices[2]);
	out3 = floatx8(baseAddress + strideInFloats * indices[3]);
	out4 = floatx8(baseAddress + strideInFloats * indices[4]);
	out5 = floatx8(baseAddress + strideInFloats * indices[5]);
	out6 = floatx8(baseAddress + strideInFloats * indices[6]);
	out7 = floatx8(baseAddress + strideInFloats * indices[7]);

	transpose(out0, out1, out2, out3, out4, out5, out6, out7);
}

static void store8(float* baseAddress, const uint16* indices, uint32 stride,
	floatx8 in0, floatx8 in1, floatx8 in2, floatx8 in3, floatx8 in4, floatx8 in5, floatx8 in6, floatx8 in7)
{
	const uint32 strideInFloats = stride / sizeof(float);

	transpose(in0, in1, in2, in3, in4, in5, in6, in7);

	in0.store(baseAddress + strideInFloats * indices[0]);
	in1.store(baseAddress + strideInFloats * indices[1]);
	in2.store(baseAddress + strideInFloats * indices[2]);
	in3.store(baseAddress + strideInFloats * indices[3]);
	in4.store(baseAddress + strideInFloats * indices[4]);
	in5.store(baseAddress + strideInFloats * indices[5]);
	in6.store(baseAddress + strideInFloats * indices[6]);
	in7.store(baseAddress + strideInFloats * indices[7]);
}


#endif

#if defined(SIMD_AVX_512)

struct floatx16
{
	__m512 f;

	floatx16() {}
	floatx16(float f_) { f = _mm512_set1_ps(f_); }
	floatx16(__m512 f_) { f = f_; }
	floatx16(float a, float b, float c, float d, float e, float f, float g, float h, float i, float j, float k, float l, float m, float n, float o, float p) { this->f = _mm512_setr_ps(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p); }
	floatx16(const float* f_) { f = _mm512_loadu_ps(f_); }

	floatx16(const float* baseAddress, __m512i indices) { f = _mm512_i32gather_ps(indices, baseAddress, 4); }
	floatx16(const float* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l, int m, int n, int o, int p) : floatx16(baseAddress, _mm512_setr_epi32(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)) {}

	operator __m512() { return f; }

	void store(float* f_) const { _mm512_storeu_ps(f_, f); }

	void scatter(float* baseAddress, __m512i indices) const { _mm512_i32scatter_ps(baseAddress, indices, f, 4); }
	void scatter(float* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l, int m, int n, int o, int p) const { scatter(baseAddress, _mm512_setr_epi32(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)); }
};

struct intx16
{
	__m512i i;

	intx16() {}
	intx16(int i_) { i = _mm512_set1_epi32(i_); }
	intx16(__m512i i_) { i = i_; }
	intx16(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l, int m, int n, int o, int p) { this->i = _mm512_setr_epi32(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p); }
	intx16(const int* i_) { i = _mm512_loadu_epi32(i_); }

	intx16(const int* baseAddress, __m512i indices) { i = _mm512_i32gather_epi32(indices, baseAddress, 4); }
	intx16(const int* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l, int m, int n, int o, int p) : intx16(baseAddress, _mm512_setr_epi32(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)) {}

	operator __m512i() { return i; }

	void store(int* i_) const { _mm512_storeu_epi32(i_, i); }
};

static floatx16 truex16() { const float nnan = (const float&)0xFFFFFFFF; return nnan; }
static floatx16 zerox16() { return _mm512_setzero_ps(); }

static floatx16 convert(intx16 i) { return _mm512_cvtepi32_ps(i); }
static intx16 convert(floatx16 f) { return _mm512_cvtps_epi32(f); }
static floatx16 reinterpret(intx16 i) { return _mm512_castsi512_ps(i); }
static intx16 reinterpret(floatx16 f) { return _mm512_castps_si512(f); }


// Int operators.
static intx16 andNot(intx16 a, intx16 b) { return _mm512_andnot_si512(a, b); }

static intx16 operator+(intx16 a, intx16 b) { return _mm512_add_epi32(a, b); }
static intx16& operator+=(intx16& a, intx16 b) { a = a + b; return a; }
static intx16 operator-(intx16 a, intx16 b) { return _mm512_sub_epi32(a, b); }
static intx16& operator-=(intx16& a, intx16 b) { a = a - b; return a; }
static intx16 operator*(intx16 a, intx16 b) { return _mm512_mul_epi32(a, b); }
static intx16& operator*=(intx16& a, intx16 b) { a = a * b; return a; }
static intx16 operator/(intx16 a, intx16 b) { return _mm512_div_epi32(a, b); }
static intx16& operator/=(intx16& a, intx16 b) { a = a / b; return a; }
static intx16 operator&(intx16 a, intx16 b) { return _mm512_and_epi32(a, b); }
static intx16& operator&=(intx16& a, intx16 b) { a = a & b; return a; }
static intx16 operator|(intx16 a, intx16 b) { return _mm512_or_epi32(a, b); }
static intx16& operator|=(intx16& a, intx16 b) { a = a | b; return a; }
static intx16 operator^(intx16 a, intx16 b) { return _mm512_xor_epi32(a, b); }
static intx16& operator^=(intx16& a, intx16 b) { a = a ^ b; return a; }

static intx16 operator~(intx16 a) { a = andNot(a, reinterpret(truex16())); return a; }

static uint16 operator==(intx16 a, intx16 b) { return _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_EQ); }
static uint16 operator!=(intx16 a, intx16 b) { return _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_NE); }
static uint16 operator>(intx16 a, intx16 b) { return _mm512_cmp_epi32_mask(b, a, _MM_CMPINT_LT); }
static uint16 operator>=(intx16 a, intx16 b) { return _mm512_cmp_epi32_mask(b, a, _MM_CMPINT_LE); }
static uint16 operator<(intx16 a, intx16 b) { return _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_LT); }
static uint16 operator<=(intx16 a, intx16 b) { return _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_LE); }


static intx16 operator>>(intx16 a, int b) { return _mm512_srli_epi32(a, b); }
static intx16& operator>>=(intx16& a, int b) { a = a >> b; return a; }
static intx16 operator<<(intx16 a, int b) { return _mm512_slli_epi32(a, b); }
static intx16& operator<<=(intx16& a, int b) { a = a << b; return a; }

static intx16 operator-(intx16 a) { return _mm512_sub_epi32(_mm512_setzero_si512(), a); }



// Float operators.
static floatx16 andNot(floatx16 a, floatx16 b) { return _mm512_andnot_ps(a, b); }

static floatx16 operator+(floatx16 a, floatx16 b) { return _mm512_add_ps(a, b); }
static floatx16& operator+=(floatx16& a, floatx16 b) { a = a + b; return a; }
static floatx16 operator-(floatx16 a, floatx16 b) { return _mm512_sub_ps(a, b); }
static floatx16& operator-=(floatx16& a, floatx16 b) { a = a - b; return a; }
static floatx16 operator*(floatx16 a, floatx16 b) { return _mm512_mul_ps(a, b); }
static floatx16& operator*=(floatx16& a, floatx16 b) { a = a * b; return a; }
static floatx16 operator/(floatx16 a, floatx16 b) { return _mm512_div_ps(a, b); }
static floatx16& operator/=(floatx16& a, floatx16 b) { a = a / b; return a; }
static floatx16 operator&(floatx16 a, floatx16 b) { return _mm512_and_ps(a, b); }
static floatx16& operator&=(floatx16& a, floatx16 b) { a = a & b; return a; }
static floatx16 operator|(floatx16 a, floatx16 b) { return _mm512_or_ps(a, b); }
static floatx16& operator|=(floatx16& a, floatx16 b) { a = a | b; return a; }
static floatx16 operator^(floatx16 a, floatx16 b) { return _mm512_xor_ps(a, b); }
static floatx16& operator^=(floatx16& a, floatx16 b) { a = a ^ b; return a; }

static floatx16 operator~(floatx16 a) { a = andNot(a, truex16()); return a; }

static uint16 operator==(floatx16 a, floatx16 b) { return _mm512_cmp_ps_mask(a, b, _CMP_EQ_OQ); }
static uint16 operator!=(floatx16 a, floatx16 b) { return _mm512_cmp_ps_mask(a, b, _CMP_NEQ_OQ); }
static uint16 operator>(floatx16 a, floatx16 b) { return _mm512_cmp_ps_mask(a, b, _CMP_GT_OQ); }
static uint16 operator>=(floatx16 a, floatx16 b) { return _mm512_cmp_ps_mask(a, b, _CMP_GE_OQ); }
static uint16 operator<(floatx16 a, floatx16 b) { return _mm512_cmp_ps_mask(a, b, _CMP_LT_OQ); }
static uint16 operator<=(floatx16 a, floatx16 b) { return _mm512_cmp_ps_mask(a, b, _CMP_LE_OQ); }


static floatx16 operator>>(floatx16 a, int b) { return reinterpret(reinterpret(a) >> b); }
static floatx16& operator>>=(floatx16& a, int b) { a = a >> b; return a; }
static floatx16 operator<<(floatx16 a, int b) { return reinterpret(reinterpret(a) << b); }
static floatx16& operator<<=(floatx16& a, int b) { a = a << b; return a; }

static floatx16 operator-(floatx16 a) { return _mm512_xor_ps(a, reinterpret(intx16(1 << 31))); }




static float addElements(floatx16 a) { return _mm512_reduce_add_ps(a); }

static floatx16 fmadd(floatx16 a, floatx16 b, floatx16 c) { return _mm512_fmadd_ps(a, b, c); }
static floatx16 fmsub(floatx16 a, floatx16 b, floatx16 c) { return _mm512_fmsub_ps(a, b, c); }

static floatx16 sqrt(floatx16 a) { return _mm512_sqrt_ps(a); }
static floatx16 rsqrt(floatx16 a) { return 1.f / _mm512_sqrt_ps(a); }

static floatx16 ifThen(uint16 cond, floatx16 ifCase, floatx16 elseCase) { return _mm512_mask_blend_ps(cond, elseCase, ifCase); }
static intx16 ifThen(uint16 cond, intx16 ifCase, intx16 elseCase) { return reinterpret(ifThen(cond, reinterpret(ifCase), reinterpret(elseCase))); }

static bool allTrue(uint16 a) { return a == (1 << 16) - 1; }
static bool allFalse(uint16 a) { return a == 0; }
static bool anyTrue(uint16 a) { return a > 0; }
static bool anyFalse(uint16 a) { return !allTrue(a); }


static floatx16 abs(floatx16 a) { floatx16 result = andNot(-0.f, a); return result; }
static floatx16 floor(floatx16 a) { return _mm512_floor_ps(a); }
static floatx16 minimum(floatx16 a, floatx16 b) { return _mm512_min_ps(a, b); }
static floatx16 maximum(floatx16 a, floatx16 b) { return _mm512_max_ps(a, b); }

static floatx16 lerp(floatx16 l, floatx16 u, floatx16 t) { return fmadd(t, u - l, l); }
static floatx16 inverseLerp(floatx16 l, floatx16 u, floatx16 v) { return (v - l) / (u - l); }
static floatx16 remap(floatx16 v, floatx16 oldL, floatx16 oldU, floatx16 newL, floatx16 newU) { return lerp(newL, newU, inverseLerp(oldL, oldU, v)); }
static floatx16 clamp(floatx16 v, floatx16 l, floatx16 u) { return minimum(u, maximum(l, v)); }
static floatx16 clamp01(floatx16 v) { return clamp(v, 0.f, 1.f); }

static floatx16 signOf(floatx16 f) { return ifThen(f < 0.f, floatx16(-1), ifThen(f == 0.f, zerox16(), floatx16(1))); }
static floatx16 signbit(floatx16 f) { return (f & -0.f) >> 31; }

static floatx16 cos(floatx16 x) { return cosInternal(x); }
static floatx16 sin(floatx16 x) { return sinInternal(x); }
static floatx16 exp2(floatx16 x) { return exp2Internal<floatx16, intx16>(x); }
static floatx16 log2(floatx16 x) { return log2Internal<floatx16, intx16>(x); }
static floatx16 pow(floatx16 x, floatx16 y) { return powInternal<floatx16, intx16>(x, y); }
static floatx16 exp(floatx16 x) { return expInternal<floatx16, intx16>(x); }
static floatx16 tanh(floatx16 x) { return tanhInternal(x); }
static floatx16 atan(floatx16 x) { return atanInternal<floatx16, intx16>(x); }
static floatx16 atan2(floatx16 y, floatx16 x) { return atan2Internal<floatx16, intx16>(y, x); }
static floatx16 acos(floatx16 x) { return acosInternal(x); }


#endif


#if defined(SIMD_AVX_512)
typedef floatx16 floatx;
typedef intx16 intx;
#define SIMD_WIDTH 16
#elif defined(SIMD_AVX_2)
typedef floatx8 floatx;
typedef intx8 intx;
#define SIMD_WIDTH 8
#else
typedef floatx4 floatx;
typedef intx4 intx;
typedef floatx4 cmpx;
#define SIMD_WIDTH 4
#endif








