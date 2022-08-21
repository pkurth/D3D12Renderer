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

struct w4_float
{
	__m128 f;

	w4_float() {}
	w4_float(float f_) { f = _mm_set1_ps(f_); }
	w4_float(__m128 f_) { f = f_; }
	w4_float(float a, float b, float c, float d) { f = _mm_setr_ps(a, b, c, d); }
	w4_float(const float* f_) { f = _mm_loadu_ps(f_); }

#if defined(SIMD_AVX_2)
	w4_float(const float* baseAddress, __m128i indices) { f = _mm_i32gather_ps(baseAddress, indices, 4); }
	w4_float(const float* baseAddress, int a, int b, int c, int d) : w4_float(baseAddress, _mm_setr_epi32(a, b, c, d)) {}
#else
	w4_float(const float* baseAddress, int a, int b, int c, int d) { f = _mm_setr_ps(baseAddress[a], baseAddress[b], baseAddress[c], baseAddress[d]);  }
	w4_float(const float* baseAddress, __m128i indices) : w4_float(baseAddress, indices.m128i_i32[0], indices.m128i_i32[1], indices.m128i_i32[2], indices.m128i_i32[3]) {}
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

	static w4_float allOnes() { const float nnan = (const float&)0xFFFFFFFF; return nnan; }
	static w4_float zero() { return _mm_setzero_ps(); }
};

struct w4_int
{
	__m128i i;

	w4_int() {}
	w4_int(int i_) { i = _mm_set1_epi32(i_); }
	w4_int(__m128i i_) { i = i_; }
	w4_int(int a, int b, int c, int d) { i = _mm_setr_epi32(a, b, c, d); }
	w4_int(int* i_) { i = _mm_loadu_si128((const __m128i*)i_); }

#if defined(SIMD_AVX_2)
	w4_int(const int* baseAddress, __m128i indices) { i = _mm_i32gather_epi32(baseAddress, indices, 4); }
	w4_int(const int* baseAddress, int a, int b, int c, int d) : w4_int(baseAddress, _mm_setr_epi32(a, b, c, d)) {}
#else
	w4_int(const int* baseAddress, int a, int b, int c, int d) { i = _mm_setr_epi32(baseAddress[a], baseAddress[b], baseAddress[c], baseAddress[d]); }
	w4_int(const int* baseAddress, __m128i indices) : w4_int(baseAddress, indices.m128i_i32[0], indices.m128i_i32[1], indices.m128i_i32[2], indices.m128i_i32[3]) {}
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

	static w4_int allOnes() { return UINT32_MAX; }
	static w4_int zero() { return _mm_setzero_si128(); }
};

static w4_float convert(w4_int i) { return _mm_cvtepi32_ps(i); }
static w4_int convert(w4_float f) { return _mm_cvtps_epi32(f); }
static w4_float reinterpret(w4_int i) { return _mm_castsi128_ps(i); }
static w4_int reinterpret(w4_float f) { return _mm_castps_si128(f); }


// Int operators.
static w4_int andNot(w4_int a, w4_int b) { return _mm_andnot_si128(a, b); }

static w4_int operator+(w4_int a, w4_int b) { return _mm_add_epi32(a, b); }
static w4_int& operator+=(w4_int& a, w4_int b) { a = a + b; return a; }
static w4_int operator-(w4_int a, w4_int b) { return _mm_sub_epi32(a, b); }
static w4_int& operator-=(w4_int& a, w4_int b) { a = a - b; return a; }
static w4_int operator*(w4_int a, w4_int b) { return _mm_mul_epi32(a, b); }
static w4_int& operator*=(w4_int& a, w4_int b) { a = a * b; return a; }
static w4_int operator/(w4_int a, w4_int b) { return _mm_div_epi32(a, b); }
static w4_int& operator/=(w4_int& a, w4_int b) { a = a / b; return a; }
static w4_int operator&(w4_int a, w4_int b) { return _mm_and_si128(a, b); }
static w4_int& operator&=(w4_int& a, w4_int b) { a = a & b; return a; }
static w4_int operator|(w4_int a, w4_int b) { return _mm_or_si128(a, b); }
static w4_int& operator|=(w4_int& a, w4_int b) { a = a | b; return a; }
static w4_int operator^(w4_int a, w4_int b) { return _mm_xor_si128(a, b); }
static w4_int& operator^=(w4_int& a, w4_int b) { a = a ^ b; return a; }

static w4_int operator~(w4_int a) { a = andNot(a, w4_int::allOnes()); return a; }

static w4_int operator>>(w4_int a, int b) { return _mm_srli_epi32(a, b); }
static w4_int operator>>(w4_int a, w4_int b) { return _mm_srlv_epi32(a, b); }
static w4_int& operator>>=(w4_int& a, int b) { a = a >> b; return a; }
static w4_int& operator>>=(w4_int& a, w4_int b) { a = a >> b; return a; }
static w4_int operator<<(w4_int a, int b) { return _mm_slli_epi32(a, b); }
static w4_int operator<<(w4_int a, w4_int b) { return _mm_sllv_epi32(a, b); }
static w4_int& operator<<=(w4_int& a, int b) { a = a << b; return a; }
static w4_int& operator<<=(w4_int& a, w4_int b) { a = a << b; return a; }

static w4_int operator-(w4_int a) { return _mm_sub_epi32(w4_int::zero(), a); }



// Float operators.
static w4_float andNot(w4_float a, w4_float b) { return _mm_andnot_ps(a, b); }

static w4_float operator+(w4_float a, w4_float b) { return _mm_add_ps(a, b); }
static w4_float& operator+=(w4_float& a, w4_float b) { a = a + b; return a; }
static w4_float operator-(w4_float a, w4_float b) { return _mm_sub_ps(a, b); }
static w4_float& operator-=(w4_float& a, w4_float b) { a = a - b; return a; }
static w4_float operator*(w4_float a, w4_float b) { return _mm_mul_ps(a, b); }
static w4_float& operator*=(w4_float& a, w4_float b) { a = a * b; return a; }
static w4_float operator/(w4_float a, w4_float b) { return _mm_div_ps(a, b); }
static w4_float& operator/=(w4_float& a, w4_float b) { a = a / b; return a; }
static w4_float operator&(w4_float a, w4_float b) { return _mm_and_ps(a, b); }
static w4_float& operator&=(w4_float& a, w4_float b) { a = a & b; return a; }
static w4_float operator|(w4_float a, w4_float b) { return _mm_or_ps(a, b); }
static w4_float& operator|=(w4_float& a, w4_float b) { a = a | b; return a; }
static w4_float operator^(w4_float a, w4_float b) { return _mm_xor_ps(a, b); }
static w4_float& operator^=(w4_float& a, w4_float b) { a = a ^ b; return a; }

static w4_float operator~(w4_float a) { a = andNot(a, w4_float::allOnes()); return a; }


static w4_int operator==(w4_int a, w4_int b) { return _mm_cmpeq_epi32(a, b); }
static w4_int operator!=(w4_int a, w4_int b) { return ~(a == b); }
static w4_int operator>(w4_int a, w4_int b) { return _mm_cmpgt_epi32(a, b); }
static w4_int operator>=(w4_int a, w4_int b) { return (a > b) | (a == b); }
static w4_int operator<(w4_int a, w4_int b) { return _mm_cmplt_epi32(a, b); }
static w4_int operator<=(w4_int a, w4_int b) { return (a < b) | (a == b); }

static w4_float operator==(w4_float a, w4_float b) { return _mm_cmpeq_ps(a, b); }
static w4_float operator!=(w4_float a, w4_float b) { return _mm_cmpneq_ps(a, b); }
static w4_float operator>(w4_float a, w4_float b) { return _mm_cmpgt_ps(a, b); }
static w4_float operator>=(w4_float a, w4_float b) { return _mm_cmpge_ps(a, b); }
static w4_float operator<(w4_float a, w4_float b) { return _mm_cmplt_ps(a, b); }
static w4_float operator<=(w4_float a, w4_float b) { return _mm_cmple_ps(a, b); }

static w4_float operator>>(w4_float a, int b) { return reinterpret(reinterpret(a) >> b); }
static w4_float& operator>>=(w4_float& a, int b) { a = a >> b; return a; }
static w4_float operator<<(w4_float a, int b) { return reinterpret(reinterpret(a) << b); }
static w4_float& operator<<=(w4_float& a, int b) { a = a << b; return a; }

static w4_float operator-(w4_float a) { return _mm_xor_ps(a, reinterpret(w4_int(1 << 31))); }




static float addElements(w4_float a) { __m128 aa = _mm_hadd_ps(a, a); aa = _mm_hadd_ps(aa, aa); return aa.m128_f32[0]; }

static w4_float fmadd(w4_float a, w4_float b, w4_float c) { return _mm_fmadd_ps(a, b, c); }
static w4_float fmsub(w4_float a, w4_float b, w4_float c) { return _mm_fmsub_ps(a, b, c); }

static w4_float sqrt(w4_float a) { return _mm_sqrt_ps(a); }
static w4_float rsqrt(w4_float a) { return _mm_rsqrt_ps(a); }

static w4_float ifThen(w4_float cond, w4_float ifCase, w4_float elseCase) { return _mm_blendv_ps(elseCase, ifCase, cond); }
static w4_int ifThen(w4_int cond, w4_int ifCase, w4_int elseCase) { return reinterpret(ifThen(reinterpret(cond), reinterpret(ifCase), reinterpret(elseCase))); }

static int toBitMask(w4_float a) { return _mm_movemask_ps(a); }
static int toBitMask(w4_int a) { return toBitMask(reinterpret(a)); }

static bool allTrue(w4_float a) { return toBitMask(a) == (1 << 4) - 1; }
static bool allFalse(w4_float a) { return toBitMask(a) == 0; }
static bool anyTrue(w4_float a) { return toBitMask(a) > 0; }
static bool anyFalse(w4_float a) { return !allTrue(a); }

static bool allTrue(w4_int a) { return allTrue(reinterpret(a)); }
static bool allFalse(w4_int a) { return allFalse(reinterpret(a)); }
static bool anyTrue(w4_int a) { return anyTrue(reinterpret(a)); }
static bool anyFalse(w4_int a) { return anyFalse(reinterpret(a)); }

static w4_float abs(w4_float a) { w4_float result = andNot(-0.f, a); return result; }
static w4_float floor(w4_float a) { return _mm_floor_ps(a); }
static w4_float round(w4_float a) { return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC); }
static w4_float minimum(w4_float a, w4_float b) { return _mm_min_ps(a, b); }
static w4_float maximum(w4_float a, w4_float b) { return _mm_max_ps(a, b); }

static w4_float lerp(w4_float l, w4_float u, w4_float t) { return fmadd(t, u - l, l); }
static w4_float inverseLerp(w4_float l, w4_float u, w4_float v) { return (v - l) / (u - l); }
static w4_float remap(w4_float v, w4_float oldL, w4_float oldU, w4_float newL, w4_float newU) { return lerp(newL, newU, inverseLerp(oldL, oldU, v)); }
static w4_float clamp(w4_float v, w4_float l, w4_float u) { return minimum(u, maximum(l, v)); }
static w4_float clamp01(w4_float v) { return clamp(v, 0.f, 1.f); }

static w4_float signOf(w4_float f) { w4_float z = w4_float::zero(); return ifThen(f < z, w4_float(-1), ifThen(f == z, z, w4_float(1))); }
static w4_float signbit(w4_float f) { return (f & -0.f) >> 31; }

static w4_float cos(w4_float x) { return cosInternal(x); }
static w4_float sin(w4_float x) { return sinInternal(x); }
static w4_float exp2(w4_float x) { return exp2Internal<w4_float, w4_int>(x); }
static w4_float log2(w4_float x) { return log2Internal<w4_float, w4_int>(x); }
static w4_float pow(w4_float x, w4_float y) { return powInternal<w4_float, w4_int>(x, y); }
static w4_float exp(w4_float x) { return expInternal<w4_float, w4_int>(x); }
static w4_float tanh(w4_float x) { return tanhInternal(x); }
static w4_float atan(w4_float x) { return atanInternal<w4_float, w4_int>(x); }
static w4_float atan2(w4_float y, w4_float x) { return atan2Internal<w4_float, w4_int>(y, x); }
static w4_float acos(w4_float x) { return acosInternal(x); }

static w4_int fillWithFirstLane(w4_int a)
{
	w4_int first = _mm_shuffle_epi32(a, _MM_SHUFFLE(0, 0, 0, 0));
	return first;
}

static void transpose(w4_float& out0, w4_float& out1, w4_float& out2, w4_float& out3)
{
	_MM_TRANSPOSE4_PS(out0.f, out1.f, out2.f, out3.f);
}

static void load4(const float* baseAddress, const uint16* indices, uint32 stride,
	w4_float& out0, w4_float& out1, w4_float& out2, w4_float& out3)
{
	const uint32 strideInFloats = stride / sizeof(float);

	out0 = w4_float(baseAddress + strideInFloats * indices[0]);
	out1 = w4_float(baseAddress + strideInFloats * indices[1]);
	out2 = w4_float(baseAddress + strideInFloats * indices[2]);
	out3 = w4_float(baseAddress + strideInFloats * indices[3]);

	transpose(out0, out1, out2, out3);
}

static void store4(float* baseAddress, const uint16* indices, uint32 stride,
	w4_float in0, w4_float in1, w4_float in2, w4_float in3)
{
	const uint32 strideInFloats = stride / sizeof(float);

	transpose(in0, in1, in2, in3);

	in0.store(baseAddress + strideInFloats * indices[0]);
	in1.store(baseAddress + strideInFloats * indices[1]);
	in2.store(baseAddress + strideInFloats * indices[2]);
	in3.store(baseAddress + strideInFloats * indices[3]);
}

static void load8(const float* baseAddress, const uint16* indices, uint32 stride,
	w4_float& out0, w4_float& out1, w4_float& out2, w4_float& out3, w4_float& out4, w4_float& out5, w4_float& out6, w4_float& out7)
{
	const uint32 strideInFloats = stride / sizeof(float);

	out0 = w4_float(baseAddress + strideInFloats * indices[0]);
	out1 = w4_float(baseAddress + strideInFloats * indices[1]);
	out2 = w4_float(baseAddress + strideInFloats * indices[2]);
	out3 = w4_float(baseAddress + strideInFloats * indices[3]);
	out4 = w4_float(baseAddress + strideInFloats * indices[0] + 4);
	out5 = w4_float(baseAddress + strideInFloats * indices[1] + 4);
	out6 = w4_float(baseAddress + strideInFloats * indices[2] + 4);
	out7 = w4_float(baseAddress + strideInFloats * indices[3] + 4);

	transpose(out0, out1, out2, out3);
	transpose(out4, out5, out6, out7);
}

static void store8(float* baseAddress, const uint16* indices, uint32 stride,
	w4_float in0, w4_float in1, w4_float in2, w4_float in3, w4_float in4, w4_float in5, w4_float in6, w4_float in7)
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

struct w8_float
{
	__m256 f;

	w8_float() {}
	w8_float(float f_) { f = _mm256_set1_ps(f_); }
	w8_float(__m256 f_) { f = f_; }
	w8_float(float a, float b, float c, float d, float e, float f, float g, float h) { this->f = _mm256_setr_ps(a, b, c, d, e, f, g, h); }
	w8_float(const float* f_) { f = _mm256_loadu_ps(f_); }

	w8_float(const float* baseAddress, __m256i indices) { f = _mm256_i32gather_ps(baseAddress, indices, 4); }
	w8_float(const float* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h) : w8_float(baseAddress, _mm256_setr_epi32(a, b, c, d, e, f, g, h)) {}

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

	static w8_float allOnes() { const float nnan = (const float&)0xFFFFFFFF; return nnan; }
	static w8_float zero() { return _mm256_setzero_ps(); }
};

struct w8_int
{
	__m256i i;

	w8_int() {}
	w8_int(int i_) { i = _mm256_set1_epi32(i_); }
	w8_int(__m256i i_) { i = i_; }
	w8_int(int a, int b, int c, int d, int e, int f, int g, int h) { this->i = _mm256_setr_epi32(a, b, c, d, e, f, g, h); }
	w8_int(const int* i_) { i = _mm256_loadu_epi32(i_); }

	w8_int(const int* baseAddress, __m256i indices) { i = _mm256_i32gather_epi32(baseAddress, indices, 4); }
	w8_int(const int* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h) : w8_int(baseAddress, _mm256_setr_epi32(a, b, c, d, e, f, g, h)) {}

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

	static w8_int allOnes() { return UINT32_MAX; }
	static w8_int zero() { return _mm256_setzero_si256(); }
};

static w8_float convert(w8_int i) { return _mm256_cvtepi32_ps(i); }
static w8_int convert(w8_float f) { return _mm256_cvtps_epi32(f); }
static w8_float reinterpret(w8_int i) { return _mm256_castsi256_ps(i); }
static w8_int reinterpret(w8_float f) { return _mm256_castps_si256(f); }


// Int operators.
static w8_int andNot(w8_int a, w8_int b) { return _mm256_andnot_si256(a, b); }

static w8_int operator+(w8_int a, w8_int b) { return _mm256_add_epi32(a, b); }
static w8_int& operator+=(w8_int& a, w8_int b) { a = a + b; return a; }
static w8_int operator-(w8_int a, w8_int b) { return _mm256_sub_epi32(a, b); }
static w8_int& operator-=(w8_int& a, w8_int b) { a = a - b; return a; }
static w8_int operator*(w8_int a, w8_int b) { return _mm256_mul_epi32(a, b); }
static w8_int& operator*=(w8_int& a, w8_int b) { a = a * b; return a; }
static w8_int operator/(w8_int a, w8_int b) { return _mm256_div_epi32(a, b); }
static w8_int& operator/=(w8_int& a, w8_int b) { a = a / b; return a; }
static w8_int operator&(w8_int a, w8_int b) { return _mm256_and_si256(a, b); }
static w8_int& operator&=(w8_int& a, w8_int b) { a = a & b; return a; }
static w8_int operator|(w8_int a, w8_int b) { return _mm256_or_si256(a, b); }
static w8_int& operator|=(w8_int& a, w8_int b) { a = a | b; return a; }
static w8_int operator^(w8_int a, w8_int b) { return _mm256_xor_si256(a, b); }
static w8_int& operator^=(w8_int& a, w8_int b) { a = a ^ b; return a; }

static w8_int operator~(w8_int a) { a = andNot(a, w8_int::allOnes()); return a; }

static w8_int operator>>(w8_int a, int b) { return _mm256_srli_epi32(a, b); }
static w8_int operator>>(w8_int a, w8_int b) { return _mm256_srlv_epi32(a, b); }
static w8_int& operator>>=(w8_int& a, int b) { a = a >> b; return a; }
static w8_int& operator>>=(w8_int& a, w8_int b) { a = a >> b; return a; }
static w8_int operator<<(w8_int a, int b) { return _mm256_slli_epi32(a, b); }
static w8_int operator<<(w8_int a, w8_int b) { return _mm256_sllv_epi32(a, b); }
static w8_int& operator<<=(w8_int& a, int b) { a = a << b; return a; }
static w8_int& operator<<=(w8_int& a, w8_int b) { a = a << b; return a; }

static w8_int operator-(w8_int a) { return _mm256_sub_epi32(w8_int::zero(), a); }



// Float operators.
static w8_float andNot(w8_float a, w8_float b) { return _mm256_andnot_ps(a, b); }

static w8_float operator+(w8_float a, w8_float b) { return _mm256_add_ps(a, b); }
static w8_float& operator+=(w8_float& a, w8_float b) { a = a + b; return a; }
static w8_float operator-(w8_float a, w8_float b) { return _mm256_sub_ps(a, b); }
static w8_float& operator-=(w8_float& a, w8_float b) { a = a - b; return a; }
static w8_float operator*(w8_float a, w8_float b) { return _mm256_mul_ps(a, b); }
static w8_float& operator*=(w8_float& a, w8_float b) { a = a * b; return a; }
static w8_float operator/(w8_float a, w8_float b) { return _mm256_div_ps(a, b); }
static w8_float& operator/=(w8_float& a, w8_float b) { a = a / b; return a; }
static w8_float operator&(w8_float a, w8_float b) { return _mm256_and_ps(a, b); }
static w8_float& operator&=(w8_float& a, w8_float b) { a = a & b; return a; }
static w8_float operator|(w8_float a, w8_float b) { return _mm256_or_ps(a, b); }
static w8_float& operator|=(w8_float& a, w8_float b) { a = a | b; return a; }
static w8_float operator^(w8_float a, w8_float b) { return _mm256_xor_ps(a, b); }
static w8_float& operator^=(w8_float& a, w8_float b) { a = a ^ b; return a; }

static w8_float operator~(w8_float a) { a = andNot(a, w8_float::allOnes()); return a; }


#if defined(SIMD_AVX_512)
static uint8 operator==(w8_int a, w8_int b) { return _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_EQ); }
static uint8 operator!=(w8_int a, w8_int b) { return _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_NE); }
static uint8 operator>(w8_int a, w8_int b) { return _mm256_cmp_epi32_mask(b, a, _MM_CMPINT_LT); }
static uint8 operator>=(w8_int a, w8_int b) { return _mm256_cmp_epi32_mask(b, a, _MM_CMPINT_LE); }
static uint8 operator<(w8_int a, w8_int b) { return _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_LT); }
static uint8 operator<=(w8_int a, w8_int b) { return _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_LE); }
#else
static w8_int operator==(w8_int a, w8_int b) { return _mm256_cmpeq_epi32(a, b); }
static w8_int operator!=(w8_int a, w8_int b) { return ~(a == b); }
static w8_int operator>(w8_int a, w8_int b) { return _mm256_cmpgt_epi32(a, b); }
static w8_int operator>=(w8_int a, w8_int b) { return (a > b) | (a == b); }
static w8_int operator<(w8_int a, w8_int b) { return _mm256_cmpgt_epi32(b, a); }
static w8_int operator<=(w8_int a, w8_int b) { return (a < b) | (a == b); }
#endif


#if defined(SIMD_AVX_512)
static uint8 operator==(w8_float a, w8_float b) { return _mm256_cmp_ps_mask(a, b, _CMP_EQ_OQ); }
static uint8 operator!=(w8_float a, w8_float b) { return _mm256_cmp_ps_mask(a, b, _CMP_NEQ_OQ); }
static uint8 operator>(w8_float a, w8_float b) { return _mm256_cmp_ps_mask(a, b, _CMP_GT_OQ); }
static uint8 operator>=(w8_float a, w8_float b) { return _mm256_cmp_ps_mask(a, b, _CMP_GE_OQ); }
static uint8 operator<(w8_float a, w8_float b) { return _mm256_cmp_ps_mask(a, b, _CMP_LT_OQ); }
static uint8 operator<=(w8_float a, w8_float b) { return _mm256_cmp_ps_mask(a, b, _CMP_LE_OQ); }
#else
static w8_float operator==(w8_float a, w8_float b) { return _mm256_cmp_ps(a, b, _CMP_EQ_OQ); }
static w8_float operator!=(w8_float a, w8_float b) { return _mm256_cmp_ps(a, b, _CMP_NEQ_OQ); }
static w8_float operator>(w8_float a, w8_float b) { return _mm256_cmp_ps(a, b, _CMP_GT_OQ); }
static w8_float operator>=(w8_float a, w8_float b) { return _mm256_cmp_ps(a, b, _CMP_GE_OQ); }
static w8_float operator<(w8_float a, w8_float b) { return _mm256_cmp_ps(a, b, _CMP_LT_OQ); }
static w8_float operator<=(w8_float a, w8_float b) { return _mm256_cmp_ps(a, b, _CMP_LE_OQ); }
#endif

static w8_float operator>>(w8_float a, int b) { return reinterpret(reinterpret(a) >> b); }
static w8_float& operator>>=(w8_float& a, int b) { a = a >> b; return a; }
static w8_float operator<<(w8_float a, int b) { return reinterpret(reinterpret(a) << b); }
static w8_float& operator<<=(w8_float& a, int b) { a = a << b; return a; }

static w8_float operator-(w8_float a) { return _mm256_xor_ps(a, reinterpret(w8_int(1 << 31))); }




static float addElements(w8_float a) { __m256 aa = _mm256_hadd_ps(a, a); aa = _mm256_hadd_ps(aa, aa); return aa.m256_f32[0] + aa.m256_f32[4]; }

static w8_float fmadd(w8_float a, w8_float b, w8_float c) { return _mm256_fmadd_ps(a, b, c); }
static w8_float fmsub(w8_float a, w8_float b, w8_float c) { return _mm256_fmsub_ps(a, b, c); }

static w8_float sqrt(w8_float a) { return _mm256_sqrt_ps(a); }
static w8_float rsqrt(w8_float a) { return _mm256_rsqrt_ps(a); }

static int toBitMask(w8_float a) { return _mm256_movemask_ps(a); }
static int toBitMask(w8_int a) { return toBitMask(reinterpret(a)); }

#if defined(SIMD_AVX_512)
static w8_float ifThen(uint8 cond, w8_float ifCase, w8_float elseCase) { return _mm256_mask_blend_ps(cond, elseCase, ifCase); }
static w8_int ifThen(uint8 cond, w8_int ifCase, w8_int elseCase) { return reinterpret(ifThen(cond, reinterpret(ifCase), reinterpret(elseCase))); }

static int toBitMask(uint8 a) { return a; }

static bool allTrue(uint8 a) { return a == (1 << 8) - 1; }
static bool allFalse(uint8 a) { return a == 0; }
static bool anyTrue(uint8 a) { return a > 0; }
static bool anyFalse(uint8 a) { return !allTrue(a); }
#else
static w8_float ifThen(w8_float cond, w8_float ifCase, w8_float elseCase) { return _mm256_blendv_ps(elseCase, ifCase, cond); }
static w8_int ifThen(w8_int cond, w8_int ifCase, w8_int elseCase) { return reinterpret(ifThen(reinterpret(cond), reinterpret(ifCase), reinterpret(elseCase))); }

static bool allTrue(w8_float a) { return toBitMask(a) == (1 << 8) - 1; }
static bool allFalse(w8_float a) { return toBitMask(a) == 0; }
static bool anyTrue(w8_float a) { return toBitMask(a) > 0; }
static bool anyFalse(w8_float a) { return !allTrue(a); }

static bool allTrue(w8_int a) { return allTrue(reinterpret(a)); }
static bool allFalse(w8_int a) { return allFalse(reinterpret(a)); }
static bool anyTrue(w8_int a) { return anyTrue(reinterpret(a)); }
static bool anyFalse(w8_int a) { return anyFalse(reinterpret(a)); }
#endif


static w8_float abs(w8_float a) { w8_float result = andNot(-0.f, a); return result; }
static w8_float floor(w8_float a) { return _mm256_floor_ps(a); }
static w8_float minimum(w8_float a, w8_float b) { return _mm256_min_ps(a, b); }
static w8_float maximum(w8_float a, w8_float b) { return _mm256_max_ps(a, b); }

static w8_float lerp(w8_float l, w8_float u, w8_float t) { return fmadd(t, u - l, l); }
static w8_float inverseLerp(w8_float l, w8_float u, w8_float v) { return (v - l) / (u - l); }
static w8_float remap(w8_float v, w8_float oldL, w8_float oldU, w8_float newL, w8_float newU) { return lerp(newL, newU, inverseLerp(oldL, oldU, v)); }
static w8_float clamp(w8_float v, w8_float l, w8_float u) { return minimum(u, maximum(l, v)); }
static w8_float clamp01(w8_float v) { return clamp(v, 0.f, 1.f); }

static w8_float signOf(w8_float f) { w8_float z = w8_float::zero(); return ifThen(f < z, w8_float(-1), ifThen(f == z, z, w8_float(1))); }
static w8_float signbit(w8_float f) { return (f & -0.f) >> 31; }

static w8_float cos(w8_float x) { return cosInternal(x); }
static w8_float sin(w8_float x) { return sinInternal(x); }
static w8_float exp2(w8_float x) { return exp2Internal<w8_float, w8_int>(x); }
static w8_float log2(w8_float x) { return log2Internal<w8_float, w8_int>(x); }
static w8_float pow(w8_float x, w8_float y) { return powInternal<w8_float, w8_int>(x, y); }
static w8_float exp(w8_float x) { return expInternal<w8_float, w8_int>(x); }
static w8_float tanh(w8_float x) { return tanhInternal(x); }
static w8_float atan(w8_float x) { return atanInternal<w8_float, w8_int>(x); }
static w8_float atan2(w8_float y, w8_float x) { return atan2Internal<w8_float, w8_int>(y, x); }
static w8_float acos(w8_float x) { return acosInternal(x); }

static w8_float concat(w4_float a, w4_float b)
{
	return _mm256_insertf128_ps(_mm256_castps128_ps256(a), b, 1);
}

static w8_int concat(w4_int a, w4_int b)
{
	return _mm256_inserti128_si256(_mm256_castsi128_si256(a), b, 1);
}

static w8_float concatLow(w8_float a, w8_float b)
{
	return _mm256_permute2f128_ps(a, b, 0 | (0 << 4));
}

static w8_int concatLow(w8_int a, w8_int b)
{
	return _mm256_permute2x128_si256(a, b, 0 | (0 << 4));
}

static w8_int fillWithFirstLane(w8_int a)
{
	w8_int first = _mm256_shuffle_epi32(a, _MM_SHUFFLE(0, 0, 0, 0));
	first = concatLow(first, first);
	return first;
}

static void transpose32(w8_float& out0, w8_float& out1, w8_float& out2, w8_float& out3)
{
	w8_float t0 = _mm256_unpacklo_ps(out0, out1);
	w8_float t1 = _mm256_unpacklo_ps(out2, out3);
	w8_float t2 = _mm256_unpackhi_ps(out0, out1);
	w8_float t3 = _mm256_unpackhi_ps(out2, out3);
	out0 = _mm256_shuffle_ps(t0, t1, _MM_SHUFFLE(1, 0, 1, 0));
	out1 = _mm256_shuffle_ps(t0, t1, _MM_SHUFFLE(3, 2, 3, 2));
	out2 = _mm256_shuffle_ps(t2, t3, _MM_SHUFFLE(1, 0, 1, 0));
	out3 = _mm256_shuffle_ps(t2, t3, _MM_SHUFFLE(3, 2, 3, 2));
}

static void transpose(w8_float& out0, w8_float& out1, w8_float& out2, w8_float& out3, w8_float& out4, w8_float& out5, w8_float& out6, w8_float& out7)
{
	w8_float tmp0 = reinterpret(_mm256_permute2x128_si256(reinterpret(out0), reinterpret(out4), 0 | (2 << 4)));
	w8_float tmp1 = reinterpret(_mm256_permute2x128_si256(reinterpret(out1), reinterpret(out5), 0 | (2 << 4)));
	w8_float tmp2 = reinterpret(_mm256_permute2x128_si256(reinterpret(out2), reinterpret(out6), 0 | (2 << 4)));
	w8_float tmp3 = reinterpret(_mm256_permute2x128_si256(reinterpret(out3), reinterpret(out7), 0 | (2 << 4)));
	w8_float tmp4 = reinterpret(_mm256_permute2x128_si256(reinterpret(out0), reinterpret(out4), 1 | (3 << 4)));
	w8_float tmp5 = reinterpret(_mm256_permute2x128_si256(reinterpret(out1), reinterpret(out5), 1 | (3 << 4)));
	w8_float tmp6 = reinterpret(_mm256_permute2x128_si256(reinterpret(out2), reinterpret(out6), 1 | (3 << 4)));
	w8_float tmp7 = reinterpret(_mm256_permute2x128_si256(reinterpret(out3), reinterpret(out7), 1 | (3 << 4)));

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
	w8_float& out0, w8_float& out1, w8_float& out2, w8_float& out3)
{
	const uint32 strideInFloats = stride / sizeof(float);

	w4_float tmp0(baseAddress + strideInFloats * indices[0]);
	w4_float tmp1(baseAddress + strideInFloats * indices[1]);
	w4_float tmp2(baseAddress + strideInFloats * indices[2]);
	w4_float tmp3(baseAddress + strideInFloats * indices[3]);
	w4_float tmp4(baseAddress + strideInFloats * indices[4]);
	w4_float tmp5(baseAddress + strideInFloats * indices[5]);
	w4_float tmp6(baseAddress + strideInFloats * indices[6]);
	w4_float tmp7(baseAddress + strideInFloats * indices[7]);

	out0 = concat(tmp0, tmp4);
	out1 = concat(tmp1, tmp5);
	out2 = concat(tmp2, tmp6);
	out3 = concat(tmp3, tmp7);

	transpose32(out0, out1, out2, out3);
}

static void load8(const float* baseAddress, const uint16* indices, uint32 stride,
	w8_float& out0, w8_float& out1, w8_float& out2, w8_float& out3, w8_float& out4, w8_float& out5, w8_float& out6, w8_float& out7)
{
	const uint32 strideInFloats = stride / sizeof(float);

	out0 = w8_float(baseAddress + strideInFloats * indices[0]);
	out1 = w8_float(baseAddress + strideInFloats * indices[1]);
	out2 = w8_float(baseAddress + strideInFloats * indices[2]);
	out3 = w8_float(baseAddress + strideInFloats * indices[3]);
	out4 = w8_float(baseAddress + strideInFloats * indices[4]);
	out5 = w8_float(baseAddress + strideInFloats * indices[5]);
	out6 = w8_float(baseAddress + strideInFloats * indices[6]);
	out7 = w8_float(baseAddress + strideInFloats * indices[7]);

	transpose(out0, out1, out2, out3, out4, out5, out6, out7);
}

static void store8(float* baseAddress, const uint16* indices, uint32 stride,
	w8_float in0, w8_float in1, w8_float in2, w8_float in3, w8_float in4, w8_float in5, w8_float in6, w8_float in7)
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

struct w16_float
{
	__m512 f;

	w16_float() {}
	w16_float(float f_) { f = _mm512_set1_ps(f_); }
	w16_float(__m512 f_) { f = f_; }
	w16_float(float a, float b, float c, float d, float e, float f, float g, float h, float i, float j, float k, float l, float m, float n, float o, float p) { this->f = _mm512_setr_ps(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p); }
	w16_float(const float* f_) { f = _mm512_loadu_ps(f_); }

	w16_float(const float* baseAddress, __m512i indices) { f = _mm512_i32gather_ps(indices, baseAddress, 4); }
	w16_float(const float* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l, int m, int n, int o, int p) : w16_float(baseAddress, _mm512_setr_epi32(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)) {}

	operator __m512() { return f; }

	void store(float* f_) const { _mm512_storeu_ps(f_, f); }

	void scatter(float* baseAddress, __m512i indices) const { _mm512_i32scatter_ps(baseAddress, indices, f, 4); }
	void scatter(float* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l, int m, int n, int o, int p) const { scatter(baseAddress, _mm512_setr_epi32(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)); }
};

struct w16_int
{
	__m512i i;

	w16_int() {}
	w16_int(int i_) { i = _mm512_set1_epi32(i_); }
	w16_int(__m512i i_) { i = i_; }
	w16_int(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l, int m, int n, int o, int p) { this->i = _mm512_setr_epi32(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p); }
	w16_int(const int* i_) { i = _mm512_loadu_epi32(i_); }

	w16_int(const int* baseAddress, __m512i indices) { i = _mm512_i32gather_epi32(indices, baseAddress, 4); }
	w16_int(const int* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l, int m, int n, int o, int p) : w16_int(baseAddress, _mm512_setr_epi32(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)) {}

	operator __m512i() { return i; }

	void store(int* i_) const { _mm512_storeu_epi32(i_, i); }
};

static w16_float truex16() { const float nnan = (const float&)0xFFFFFFFF; return nnan; }
static w16_float zerox16() { return _mm512_setzero_ps(); }

static w16_float convert(w16_int i) { return _mm512_cvtepi32_ps(i); }
static w16_int convert(w16_float f) { return _mm512_cvtps_epi32(f); }
static w16_float reinterpret(w16_int i) { return _mm512_castsi512_ps(i); }
static w16_int reinterpret(w16_float f) { return _mm512_castps_si512(f); }


// Int operators.
static w16_int andNot(w16_int a, w16_int b) { return _mm512_andnot_si512(a, b); }

static w16_int operator+(w16_int a, w16_int b) { return _mm512_add_epi32(a, b); }
static w16_int& operator+=(w16_int& a, w16_int b) { a = a + b; return a; }
static w16_int operator-(w16_int a, w16_int b) { return _mm512_sub_epi32(a, b); }
static w16_int& operator-=(w16_int& a, w16_int b) { a = a - b; return a; }
static w16_int operator*(w16_int a, w16_int b) { return _mm512_mul_epi32(a, b); }
static w16_int& operator*=(w16_int& a, w16_int b) { a = a * b; return a; }
static w16_int operator/(w16_int a, w16_int b) { return _mm512_div_epi32(a, b); }
static w16_int& operator/=(w16_int& a, w16_int b) { a = a / b; return a; }
static w16_int operator&(w16_int a, w16_int b) { return _mm512_and_epi32(a, b); }
static w16_int& operator&=(w16_int& a, w16_int b) { a = a & b; return a; }
static w16_int operator|(w16_int a, w16_int b) { return _mm512_or_epi32(a, b); }
static w16_int& operator|=(w16_int& a, w16_int b) { a = a | b; return a; }
static w16_int operator^(w16_int a, w16_int b) { return _mm512_xor_epi32(a, b); }
static w16_int& operator^=(w16_int& a, w16_int b) { a = a ^ b; return a; }

static w16_int operator~(w16_int a) { a = andNot(a, reinterpret(truex16())); return a; }

static uint16 operator==(w16_int a, w16_int b) { return _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_EQ); }
static uint16 operator!=(w16_int a, w16_int b) { return _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_NE); }
static uint16 operator>(w16_int a, w16_int b) { return _mm512_cmp_epi32_mask(b, a, _MM_CMPINT_LT); }
static uint16 operator>=(w16_int a, w16_int b) { return _mm512_cmp_epi32_mask(b, a, _MM_CMPINT_LE); }
static uint16 operator<(w16_int a, w16_int b) { return _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_LT); }
static uint16 operator<=(w16_int a, w16_int b) { return _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_LE); }


static w16_int operator>>(w16_int a, int b) { return _mm512_srli_epi32(a, b); }
static w16_int& operator>>=(w16_int& a, int b) { a = a >> b; return a; }
static w16_int operator<<(w16_int a, int b) { return _mm512_slli_epi32(a, b); }
static w16_int& operator<<=(w16_int& a, int b) { a = a << b; return a; }

static w16_int operator-(w16_int a) { return _mm512_sub_epi32(_mm512_setzero_si512(), a); }



// Float operators.
static w16_float andNot(w16_float a, w16_float b) { return _mm512_andnot_ps(a, b); }

static w16_float operator+(w16_float a, w16_float b) { return _mm512_add_ps(a, b); }
static w16_float& operator+=(w16_float& a, w16_float b) { a = a + b; return a; }
static w16_float operator-(w16_float a, w16_float b) { return _mm512_sub_ps(a, b); }
static w16_float& operator-=(w16_float& a, w16_float b) { a = a - b; return a; }
static w16_float operator*(w16_float a, w16_float b) { return _mm512_mul_ps(a, b); }
static w16_float& operator*=(w16_float& a, w16_float b) { a = a * b; return a; }
static w16_float operator/(w16_float a, w16_float b) { return _mm512_div_ps(a, b); }
static w16_float& operator/=(w16_float& a, w16_float b) { a = a / b; return a; }
static w16_float operator&(w16_float a, w16_float b) { return _mm512_and_ps(a, b); }
static w16_float& operator&=(w16_float& a, w16_float b) { a = a & b; return a; }
static w16_float operator|(w16_float a, w16_float b) { return _mm512_or_ps(a, b); }
static w16_float& operator|=(w16_float& a, w16_float b) { a = a | b; return a; }
static w16_float operator^(w16_float a, w16_float b) { return _mm512_xor_ps(a, b); }
static w16_float& operator^=(w16_float& a, w16_float b) { a = a ^ b; return a; }

static w16_float operator~(w16_float a) { a = andNot(a, truex16()); return a; }

static uint16 operator==(w16_float a, w16_float b) { return _mm512_cmp_ps_mask(a, b, _CMP_EQ_OQ); }
static uint16 operator!=(w16_float a, w16_float b) { return _mm512_cmp_ps_mask(a, b, _CMP_NEQ_OQ); }
static uint16 operator>(w16_float a, w16_float b) { return _mm512_cmp_ps_mask(a, b, _CMP_GT_OQ); }
static uint16 operator>=(w16_float a, w16_float b) { return _mm512_cmp_ps_mask(a, b, _CMP_GE_OQ); }
static uint16 operator<(w16_float a, w16_float b) { return _mm512_cmp_ps_mask(a, b, _CMP_LT_OQ); }
static uint16 operator<=(w16_float a, w16_float b) { return _mm512_cmp_ps_mask(a, b, _CMP_LE_OQ); }


static w16_float operator>>(w16_float a, int b) { return reinterpret(reinterpret(a) >> b); }
static w16_float& operator>>=(w16_float& a, int b) { a = a >> b; return a; }
static w16_float operator<<(w16_float a, int b) { return reinterpret(reinterpret(a) << b); }
static w16_float& operator<<=(w16_float& a, int b) { a = a << b; return a; }

static w16_float operator-(w16_float a) { return _mm512_xor_ps(a, reinterpret(w16_int(1 << 31))); }




static float addElements(w16_float a) { return _mm512_reduce_add_ps(a); }

static w16_float fmadd(w16_float a, w16_float b, w16_float c) { return _mm512_fmadd_ps(a, b, c); }
static w16_float fmsub(w16_float a, w16_float b, w16_float c) { return _mm512_fmsub_ps(a, b, c); }

static w16_float sqrt(w16_float a) { return _mm512_sqrt_ps(a); }
static w16_float rsqrt(w16_float a) { return 1.f / _mm512_sqrt_ps(a); }

static w16_float ifThen(uint16 cond, w16_float ifCase, w16_float elseCase) { return _mm512_mask_blend_ps(cond, elseCase, ifCase); }
static w16_int ifThen(uint16 cond, w16_int ifCase, w16_int elseCase) { return reinterpret(ifThen(cond, reinterpret(ifCase), reinterpret(elseCase))); }

static bool allTrue(uint16 a) { return a == (1 << 16) - 1; }
static bool allFalse(uint16 a) { return a == 0; }
static bool anyTrue(uint16 a) { return a > 0; }
static bool anyFalse(uint16 a) { return !allTrue(a); }


static w16_float abs(w16_float a) { w16_float result = andNot(-0.f, a); return result; }
static w16_float floor(w16_float a) { return _mm512_floor_ps(a); }
static w16_float minimum(w16_float a, w16_float b) { return _mm512_min_ps(a, b); }
static w16_float maximum(w16_float a, w16_float b) { return _mm512_max_ps(a, b); }

static w16_float lerp(w16_float l, w16_float u, w16_float t) { return fmadd(t, u - l, l); }
static w16_float inverseLerp(w16_float l, w16_float u, w16_float v) { return (v - l) / (u - l); }
static w16_float remap(w16_float v, w16_float oldL, w16_float oldU, w16_float newL, w16_float newU) { return lerp(newL, newU, inverseLerp(oldL, oldU, v)); }
static w16_float clamp(w16_float v, w16_float l, w16_float u) { return minimum(u, maximum(l, v)); }
static w16_float clamp01(w16_float v) { return clamp(v, 0.f, 1.f); }

static w16_float signOf(w16_float f) { return ifThen(f < 0.f, w16_float(-1), ifThen(f == 0.f, zerox16(), w16_float(1))); }
static w16_float signbit(w16_float f) { return (f & -0.f) >> 31; }

static w16_float cos(w16_float x) { return cosInternal(x); }
static w16_float sin(w16_float x) { return sinInternal(x); }
static w16_float exp2(w16_float x) { return exp2Internal<w16_float, w16_int>(x); }
static w16_float log2(w16_float x) { return log2Internal<w16_float, w16_int>(x); }
static w16_float pow(w16_float x, w16_float y) { return powInternal<w16_float, w16_int>(x, y); }
static w16_float exp(w16_float x) { return expInternal<w16_float, w16_int>(x); }
static w16_float tanh(w16_float x) { return tanhInternal(x); }
static w16_float atan(w16_float x) { return atanInternal<w16_float, w16_int>(x); }
static w16_float atan2(w16_float y, w16_float x) { return atan2Internal<w16_float, w16_int>(y, x); }
static w16_float acos(w16_float x) { return acosInternal(x); }


#endif








