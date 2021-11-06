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

struct floatw4
{
	__m128 f;

	floatw4() {}
	floatw4(float f_) { f = _mm_set1_ps(f_); }
	floatw4(__m128 f_) { f = f_; }
	floatw4(float a, float b, float c, float d) { f = _mm_setr_ps(a, b, c, d); }
	floatw4(const float* f_) { f = _mm_loadu_ps(f_); }

#if defined(SIMD_AVX_2)
	floatw4(const float* baseAddress, __m128i indices) { f = _mm_i32gather_ps(baseAddress, indices, 4); }
	floatw4(const float* baseAddress, int a, int b, int c, int d) : floatw4(baseAddress, _mm_setr_epi32(a, b, c, d)) {}
#else
	floatw4(const float* baseAddress, int a, int b, int c, int d) { f = _mm_setr_ps(baseAddress[a], baseAddress[b], baseAddress[c], baseAddress[d]);  }
	floatw4(const float* baseAddress, __m128i indices) : floatw4(baseAddress, indices.m128i_i32[0], indices.m128i_i32[1], indices.m128i_i32[2], indices.m128i_i32[3]) {}
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

	static floatw4 allOnes() { const float nnan = (const float&)0xFFFFFFFF; return nnan; }
	static floatw4 zero() { return _mm_setzero_ps(); }
};

struct intw4
{
	__m128i i;

	intw4() {}
	intw4(int i_) { i = _mm_set1_epi32(i_); }
	intw4(__m128i i_) { i = i_; }
	intw4(int a, int b, int c, int d) { i = _mm_setr_epi32(a, b, c, d); }
	intw4(int* i_) { i = _mm_loadu_si128((const __m128i*)i_); }

#if defined(SIMD_AVX_2)
	intw4(const int* baseAddress, __m128i indices) { i = _mm_i32gather_epi32(baseAddress, indices, 4); }
	intw4(const int* baseAddress, int a, int b, int c, int d) : intw4(baseAddress, _mm_setr_epi32(a, b, c, d)) {}
#else
	intw4(const int* baseAddress, int a, int b, int c, int d) { i = _mm_setr_epi32(baseAddress[a], baseAddress[b], baseAddress[c], baseAddress[d]); }
	intw4(const int* baseAddress, __m128i indices) : intw4(baseAddress, indices.m128i_i32[0], indices.m128i_i32[1], indices.m128i_i32[2], indices.m128i_i32[3]) {}
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

	static intw4 allOnes() { return UINT32_MAX; }
	static intw4 zero() { return _mm_setzero_si128(); }
};

static floatw4 convert(intw4 i) { return _mm_cvtepi32_ps(i); }
static intw4 convert(floatw4 f) { return _mm_cvtps_epi32(f); }
static floatw4 reinterpret(intw4 i) { return _mm_castsi128_ps(i); }
static intw4 reinterpret(floatw4 f) { return _mm_castps_si128(f); }


// Int operators.
static intw4 andNot(intw4 a, intw4 b) { return _mm_andnot_si128(a, b); }

static intw4 operator+(intw4 a, intw4 b) { return _mm_add_epi32(a, b); }
static intw4& operator+=(intw4& a, intw4 b) { a = a + b; return a; }
static intw4 operator-(intw4 a, intw4 b) { return _mm_sub_epi32(a, b); }
static intw4& operator-=(intw4& a, intw4 b) { a = a - b; return a; }
static intw4 operator*(intw4 a, intw4 b) { return _mm_mul_epi32(a, b); }
static intw4& operator*=(intw4& a, intw4 b) { a = a * b; return a; }
static intw4 operator/(intw4 a, intw4 b) { return _mm_div_epi32(a, b); }
static intw4& operator/=(intw4& a, intw4 b) { a = a / b; return a; }
static intw4 operator&(intw4 a, intw4 b) { return _mm_and_si128(a, b); }
static intw4& operator&=(intw4& a, intw4 b) { a = a & b; return a; }
static intw4 operator|(intw4 a, intw4 b) { return _mm_or_si128(a, b); }
static intw4& operator|=(intw4& a, intw4 b) { a = a | b; return a; }
static intw4 operator^(intw4 a, intw4 b) { return _mm_xor_si128(a, b); }
static intw4& operator^=(intw4& a, intw4 b) { a = a ^ b; return a; }

static intw4 operator~(intw4 a) { a = andNot(a, intw4::allOnes()); return a; }

static intw4 operator>>(intw4 a, int b) { return _mm_srli_epi32(a, b); }
static intw4& operator>>=(intw4& a, int b) { a = a >> b; return a; }
static intw4 operator<<(intw4 a, int b) { return _mm_slli_epi32(a, b); }
static intw4& operator<<=(intw4& a, int b) { a = a << b; return a; }

static intw4 operator-(intw4 a) { return _mm_sub_epi32(intw4::zero(), a); }



// Float operators.
static floatw4 andNot(floatw4 a, floatw4 b) { return _mm_andnot_ps(a, b); }

static floatw4 operator+(floatw4 a, floatw4 b) { return _mm_add_ps(a, b); }
static floatw4& operator+=(floatw4& a, floatw4 b) { a = a + b; return a; }
static floatw4 operator-(floatw4 a, floatw4 b) { return _mm_sub_ps(a, b); }
static floatw4& operator-=(floatw4& a, floatw4 b) { a = a - b; return a; }
static floatw4 operator*(floatw4 a, floatw4 b) { return _mm_mul_ps(a, b); }
static floatw4& operator*=(floatw4& a, floatw4 b) { a = a * b; return a; }
static floatw4 operator/(floatw4 a, floatw4 b) { return _mm_div_ps(a, b); }
static floatw4& operator/=(floatw4& a, floatw4 b) { a = a / b; return a; }
static floatw4 operator&(floatw4 a, floatw4 b) { return _mm_and_ps(a, b); }
static floatw4& operator&=(floatw4& a, floatw4 b) { a = a & b; return a; }
static floatw4 operator|(floatw4 a, floatw4 b) { return _mm_or_ps(a, b); }
static floatw4& operator|=(floatw4& a, floatw4 b) { a = a | b; return a; }
static floatw4 operator^(floatw4 a, floatw4 b) { return _mm_xor_ps(a, b); }
static floatw4& operator^=(floatw4& a, floatw4 b) { a = a ^ b; return a; }

static floatw4 operator~(floatw4 a) { a = andNot(a, floatw4::allOnes()); return a; }


static intw4 operator==(intw4 a, intw4 b) { return _mm_cmpeq_epi32(a, b); }
static intw4 operator!=(intw4 a, intw4 b) { return ~(a == b); }
static intw4 operator>(intw4 a, intw4 b) { return _mm_cmpgt_epi32(a, b); }
static intw4 operator>=(intw4 a, intw4 b) { return (a > b) | (a == b); }
static intw4 operator<(intw4 a, intw4 b) { return _mm_cmplt_epi32(a, b); }
static intw4 operator<=(intw4 a, intw4 b) { return (a < b) | (a == b); }

static floatw4 operator==(floatw4 a, floatw4 b) { return _mm_cmpeq_ps(a, b); }
static floatw4 operator!=(floatw4 a, floatw4 b) { return _mm_cmpneq_ps(a, b); }
static floatw4 operator>(floatw4 a, floatw4 b) { return _mm_cmpgt_ps(a, b); }
static floatw4 operator>=(floatw4 a, floatw4 b) { return _mm_cmpge_ps(a, b); }
static floatw4 operator<(floatw4 a, floatw4 b) { return _mm_cmplt_ps(a, b); }
static floatw4 operator<=(floatw4 a, floatw4 b) { return _mm_cmple_ps(a, b); }

static floatw4 operator>>(floatw4 a, int b) { return reinterpret(reinterpret(a) >> b); }
static floatw4& operator>>=(floatw4& a, int b) { a = a >> b; return a; }
static floatw4 operator<<(floatw4 a, int b) { return reinterpret(reinterpret(a) << b); }
static floatw4& operator<<=(floatw4& a, int b) { a = a << b; return a; }

static floatw4 operator-(floatw4 a) { return _mm_xor_ps(a, reinterpret(intw4(1 << 31))); }




static float addElements(floatw4 a) { __m128 aa = _mm_hadd_ps(a, a); aa = _mm_hadd_ps(aa, aa); return aa.m128_f32[0]; }

static floatw4 fmadd(floatw4 a, floatw4 b, floatw4 c) { return _mm_fmadd_ps(a, b, c); }
static floatw4 fmsub(floatw4 a, floatw4 b, floatw4 c) { return _mm_fmsub_ps(a, b, c); }

static floatw4 sqrt(floatw4 a) { return _mm_sqrt_ps(a); }
static floatw4 rsqrt(floatw4 a) { return _mm_rsqrt_ps(a); }

static floatw4 ifThen(floatw4 cond, floatw4 ifCase, floatw4 elseCase) { return _mm_blendv_ps(elseCase, ifCase, cond); }
static intw4 ifThen(intw4 cond, intw4 ifCase, intw4 elseCase) { return reinterpret(ifThen(reinterpret(cond), reinterpret(ifCase), reinterpret(elseCase))); }

static int toBitMask(floatw4 a) { return _mm_movemask_ps(a); }
static int toBitMask(intw4 a) { return toBitMask(reinterpret(a)); }

static bool allTrue(floatw4 a) { return toBitMask(a) == (1 << 4) - 1; }
static bool allFalse(floatw4 a) { return toBitMask(a) == 0; }
static bool anyTrue(floatw4 a) { return toBitMask(a) > 0; }
static bool anyFalse(floatw4 a) { return !allTrue(a); }

static bool allTrue(intw4 a) { return allTrue(reinterpret(a)); }
static bool allFalse(intw4 a) { return allFalse(reinterpret(a)); }
static bool anyTrue(intw4 a) { return anyTrue(reinterpret(a)); }
static bool anyFalse(intw4 a) { return anyFalse(reinterpret(a)); }

static floatw4 abs(floatw4 a) { floatw4 result = andNot(-0.f, a); return result; }
static floatw4 floor(floatw4 a) { return _mm_floor_ps(a); }
static floatw4 round(floatw4 a) { return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC); }
static floatw4 minimum(floatw4 a, floatw4 b) { return _mm_min_ps(a, b); }
static floatw4 maximum(floatw4 a, floatw4 b) { return _mm_max_ps(a, b); }

static floatw4 lerp(floatw4 l, floatw4 u, floatw4 t) { return fmadd(t, u - l, l); }
static floatw4 inverseLerp(floatw4 l, floatw4 u, floatw4 v) { return (v - l) / (u - l); }
static floatw4 remap(floatw4 v, floatw4 oldL, floatw4 oldU, floatw4 newL, floatw4 newU) { return lerp(newL, newU, inverseLerp(oldL, oldU, v)); }
static floatw4 clamp(floatw4 v, floatw4 l, floatw4 u) { return minimum(u, maximum(l, v)); }
static floatw4 clamp01(floatw4 v) { return clamp(v, 0.f, 1.f); }

static floatw4 signOf(floatw4 f) { floatw4 z = floatw4::zero(); return ifThen(f < z, floatw4(-1), ifThen(f == z, z, floatw4(1))); }
static floatw4 signbit(floatw4 f) { return (f & -0.f) >> 31; }

static floatw4 cos(floatw4 x) { return cosInternal(x); }
static floatw4 sin(floatw4 x) { return sinInternal(x); }
static floatw4 exp2(floatw4 x) { return exp2Internal<floatw4, intw4>(x); }
static floatw4 log2(floatw4 x) { return log2Internal<floatw4, intw4>(x); }
static floatw4 pow(floatw4 x, floatw4 y) { return powInternal<floatw4, intw4>(x, y); }
static floatw4 exp(floatw4 x) { return expInternal<floatw4, intw4>(x); }
static floatw4 tanh(floatw4 x) { return tanhInternal(x); }
static floatw4 atan(floatw4 x) { return atanInternal<floatw4, intw4>(x); }
static floatw4 atan2(floatw4 y, floatw4 x) { return atan2Internal<floatw4, intw4>(y, x); }
static floatw4 acos(floatw4 x) { return acosInternal(x); }

static intw4 fillWithFirstLane(intw4 a)
{
	intw4 first = _mm_shuffle_epi32(a, _MM_SHUFFLE(0, 0, 0, 0));
	return first;
}

static void transpose(floatw4& out0, floatw4& out1, floatw4& out2, floatw4& out3)
{
	_MM_TRANSPOSE4_PS(out0.f, out1.f, out2.f, out3.f);
}

static void load4(const float* baseAddress, const uint16* indices, uint32 stride,
	floatw4& out0, floatw4& out1, floatw4& out2, floatw4& out3)
{
	const uint32 strideInFloats = stride / sizeof(float);

	out0 = floatw4(baseAddress + strideInFloats * indices[0]);
	out1 = floatw4(baseAddress + strideInFloats * indices[1]);
	out2 = floatw4(baseAddress + strideInFloats * indices[2]);
	out3 = floatw4(baseAddress + strideInFloats * indices[3]);

	transpose(out0, out1, out2, out3);
}

static void store4(float* baseAddress, const uint16* indices, uint32 stride,
	floatw4 in0, floatw4 in1, floatw4 in2, floatw4 in3)
{
	const uint32 strideInFloats = stride / sizeof(float);

	transpose(in0, in1, in2, in3);

	in0.store(baseAddress + strideInFloats * indices[0]);
	in1.store(baseAddress + strideInFloats * indices[1]);
	in2.store(baseAddress + strideInFloats * indices[2]);
	in3.store(baseAddress + strideInFloats * indices[3]);
}

static void load8(const float* baseAddress, const uint16* indices, uint32 stride,
	floatw4& out0, floatw4& out1, floatw4& out2, floatw4& out3, floatw4& out4, floatw4& out5, floatw4& out6, floatw4& out7)
{
	const uint32 strideInFloats = stride / sizeof(float);

	out0 = floatw4(baseAddress + strideInFloats * indices[0]);
	out1 = floatw4(baseAddress + strideInFloats * indices[1]);
	out2 = floatw4(baseAddress + strideInFloats * indices[2]);
	out3 = floatw4(baseAddress + strideInFloats * indices[3]);
	out4 = floatw4(baseAddress + strideInFloats * indices[0] + 4);
	out5 = floatw4(baseAddress + strideInFloats * indices[1] + 4);
	out6 = floatw4(baseAddress + strideInFloats * indices[2] + 4);
	out7 = floatw4(baseAddress + strideInFloats * indices[3] + 4);

	transpose(out0, out1, out2, out3);
	transpose(out4, out5, out6, out7);
}

static void store8(float* baseAddress, const uint16* indices, uint32 stride,
	floatw4 in0, floatw4 in1, floatw4 in2, floatw4 in3, floatw4 in4, floatw4 in5, floatw4 in6, floatw4 in7)
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

struct floatw8
{
	__m256 f;

	floatw8() {}
	floatw8(float f_) { f = _mm256_set1_ps(f_); }
	floatw8(__m256 f_) { f = f_; }
	floatw8(float a, float b, float c, float d, float e, float f, float g, float h) { this->f = _mm256_setr_ps(a, b, c, d, e, f, g, h); }
	floatw8(const float* f_) { f = _mm256_loadu_ps(f_); }

	floatw8(const float* baseAddress, __m256i indices) { f = _mm256_i32gather_ps(baseAddress, indices, 4); }
	floatw8(const float* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h) : floatw8(baseAddress, _mm256_setr_epi32(a, b, c, d, e, f, g, h)) {}

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

	static floatw8 allOnes() { const float nnan = (const float&)0xFFFFFFFF; return nnan; }
	static floatw8 zero() { return _mm256_setzero_ps(); }
};

struct intw8
{
	__m256i i;

	intw8() {}
	intw8(int i_) { i = _mm256_set1_epi32(i_); }
	intw8(__m256i i_) { i = i_; }
	intw8(int a, int b, int c, int d, int e, int f, int g, int h) { this->i = _mm256_setr_epi32(a, b, c, d, e, f, g, h); }
	intw8(const int* i_) { i = _mm256_loadu_epi32(i_); }

	intw8(const int* baseAddress, __m256i indices) { i = _mm256_i32gather_epi32(baseAddress, indices, 4); }
	intw8(const int* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h) : intw8(baseAddress, _mm256_setr_epi32(a, b, c, d, e, f, g, h)) {}

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

	static intw8 allOnes() { return UINT32_MAX; }
	static intw8 zero() { return _mm256_setzero_si256(); }
};

static floatw8 convert(intw8 i) { return _mm256_cvtepi32_ps(i); }
static intw8 convert(floatw8 f) { return _mm256_cvtps_epi32(f); }
static floatw8 reinterpret(intw8 i) { return _mm256_castsi256_ps(i); }
static intw8 reinterpret(floatw8 f) { return _mm256_castps_si256(f); }


// Int operators.
static intw8 andNot(intw8 a, intw8 b) { return _mm256_andnot_si256(a, b); }

static intw8 operator+(intw8 a, intw8 b) { return _mm256_add_epi32(a, b); }
static intw8& operator+=(intw8& a, intw8 b) { a = a + b; return a; }
static intw8 operator-(intw8 a, intw8 b) { return _mm256_sub_epi32(a, b); }
static intw8& operator-=(intw8& a, intw8 b) { a = a - b; return a; }
static intw8 operator*(intw8 a, intw8 b) { return _mm256_mul_epi32(a, b); }
static intw8& operator*=(intw8& a, intw8 b) { a = a * b; return a; }
static intw8 operator/(intw8 a, intw8 b) { return _mm256_div_epi32(a, b); }
static intw8& operator/=(intw8& a, intw8 b) { a = a / b; return a; }
static intw8 operator&(intw8 a, intw8 b) { return _mm256_and_si256(a, b); }
static intw8& operator&=(intw8& a, intw8 b) { a = a & b; return a; }
static intw8 operator|(intw8 a, intw8 b) { return _mm256_or_si256(a, b); }
static intw8& operator|=(intw8& a, intw8 b) { a = a | b; return a; }
static intw8 operator^(intw8 a, intw8 b) { return _mm256_xor_si256(a, b); }
static intw8& operator^=(intw8& a, intw8 b) { a = a ^ b; return a; }

static intw8 operator~(intw8 a) { a = andNot(a, intw8::allOnes()); return a; }

static intw8 operator>>(intw8 a, int b) { return _mm256_srli_epi32(a, b); }
static intw8& operator>>=(intw8& a, int b) { a = a >> b; return a; }
static intw8 operator<<(intw8 a, int b) { return _mm256_slli_epi32(a, b); }
static intw8& operator<<=(intw8& a, int b) { a = a << b; return a; }

static intw8 operator-(intw8 a) { return _mm256_sub_epi32(intw8::zero(), a); }



// Float operators.
static floatw8 andNot(floatw8 a, floatw8 b) { return _mm256_andnot_ps(a, b); }

static floatw8 operator+(floatw8 a, floatw8 b) { return _mm256_add_ps(a, b); }
static floatw8& operator+=(floatw8& a, floatw8 b) { a = a + b; return a; }
static floatw8 operator-(floatw8 a, floatw8 b) { return _mm256_sub_ps(a, b); }
static floatw8& operator-=(floatw8& a, floatw8 b) { a = a - b; return a; }
static floatw8 operator*(floatw8 a, floatw8 b) { return _mm256_mul_ps(a, b); }
static floatw8& operator*=(floatw8& a, floatw8 b) { a = a * b; return a; }
static floatw8 operator/(floatw8 a, floatw8 b) { return _mm256_div_ps(a, b); }
static floatw8& operator/=(floatw8& a, floatw8 b) { a = a / b; return a; }
static floatw8 operator&(floatw8 a, floatw8 b) { return _mm256_and_ps(a, b); }
static floatw8& operator&=(floatw8& a, floatw8 b) { a = a & b; return a; }
static floatw8 operator|(floatw8 a, floatw8 b) { return _mm256_or_ps(a, b); }
static floatw8& operator|=(floatw8& a, floatw8 b) { a = a | b; return a; }
static floatw8 operator^(floatw8 a, floatw8 b) { return _mm256_xor_ps(a, b); }
static floatw8& operator^=(floatw8& a, floatw8 b) { a = a ^ b; return a; }

static floatw8 operator~(floatw8 a) { a = andNot(a, floatw8::allOnes()); return a; }


#if defined(SIMD_AVX_512)
static uint8 operator==(intw8 a, intw8 b) { return _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_EQ); }
static uint8 operator!=(intw8 a, intw8 b) { return _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_NE); }
static uint8 operator>(intw8 a, intw8 b) { return _mm256_cmp_epi32_mask(b, a, _MM_CMPINT_LT); }
static uint8 operator>=(intw8 a, intw8 b) { return _mm256_cmp_epi32_mask(b, a, _MM_CMPINT_LE); }
static uint8 operator<(intw8 a, intw8 b) { return _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_LT); }
static uint8 operator<=(intw8 a, intw8 b) { return _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_LE); }
#else
static intw8 operator==(intw8 a, intw8 b) { return _mm256_cmpeq_epi32(a, b); }
static intw8 operator!=(intw8 a, intw8 b) { return ~(a == b); }
static intw8 operator>(intw8 a, intw8 b) { return _mm256_cmpgt_epi32(a, b); }
static intw8 operator>=(intw8 a, intw8 b) { return (a > b) | (a == b); }
static intw8 operator<(intw8 a, intw8 b) { return _mm256_cmpgt_epi32(b, a); }
static intw8 operator<=(intw8 a, intw8 b) { return (a < b) | (a == b); }
#endif


#if defined(SIMD_AVX_512)
static uint8 operator==(floatw8 a, floatw8 b) { return _mm256_cmp_ps_mask(a, b, _CMP_EQ_OQ); }
static uint8 operator!=(floatw8 a, floatw8 b) { return _mm256_cmp_ps_mask(a, b, _CMP_NEQ_OQ); }
static uint8 operator>(floatw8 a, floatw8 b) { return _mm256_cmp_ps_mask(a, b, _CMP_GT_OQ); }
static uint8 operator>=(floatw8 a, floatw8 b) { return _mm256_cmp_ps_mask(a, b, _CMP_GE_OQ); }
static uint8 operator<(floatw8 a, floatw8 b) { return _mm256_cmp_ps_mask(a, b, _CMP_LT_OQ); }
static uint8 operator<=(floatw8 a, floatw8 b) { return _mm256_cmp_ps_mask(a, b, _CMP_LE_OQ); }
#else
static floatw8 operator==(floatw8 a, floatw8 b) { return _mm256_cmp_ps(a, b, _CMP_EQ_OQ); }
static floatw8 operator!=(floatw8 a, floatw8 b) { return _mm256_cmp_ps(a, b, _CMP_NEQ_OQ); }
static floatw8 operator>(floatw8 a, floatw8 b) { return _mm256_cmp_ps(a, b, _CMP_GT_OQ); }
static floatw8 operator>=(floatw8 a, floatw8 b) { return _mm256_cmp_ps(a, b, _CMP_GE_OQ); }
static floatw8 operator<(floatw8 a, floatw8 b) { return _mm256_cmp_ps(a, b, _CMP_LT_OQ); }
static floatw8 operator<=(floatw8 a, floatw8 b) { return _mm256_cmp_ps(a, b, _CMP_LE_OQ); }
#endif

static floatw8 operator>>(floatw8 a, int b) { return reinterpret(reinterpret(a) >> b); }
static floatw8& operator>>=(floatw8& a, int b) { a = a >> b; return a; }
static floatw8 operator<<(floatw8 a, int b) { return reinterpret(reinterpret(a) << b); }
static floatw8& operator<<=(floatw8& a, int b) { a = a << b; return a; }

static floatw8 operator-(floatw8 a) { return _mm256_xor_ps(a, reinterpret(intw8(1 << 31))); }




static float addElements(floatw8 a) { __m256 aa = _mm256_hadd_ps(a, a); aa = _mm256_hadd_ps(aa, aa); return aa.m256_f32[0] + aa.m256_f32[4]; }

static floatw8 fmadd(floatw8 a, floatw8 b, floatw8 c) { return _mm256_fmadd_ps(a, b, c); }
static floatw8 fmsub(floatw8 a, floatw8 b, floatw8 c) { return _mm256_fmsub_ps(a, b, c); }

static floatw8 sqrt(floatw8 a) { return _mm256_sqrt_ps(a); }
static floatw8 rsqrt(floatw8 a) { return _mm256_rsqrt_ps(a); }

static int toBitMask(floatw8 a) { return _mm256_movemask_ps(a); }
static int toBitMask(intw8 a) { return toBitMask(reinterpret(a)); }

#if defined(SIMD_AVX_512)
static floatw8 ifThen(uint8 cond, floatw8 ifCase, floatw8 elseCase) { return _mm256_mask_blend_ps(cond, elseCase, ifCase); }
static intw8 ifThen(uint8 cond, intw8 ifCase, intw8 elseCase) { return reinterpret(ifThen(cond, reinterpret(ifCase), reinterpret(elseCase))); }

static int toBitMask(uint8 a) { return a; }

static bool allTrue(uint8 a) { return a == (1 << 8) - 1; }
static bool allFalse(uint8 a) { return a == 0; }
static bool anyTrue(uint8 a) { return a > 0; }
static bool anyFalse(uint8 a) { return !allTrue(a); }
#else
static floatw8 ifThen(floatw8 cond, floatw8 ifCase, floatw8 elseCase) { return _mm256_blendv_ps(elseCase, ifCase, cond); }
static intw8 ifThen(intw8 cond, intw8 ifCase, intw8 elseCase) { return reinterpret(ifThen(reinterpret(cond), reinterpret(ifCase), reinterpret(elseCase))); }

static bool allTrue(floatw8 a) { return toBitMask(a) == (1 << 8) - 1; }
static bool allFalse(floatw8 a) { return toBitMask(a) == 0; }
static bool anyTrue(floatw8 a) { return toBitMask(a) > 0; }
static bool anyFalse(floatw8 a) { return !allTrue(a); }

static bool allTrue(intw8 a) { return allTrue(reinterpret(a)); }
static bool allFalse(intw8 a) { return allFalse(reinterpret(a)); }
static bool anyTrue(intw8 a) { return anyTrue(reinterpret(a)); }
static bool anyFalse(intw8 a) { return anyFalse(reinterpret(a)); }
#endif


static floatw8 abs(floatw8 a) { floatw8 result = andNot(-0.f, a); return result; }
static floatw8 floor(floatw8 a) { return _mm256_floor_ps(a); }
static floatw8 minimum(floatw8 a, floatw8 b) { return _mm256_min_ps(a, b); }
static floatw8 maximum(floatw8 a, floatw8 b) { return _mm256_max_ps(a, b); }

static floatw8 lerp(floatw8 l, floatw8 u, floatw8 t) { return fmadd(t, u - l, l); }
static floatw8 inverseLerp(floatw8 l, floatw8 u, floatw8 v) { return (v - l) / (u - l); }
static floatw8 remap(floatw8 v, floatw8 oldL, floatw8 oldU, floatw8 newL, floatw8 newU) { return lerp(newL, newU, inverseLerp(oldL, oldU, v)); }
static floatw8 clamp(floatw8 v, floatw8 l, floatw8 u) { return minimum(u, maximum(l, v)); }
static floatw8 clamp01(floatw8 v) { return clamp(v, 0.f, 1.f); }

static floatw8 signOf(floatw8 f) { floatw8 z = floatw8::zero(); return ifThen(f < z, floatw8(-1), ifThen(f == z, z, floatw8(1))); }
static floatw8 signbit(floatw8 f) { return (f & -0.f) >> 31; }

static floatw8 cos(floatw8 x) { return cosInternal(x); }
static floatw8 sin(floatw8 x) { return sinInternal(x); }
static floatw8 exp2(floatw8 x) { return exp2Internal<floatw8, intw8>(x); }
static floatw8 log2(floatw8 x) { return log2Internal<floatw8, intw8>(x); }
static floatw8 pow(floatw8 x, floatw8 y) { return powInternal<floatw8, intw8>(x, y); }
static floatw8 exp(floatw8 x) { return expInternal<floatw8, intw8>(x); }
static floatw8 tanh(floatw8 x) { return tanhInternal(x); }
static floatw8 atan(floatw8 x) { return atanInternal<floatw8, intw8>(x); }
static floatw8 atan2(floatw8 y, floatw8 x) { return atan2Internal<floatw8, intw8>(y, x); }
static floatw8 acos(floatw8 x) { return acosInternal(x); }

static floatw8 concat(floatw4 a, floatw4 b)
{
	return _mm256_insertf128_ps(_mm256_castps128_ps256(a), b, 1);
}

static intw8 concat(intw4 a, intw4 b)
{
	return _mm256_inserti128_si256(_mm256_castsi128_si256(a), b, 1);
}

static floatw8 concatLow(floatw8 a, floatw8 b)
{
	return _mm256_permute2f128_ps(a, b, 0 | (0 << 4));
}

static intw8 concatLow(intw8 a, intw8 b)
{
	return _mm256_permute2x128_si256(a, b, 0 | (0 << 4));
}

static intw8 fillWithFirstLane(intw8 a)
{
	intw8 first = _mm256_shuffle_epi32(a, _MM_SHUFFLE(0, 0, 0, 0));
	first = concatLow(first, first);
	return first;
}

static void transpose32(floatw8& out0, floatw8& out1, floatw8& out2, floatw8& out3)
{
	floatw8 t0 = _mm256_unpacklo_ps(out0, out1);
	floatw8 t1 = _mm256_unpacklo_ps(out2, out3);
	floatw8 t2 = _mm256_unpackhi_ps(out0, out1);
	floatw8 t3 = _mm256_unpackhi_ps(out2, out3);
	out0 = _mm256_shuffle_ps(t0, t1, _MM_SHUFFLE(1, 0, 1, 0));
	out1 = _mm256_shuffle_ps(t0, t1, _MM_SHUFFLE(3, 2, 3, 2));
	out2 = _mm256_shuffle_ps(t2, t3, _MM_SHUFFLE(1, 0, 1, 0));
	out3 = _mm256_shuffle_ps(t2, t3, _MM_SHUFFLE(3, 2, 3, 2));
}

static void transpose(floatw8& out0, floatw8& out1, floatw8& out2, floatw8& out3, floatw8& out4, floatw8& out5, floatw8& out6, floatw8& out7)
{
	floatw8 tmp0 = reinterpret(_mm256_permute2x128_si256(reinterpret(out0), reinterpret(out4), 0 | (2 << 4)));
	floatw8 tmp1 = reinterpret(_mm256_permute2x128_si256(reinterpret(out1), reinterpret(out5), 0 | (2 << 4)));
	floatw8 tmp2 = reinterpret(_mm256_permute2x128_si256(reinterpret(out2), reinterpret(out6), 0 | (2 << 4)));
	floatw8 tmp3 = reinterpret(_mm256_permute2x128_si256(reinterpret(out3), reinterpret(out7), 0 | (2 << 4)));
	floatw8 tmp4 = reinterpret(_mm256_permute2x128_si256(reinterpret(out0), reinterpret(out4), 1 | (3 << 4)));
	floatw8 tmp5 = reinterpret(_mm256_permute2x128_si256(reinterpret(out1), reinterpret(out5), 1 | (3 << 4)));
	floatw8 tmp6 = reinterpret(_mm256_permute2x128_si256(reinterpret(out2), reinterpret(out6), 1 | (3 << 4)));
	floatw8 tmp7 = reinterpret(_mm256_permute2x128_si256(reinterpret(out3), reinterpret(out7), 1 | (3 << 4)));

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
	floatw8& out0, floatw8& out1, floatw8& out2, floatw8& out3)
{
	const uint32 strideInFloats = stride / sizeof(float);

	floatw4 tmp0(baseAddress + strideInFloats * indices[0]);
	floatw4 tmp1(baseAddress + strideInFloats * indices[1]);
	floatw4 tmp2(baseAddress + strideInFloats * indices[2]);
	floatw4 tmp3(baseAddress + strideInFloats * indices[3]);
	floatw4 tmp4(baseAddress + strideInFloats * indices[4]);
	floatw4 tmp5(baseAddress + strideInFloats * indices[5]);
	floatw4 tmp6(baseAddress + strideInFloats * indices[6]);
	floatw4 tmp7(baseAddress + strideInFloats * indices[7]);

	out0 = concat(tmp0, tmp4);
	out1 = concat(tmp1, tmp5);
	out2 = concat(tmp2, tmp6);
	out3 = concat(tmp3, tmp7);

	transpose32(out0, out1, out2, out3);
}

static void load8(const float* baseAddress, const uint16* indices, uint32 stride,
	floatw8& out0, floatw8& out1, floatw8& out2, floatw8& out3, floatw8& out4, floatw8& out5, floatw8& out6, floatw8& out7)
{
	const uint32 strideInFloats = stride / sizeof(float);

	out0 = floatw8(baseAddress + strideInFloats * indices[0]);
	out1 = floatw8(baseAddress + strideInFloats * indices[1]);
	out2 = floatw8(baseAddress + strideInFloats * indices[2]);
	out3 = floatw8(baseAddress + strideInFloats * indices[3]);
	out4 = floatw8(baseAddress + strideInFloats * indices[4]);
	out5 = floatw8(baseAddress + strideInFloats * indices[5]);
	out6 = floatw8(baseAddress + strideInFloats * indices[6]);
	out7 = floatw8(baseAddress + strideInFloats * indices[7]);

	transpose(out0, out1, out2, out3, out4, out5, out6, out7);
}

static void store8(float* baseAddress, const uint16* indices, uint32 stride,
	floatw8 in0, floatw8 in1, floatw8 in2, floatw8 in3, floatw8 in4, floatw8 in5, floatw8 in6, floatw8 in7)
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

struct floatw16
{
	__m512 f;

	floatw16() {}
	floatw16(float f_) { f = _mm512_set1_ps(f_); }
	floatw16(__m512 f_) { f = f_; }
	floatw16(float a, float b, float c, float d, float e, float f, float g, float h, float i, float j, float k, float l, float m, float n, float o, float p) { this->f = _mm512_setr_ps(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p); }
	floatw16(const float* f_) { f = _mm512_loadu_ps(f_); }

	floatw16(const float* baseAddress, __m512i indices) { f = _mm512_i32gather_ps(indices, baseAddress, 4); }
	floatw16(const float* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l, int m, int n, int o, int p) : floatw16(baseAddress, _mm512_setr_epi32(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)) {}

	operator __m512() { return f; }

	void store(float* f_) const { _mm512_storeu_ps(f_, f); }

	void scatter(float* baseAddress, __m512i indices) const { _mm512_i32scatter_ps(baseAddress, indices, f, 4); }
	void scatter(float* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l, int m, int n, int o, int p) const { scatter(baseAddress, _mm512_setr_epi32(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)); }
};

struct intw16
{
	__m512i i;

	intw16() {}
	intw16(int i_) { i = _mm512_set1_epi32(i_); }
	intw16(__m512i i_) { i = i_; }
	intw16(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l, int m, int n, int o, int p) { this->i = _mm512_setr_epi32(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p); }
	intw16(const int* i_) { i = _mm512_loadu_epi32(i_); }

	intw16(const int* baseAddress, __m512i indices) { i = _mm512_i32gather_epi32(indices, baseAddress, 4); }
	intw16(const int* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l, int m, int n, int o, int p) : intw16(baseAddress, _mm512_setr_epi32(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)) {}

	operator __m512i() { return i; }

	void store(int* i_) const { _mm512_storeu_epi32(i_, i); }
};

static floatw16 truex16() { const float nnan = (const float&)0xFFFFFFFF; return nnan; }
static floatw16 zerox16() { return _mm512_setzero_ps(); }

static floatw16 convert(intw16 i) { return _mm512_cvtepi32_ps(i); }
static intw16 convert(floatw16 f) { return _mm512_cvtps_epi32(f); }
static floatw16 reinterpret(intw16 i) { return _mm512_castsi512_ps(i); }
static intw16 reinterpret(floatw16 f) { return _mm512_castps_si512(f); }


// Int operators.
static intw16 andNot(intw16 a, intw16 b) { return _mm512_andnot_si512(a, b); }

static intw16 operator+(intw16 a, intw16 b) { return _mm512_add_epi32(a, b); }
static intw16& operator+=(intw16& a, intw16 b) { a = a + b; return a; }
static intw16 operator-(intw16 a, intw16 b) { return _mm512_sub_epi32(a, b); }
static intw16& operator-=(intw16& a, intw16 b) { a = a - b; return a; }
static intw16 operator*(intw16 a, intw16 b) { return _mm512_mul_epi32(a, b); }
static intw16& operator*=(intw16& a, intw16 b) { a = a * b; return a; }
static intw16 operator/(intw16 a, intw16 b) { return _mm512_div_epi32(a, b); }
static intw16& operator/=(intw16& a, intw16 b) { a = a / b; return a; }
static intw16 operator&(intw16 a, intw16 b) { return _mm512_and_epi32(a, b); }
static intw16& operator&=(intw16& a, intw16 b) { a = a & b; return a; }
static intw16 operator|(intw16 a, intw16 b) { return _mm512_or_epi32(a, b); }
static intw16& operator|=(intw16& a, intw16 b) { a = a | b; return a; }
static intw16 operator^(intw16 a, intw16 b) { return _mm512_xor_epi32(a, b); }
static intw16& operator^=(intw16& a, intw16 b) { a = a ^ b; return a; }

static intw16 operator~(intw16 a) { a = andNot(a, reinterpret(truex16())); return a; }

static uint16 operator==(intw16 a, intw16 b) { return _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_EQ); }
static uint16 operator!=(intw16 a, intw16 b) { return _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_NE); }
static uint16 operator>(intw16 a, intw16 b) { return _mm512_cmp_epi32_mask(b, a, _MM_CMPINT_LT); }
static uint16 operator>=(intw16 a, intw16 b) { return _mm512_cmp_epi32_mask(b, a, _MM_CMPINT_LE); }
static uint16 operator<(intw16 a, intw16 b) { return _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_LT); }
static uint16 operator<=(intw16 a, intw16 b) { return _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_LE); }


static intw16 operator>>(intw16 a, int b) { return _mm512_srli_epi32(a, b); }
static intw16& operator>>=(intw16& a, int b) { a = a >> b; return a; }
static intw16 operator<<(intw16 a, int b) { return _mm512_slli_epi32(a, b); }
static intw16& operator<<=(intw16& a, int b) { a = a << b; return a; }

static intw16 operator-(intw16 a) { return _mm512_sub_epi32(_mm512_setzero_si512(), a); }



// Float operators.
static floatw16 andNot(floatw16 a, floatw16 b) { return _mm512_andnot_ps(a, b); }

static floatw16 operator+(floatw16 a, floatw16 b) { return _mm512_add_ps(a, b); }
static floatw16& operator+=(floatw16& a, floatw16 b) { a = a + b; return a; }
static floatw16 operator-(floatw16 a, floatw16 b) { return _mm512_sub_ps(a, b); }
static floatw16& operator-=(floatw16& a, floatw16 b) { a = a - b; return a; }
static floatw16 operator*(floatw16 a, floatw16 b) { return _mm512_mul_ps(a, b); }
static floatw16& operator*=(floatw16& a, floatw16 b) { a = a * b; return a; }
static floatw16 operator/(floatw16 a, floatw16 b) { return _mm512_div_ps(a, b); }
static floatw16& operator/=(floatw16& a, floatw16 b) { a = a / b; return a; }
static floatw16 operator&(floatw16 a, floatw16 b) { return _mm512_and_ps(a, b); }
static floatw16& operator&=(floatw16& a, floatw16 b) { a = a & b; return a; }
static floatw16 operator|(floatw16 a, floatw16 b) { return _mm512_or_ps(a, b); }
static floatw16& operator|=(floatw16& a, floatw16 b) { a = a | b; return a; }
static floatw16 operator^(floatw16 a, floatw16 b) { return _mm512_xor_ps(a, b); }
static floatw16& operator^=(floatw16& a, floatw16 b) { a = a ^ b; return a; }

static floatw16 operator~(floatw16 a) { a = andNot(a, truex16()); return a; }

static uint16 operator==(floatw16 a, floatw16 b) { return _mm512_cmp_ps_mask(a, b, _CMP_EQ_OQ); }
static uint16 operator!=(floatw16 a, floatw16 b) { return _mm512_cmp_ps_mask(a, b, _CMP_NEQ_OQ); }
static uint16 operator>(floatw16 a, floatw16 b) { return _mm512_cmp_ps_mask(a, b, _CMP_GT_OQ); }
static uint16 operator>=(floatw16 a, floatw16 b) { return _mm512_cmp_ps_mask(a, b, _CMP_GE_OQ); }
static uint16 operator<(floatw16 a, floatw16 b) { return _mm512_cmp_ps_mask(a, b, _CMP_LT_OQ); }
static uint16 operator<=(floatw16 a, floatw16 b) { return _mm512_cmp_ps_mask(a, b, _CMP_LE_OQ); }


static floatw16 operator>>(floatw16 a, int b) { return reinterpret(reinterpret(a) >> b); }
static floatw16& operator>>=(floatw16& a, int b) { a = a >> b; return a; }
static floatw16 operator<<(floatw16 a, int b) { return reinterpret(reinterpret(a) << b); }
static floatw16& operator<<=(floatw16& a, int b) { a = a << b; return a; }

static floatw16 operator-(floatw16 a) { return _mm512_xor_ps(a, reinterpret(intw16(1 << 31))); }




static float addElements(floatw16 a) { return _mm512_reduce_add_ps(a); }

static floatw16 fmadd(floatw16 a, floatw16 b, floatw16 c) { return _mm512_fmadd_ps(a, b, c); }
static floatw16 fmsub(floatw16 a, floatw16 b, floatw16 c) { return _mm512_fmsub_ps(a, b, c); }

static floatw16 sqrt(floatw16 a) { return _mm512_sqrt_ps(a); }
static floatw16 rsqrt(floatw16 a) { return 1.f / _mm512_sqrt_ps(a); }

static floatw16 ifThen(uint16 cond, floatw16 ifCase, floatw16 elseCase) { return _mm512_mask_blend_ps(cond, elseCase, ifCase); }
static intw16 ifThen(uint16 cond, intw16 ifCase, intw16 elseCase) { return reinterpret(ifThen(cond, reinterpret(ifCase), reinterpret(elseCase))); }

static bool allTrue(uint16 a) { return a == (1 << 16) - 1; }
static bool allFalse(uint16 a) { return a == 0; }
static bool anyTrue(uint16 a) { return a > 0; }
static bool anyFalse(uint16 a) { return !allTrue(a); }


static floatw16 abs(floatw16 a) { floatw16 result = andNot(-0.f, a); return result; }
static floatw16 floor(floatw16 a) { return _mm512_floor_ps(a); }
static floatw16 minimum(floatw16 a, floatw16 b) { return _mm512_min_ps(a, b); }
static floatw16 maximum(floatw16 a, floatw16 b) { return _mm512_max_ps(a, b); }

static floatw16 lerp(floatw16 l, floatw16 u, floatw16 t) { return fmadd(t, u - l, l); }
static floatw16 inverseLerp(floatw16 l, floatw16 u, floatw16 v) { return (v - l) / (u - l); }
static floatw16 remap(floatw16 v, floatw16 oldL, floatw16 oldU, floatw16 newL, floatw16 newU) { return lerp(newL, newU, inverseLerp(oldL, oldU, v)); }
static floatw16 clamp(floatw16 v, floatw16 l, floatw16 u) { return minimum(u, maximum(l, v)); }
static floatw16 clamp01(floatw16 v) { return clamp(v, 0.f, 1.f); }

static floatw16 signOf(floatw16 f) { return ifThen(f < 0.f, floatw16(-1), ifThen(f == 0.f, zerox16(), floatw16(1))); }
static floatw16 signbit(floatw16 f) { return (f & -0.f) >> 31; }

static floatw16 cos(floatw16 x) { return cosInternal(x); }
static floatw16 sin(floatw16 x) { return sinInternal(x); }
static floatw16 exp2(floatw16 x) { return exp2Internal<floatw16, intw16>(x); }
static floatw16 log2(floatw16 x) { return log2Internal<floatw16, intw16>(x); }
static floatw16 pow(floatw16 x, floatw16 y) { return powInternal<floatw16, intw16>(x, y); }
static floatw16 exp(floatw16 x) { return expInternal<floatw16, intw16>(x); }
static floatw16 tanh(floatw16 x) { return tanhInternal(x); }
static floatw16 atan(floatw16 x) { return atanInternal<floatw16, intw16>(x); }
static floatw16 atan2(floatw16 y, floatw16 x) { return atan2Internal<floatw16, intw16>(y, x); }
static floatw16 acos(floatw16 x) { return acosInternal(x); }


#endif








