#pragma once


#include <emmintrin.h>
#include <immintrin.h>

#define SIMD_SSE_2 // Not quite correct but I think it is safe to assume that every processor has SSE2.

#if __AVX__
#if __AVX512BW__
#define SIMD_AVX_512
#define SIMD_AVX_2
#elif __AVX2__
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


#if defined(SIMD_SSE_2)

typedef __m128 cmpx4;

struct floatx4
{
	__m128 f;

	floatx4(float f_) { f = _mm_set1_ps(f_); }
	floatx4(__m128 f_) { f = f_; }
	floatx4(float* f_) { f = _mm_loadu_ps(f_); }

	operator __m128() { return f; }

	void store(float* f_) { _mm_storeu_ps(f_, f); }
};

struct intx4
{
	__m128i i;

	intx4(int i_) { i = _mm_set1_epi32(i_); }
	intx4(__m128i i_) { i = i_; }
	//intx4(int* i_) { i = _mm_loadu_epi32(i_); } // TODO: Give non-AVX512 alternative.

	operator __m128i() { return i; }

	//void store(int* i_) { _mm_storeu_epi32(i_, i); }
};

static floatx4 truex4() { const float nnan = (const float&)0xFFFFFFFF; return nnan; }
static floatx4 zerox4() { return _mm_setzero_ps(); }

static floatx4 convertIntToFloat(intx4 i) { return _mm_cvtepi32_ps(i); }
static intx4 convertFloatToInt(floatx4 f) { return _mm_cvtps_epi32(f); }
static floatx4 reinterpretIntAsFloat(intx4 i) { return _mm_castsi128_ps(i); }
static intx4 reinterpretFloatAsInt(floatx4 f) { return _mm_castps_si128(f); }


// Int operators.
static intx4 andNot(intx4 a, intx4 b) { intx4 result = { _mm_andnot_si128(a, b) }; return result; }

static intx4 operator+(intx4 a, intx4 b) { intx4 result = { _mm_add_epi32(a, b) }; return result; }
static intx4& operator+=(intx4& a, intx4 b) { a = a + b; return a; }
static intx4 operator-(intx4 a, intx4 b) { intx4 result = { _mm_sub_epi32(a, b) }; return result; }
static intx4& operator-=(intx4& a, intx4 b) { a = a - b; return a; }
static intx4 operator*(intx4 a, intx4 b) { intx4 result = { _mm_mul_epi32(a, b) }; return result; }
static intx4& operator*=(intx4& a, intx4 b) { a = a * b; return a; }
static intx4 operator/(intx4 a, intx4 b) { intx4 result = { _mm_div_epi32(a, b) }; return result; }
static intx4& operator/=(intx4& a, intx4 b) { a = a / b; return a; }
static intx4 operator&(intx4 a, intx4 b) { intx4 result = { _mm_and_si128(a, b) }; return result; }
static intx4& operator&=(intx4& a, intx4 b) { a = a & b; return a; }
static intx4 operator|(intx4 a, intx4 b) { intx4 result = { _mm_or_si128(a, b) }; return result; }
static intx4& operator|=(intx4& a, intx4 b) { a = a | b; return a; }
static intx4 operator^(intx4 a, intx4 b) { intx4 result = { _mm_xor_si128(a, b) }; return result; }
static intx4& operator^=(intx4& a, intx4 b) { a = a ^ b; return a; }

static intx4 operator~(intx4 a) { a = andNot(a, reinterpretFloatAsInt(truex4())); return a; }

static intx4 operator>>(intx4 a, int b) { intx4 result = { _mm_srli_epi32(a, b) }; return result; }
static intx4& operator>>=(intx4& a, int b) { a = a >> b; return a; }
static intx4 operator<<(intx4 a, int b) { intx4 result = { _mm_slli_epi32(a, b) }; return result; }
static intx4& operator<<=(intx4& a, int b) { a = a << b; return a; }

static intx4 operator-(intx4 a) { intx4 result = { _mm_sub_epi32(_mm_setzero_si128(), a) }; return result; }



// Float operators.
static floatx4 andNot(floatx4 a, floatx4 b) { floatx4 result = { _mm_andnot_ps(a, b) }; return result; }

static floatx4 operator+(floatx4 a, floatx4 b) { floatx4 result = { _mm_add_ps(a, b) }; return result; }
static floatx4& operator+=(floatx4& a, floatx4 b) { a = a + b; return a; }
static floatx4 operator-(floatx4 a, floatx4 b) { floatx4 result = { _mm_sub_ps(a, b) }; return result; }
static floatx4& operator-=(floatx4& a, floatx4 b) { a = a - b; return a; }
static floatx4 operator*(floatx4 a, floatx4 b) { floatx4 result = { _mm_mul_ps(a, b) }; return result; }
static floatx4& operator*=(floatx4& a, floatx4 b) { a = a * b; return a; }
static floatx4 operator/(floatx4 a, floatx4 b) { floatx4 result = { _mm_div_ps(a, b) }; return result; }
static floatx4& operator/=(floatx4& a, floatx4 b) { a = a / b; return a; }
static floatx4 operator&(floatx4 a, floatx4 b) { floatx4 result = { _mm_and_ps(a, b) }; return result; }
static floatx4& operator&=(floatx4& a, floatx4 b) { a = a & b; return a; }
static floatx4 operator|(floatx4 a, floatx4 b) { floatx4 result = { _mm_or_ps(a, b) }; return result; }
static floatx4& operator|=(floatx4& a, floatx4 b) { a = a | b; return a; }
static floatx4 operator^(floatx4 a, floatx4 b) { floatx4 result = { _mm_xor_ps(a, b) }; return result; }
static floatx4& operator^=(floatx4& a, floatx4 b) { a = a ^ b; return a; }

static floatx4 operator~(floatx4 a) { a = andNot(a, truex4()); return a; }


static cmpx4 operator==(intx4 a, intx4 b) { cmpx4 result = reinterpretIntAsFloat(_mm_cmpeq_epi32(a, b)); return result; }
static cmpx4 operator!=(intx4 a, intx4 b) { cmpx4 result = ~(a == b); return result; }
static cmpx4 operator>(intx4 a, intx4 b) { cmpx4 result = reinterpretIntAsFloat(_mm_cmpgt_epi32(a, b)); return result; }
static cmpx4 operator>=(intx4 a, intx4 b) { cmpx4 result = (a > b) | (a == b); return result; }
static cmpx4 operator<(intx4 a, intx4 b) { cmpx4 result = reinterpretIntAsFloat(_mm_cmplt_epi32(a, b)); return result; }
static cmpx4 operator<=(intx4 a, intx4 b) { cmpx4 result = (a < b) | (a == b); return result; }

static cmpx4 operator==(floatx4 a, floatx4 b) { cmpx4 result = { _mm_cmpeq_ps(a, b) }; return result; }
static cmpx4 operator!=(floatx4 a, floatx4 b) { cmpx4 result = { _mm_cmpneq_ps(a, b) }; return result; }
static cmpx4 operator>(floatx4 a, floatx4 b) { cmpx4 result = { _mm_cmpgt_ps(a, b) }; return result; }
static cmpx4 operator>=(floatx4 a, floatx4 b) { cmpx4 result = { _mm_cmpge_ps(a, b) }; return result; }
static cmpx4 operator<(floatx4 a, floatx4 b) { cmpx4 result = { _mm_cmplt_ps(a, b) }; return result; }
static cmpx4 operator<=(floatx4 a, floatx4 b) { cmpx4 result = { _mm_cmple_ps(a, b) }; return result; }

static floatx4 operator>>(floatx4 a, int b) { return reinterpretIntAsFloat(reinterpretFloatAsInt(a) >> b); }
static floatx4& operator>>=(floatx4& a, int b) { a = a >> b; return a; }
static floatx4 operator<<(floatx4 a, int b) { return reinterpretIntAsFloat(reinterpretFloatAsInt(a) << b); }
static floatx4& operator<<=(floatx4& a, int b) { a = a << b; return a; }

static floatx4 operator-(floatx4 a) { floatx4 result = { _mm_xor_ps(a, _mm_set1_ps(-0)) }; return result; }




static float addElements(floatx4 a) { __m128 aa = _mm_hadd_ps(a, a); aa = _mm_hadd_ps(aa, aa); return aa.m128_f32[0]; }

static floatx4 fmadd(floatx4 a, floatx4 b, floatx4 c) { floatx4 result = { _mm_fmadd_ps(a, b, c) }; return result; }
static floatx4 fmsub(floatx4 a, floatx4 b, floatx4 c) { floatx4 result = { _mm_fmsub_ps(a, b, c) }; return result; }

static floatx4 sqrt(floatx4 a) { floatx4 result = { _mm_sqrt_ps(a) }; return result; }
static floatx4 rsqrt(floatx4 a) { floatx4 result = { _mm_rsqrt_ps(a) }; return result; }

static floatx4 ifThen(floatx4 cond, floatx4 ifCase, floatx4 elseCase) { return andNot(cond, elseCase) | (cond & ifCase); }

static int toBitMask(floatx4 a) { int result = _mm_movemask_ps(a); return result; }
static bool allTrue(floatx4 a) { return toBitMask(a) == (1 << 4) - 1; }
static bool allFalse(floatx4 a) { return toBitMask(a) == 0; }
static bool anyTrue(floatx4 a) { return toBitMask(a) > 0; }
static bool anyFalse(floatx4 a) { return !allTrue(a); }

static floatx4 abs(floatx4 a) { floatx4 result = andNot(-0.f, a); return result; }
static floatx4 floor(floatx4 a) { floatx4 result = { _mm_floor_ps(a) }; return result; }
static floatx4 round(floatx4 a) { floatx4 result = { _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC) }; return result; }
static floatx4 minimum(floatx4 a, floatx4 b) { floatx4 result = { _mm_min_ps(a, b) }; return result; }
static floatx4 maximum(floatx4 a, floatx4 b) { floatx4 result = { _mm_max_ps(a, b) }; return result; }

static floatx4 lerp(floatx4 l, floatx4 u, floatx4 t) { return l + t * (u - l); }
static floatx4 inverseLerp(floatx4 l, floatx4 u, floatx4 v) { return (v - l) / (u - l); }
static floatx4 remap(floatx4 v, floatx4 oldL, floatx4 oldU, floatx4 newL, floatx4 newU) { return lerp(newL, newU, inverseLerp(oldL, oldU, v)); }
static floatx4 clamp(floatx4 v, floatx4 l, floatx4 u) { return minimum(u, maximum(l, v)); }
static floatx4 clamp01(floatx4 v) { return clamp(v, 0.f, 1.f); }

static floatx4 signOf(floatx4 f) { return ifThen(f < 0.f, floatx4(-1), ifThen(f == 0.f, zerox4(), floatx4(1))); }
static floatx4 signbit(floatx4 f) { return (f & -0.f) >> 31; }


static floatx4 exp2(floatx4 x)
{
	x = minimum(x, 129.00000f);
	x = maximum(x, -126.99999f);

	intx4 ipart = convertFloatToInt(x - 0.5f);
	floatx4 fpart = x - convertIntToFloat(ipart);
	floatx4 expipart = reinterpretIntAsFloat((ipart + 127) << 23);
	floatx4 expfpart = POLY3(fpart, 9.9992520e-1f, 6.9583356e-1f, 2.2606716e-1f, 7.8024521e-2f);
	return expipart * expfpart;
}

static floatx4 log2(floatx4 x)
{
	intx4 exp = 0x7F800000;
	intx4 mant = 0x007FFFFF;

	floatx4 one = 1;
	intx4 i = reinterpretFloatAsInt(x);

	floatx4 e = convertIntToFloat(((i & exp) >> 23) - 127);
	floatx4 m = reinterpretIntAsFloat(i & mant) | one;
	floatx4 p = POLY4(m, 2.8882704548164776201f, -2.52074962577807006663f, 1.48116647521213171641f, -0.465725644288844778798f, 0.0596515482674574969533f);

	return fmadd(p, m - one, e);
}

static floatx4 pow(floatx4 x, floatx4 y)
{
	return exp2(log2(x) * y);
}

#endif

#if defined(SIMD_AVX_2)

#if defined(SIMD_AVX_512)
typedef __mmask8 cmpx8;
#else
typedef __m256 cmpx8;
#endif

struct floatx8
{
	__m256 f;

	floatx8(float f_) { f = _mm256_set1_ps(f_); }
	floatx8(__m256 f_) { f = f_; }
	floatx8(float* f_) { f = _mm256_loadu_ps(f_); }

	operator __m256() { return f; }

	void store(float* f_) { _mm256_storeu_ps(f_, f); }
};

struct intx8
{
	__m256i i;

	intx8(int i_) { i = _mm256_set1_epi32(i_); }
	intx8(__m256i i_) { i = i_; }
	intx8(int* i_) { i = _mm256_loadu_epi32(i_); }

	operator __m256i() { return i; }

	void store(int* i_) { _mm256_storeu_epi32(i_, i); }
};

static floatx8 truex8() { const float nnan = (const float&)0xFFFFFFFF; return nnan; }
static floatx8 zerox8() { return _mm256_setzero_ps(); }

static floatx8 convertIntToFloat(intx8 i) { return _mm256_cvtepi32_ps(i); }
static intx8 convertFloatToInt(floatx8 f) { return _mm256_cvtps_epi32(f); }
static floatx8 reinterpretIntAsFloat(intx8 i) { return _mm256_castsi256_ps(i); }
static intx8 reinterpretFloatAsInt(floatx8 f) { return _mm256_castps_si256(f); }


// Int operators.
static intx8 andNot(intx8 a, intx8 b) { intx8 result = { _mm256_andnot_si256(a, b) }; return result; }

static intx8 operator+(intx8 a, intx8 b) { intx8 result = { _mm256_add_epi32(a, b) }; return result; }
static intx8& operator+=(intx8& a, intx8 b) { a = a + b; return a; }
static intx8 operator-(intx8 a, intx8 b) { intx8 result = { _mm256_sub_epi32(a, b) }; return result; }
static intx8& operator-=(intx8& a, intx8 b) { a = a - b; return a; }
static intx8 operator*(intx8 a, intx8 b) { intx8 result = { _mm256_mul_epi32(a, b) }; return result; }
static intx8& operator*=(intx8& a, intx8 b) { a = a * b; return a; }
static intx8 operator/(intx8 a, intx8 b) { intx8 result = { _mm256_div_epi32(a, b) }; return result; }
static intx8& operator/=(intx8& a, intx8 b) { a = a / b; return a; }
static intx8 operator&(intx8 a, intx8 b) { intx8 result = { _mm256_and_si256(a, b) }; return result; }
static intx8& operator&=(intx8& a, intx8 b) { a = a & b; return a; }
static intx8 operator|(intx8 a, intx8 b) { intx8 result = { _mm256_or_si256(a, b) }; return result; }
static intx8& operator|=(intx8& a, intx8 b) { a = a | b; return a; }
static intx8 operator^(intx8 a, intx8 b) { intx8 result = { _mm256_xor_si256(a, b) }; return result; }
static intx8& operator^=(intx8& a, intx8 b) { a = a ^ b; return a; }

static intx8 operator~(intx8 a) { a = andNot(a, reinterpretFloatAsInt(truex8())); return a; }

static intx8 operator>>(intx8 a, int b) { intx8 result = { _mm256_srli_epi32(a, b) }; return result; }
static intx8& operator>>=(intx8& a, int b) { a = a >> b; return a; }
static intx8 operator<<(intx8 a, int b) { intx8 result = { _mm256_slli_epi32(a, b) }; return result; }
static intx8& operator<<=(intx8& a, int b) { a = a << b; return a; }

static intx8 operator-(intx8 a) { intx8 result = { _mm256_sub_epi32(_mm256_setzero_si256(), a) }; return result; }



// Float operators.
static floatx8 andNot(floatx8 a, floatx8 b) { floatx8 result = { _mm256_andnot_ps(a, b) }; return result; }

static floatx8 operator+(floatx8 a, floatx8 b) { floatx8 result = { _mm256_add_ps(a, b) }; return result; }
static floatx8& operator+=(floatx8& a, floatx8 b) { a = a + b; return a; }
static floatx8 operator-(floatx8 a, floatx8 b) { floatx8 result = { _mm256_sub_ps(a, b) }; return result; }
static floatx8& operator-=(floatx8& a, floatx8 b) { a = a - b; return a; }
static floatx8 operator*(floatx8 a, floatx8 b) { floatx8 result = { _mm256_mul_ps(a, b) }; return result; }
static floatx8& operator*=(floatx8& a, floatx8 b) { a = a * b; return a; }
static floatx8 operator/(floatx8 a, floatx8 b) { floatx8 result = { _mm256_div_ps(a, b) }; return result; }
static floatx8& operator/=(floatx8& a, floatx8 b) { a = a / b; return a; }
static floatx8 operator&(floatx8 a, floatx8 b) { floatx8 result = { _mm256_and_ps(a, b) }; return result; }
static floatx8& operator&=(floatx8& a, floatx8 b) { a = a & b; return a; }
static floatx8 operator|(floatx8 a, floatx8 b) { floatx8 result = { _mm256_or_ps(a, b) }; return result; }
static floatx8& operator|=(floatx8& a, floatx8 b) { a = a | b; return a; }
static floatx8 operator^(floatx8 a, floatx8 b) { floatx8 result = { _mm256_xor_ps(a, b) }; return result; }
static floatx8& operator^=(floatx8& a, floatx8 b) { a = a ^ b; return a; }

static floatx8 operator~(floatx8 a) { a = andNot(a, truex8()); return a; }


#if defined(SIMD_AVX_512)
static cmpx8 operator==(intx8 a, intx8 b) { cmpx8 result = _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_EQ); return result; }
static cmpx8 operator!=(intx8 a, intx8 b) { cmpx8 result = _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_NE); return result; }
static cmpx8 operator>(intx8 a, intx8 b) { cmpx8 result = _mm256_cmp_epi32_mask(b, a, _MM_CMPINT_LT); return result; }
static cmpx8 operator>=(intx8 a, intx8 b) { cmpx8 result = _mm256_cmp_epi32_mask(b, a, _MM_CMPINT_LE); return result; }
static cmpx8 operator<(intx8 a, intx8 b) { cmpx8 result = _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_LT); return result; }
static cmpx8 operator<=(intx8 a, intx8 b) { cmpx8 result = _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_LE); return result; }
#else
static cmpx8 operator==(intx8 a, intx8 b) { cmpx8 result = reinterpretIntAsFloat(_mm256_cmpeq_epi32(a, b)); return result; }
static cmpx8 operator!=(intx8 a, intx8 b) { cmpx8 result = ~(a == b); return result; }
static cmpx8 operator>(intx8 a, intx8 b) { cmpx8 result = reinterpretIntAsFloat(_mm256_cmpgt_epi32(a, b)); return result; }
static cmpx8 operator>=(intx8 a, intx8 b) { cmpx8 result = (a > b) | (a == b); return result; }
static cmpx8 operator<(intx8 a, intx8 b) { cmpx8 result = reinterpretIntAsFloat(_mm256_cmpgt_epi32(b, a)); return result; }
static cmpx8 operator<=(intx8 a, intx8 b) { cmpx8 result = (a < b) | (a == b); return result; }
#endif


#if defined(SIMD_AVX_512)
static cmpx8 operator==(floatx8 a, floatx8 b) { cmpx8 result = _mm256_cmp_ps_mask(a, b, _CMP_EQ_OQ); return result; }
static cmpx8 operator!=(floatx8 a, floatx8 b) { cmpx8 result = _mm256_cmp_ps_mask(a, b, _CMP_NEQ_OQ); return result; }
static cmpx8 operator>(floatx8 a, floatx8 b) { cmpx8 result = _mm256_cmp_ps_mask(a, b, _CMP_GT_OQ); return result; }
static cmpx8 operator>=(floatx8 a, floatx8 b) { cmpx8 result = _mm256_cmp_ps_mask(a, b, _CMP_GE_OQ); return result; }
static cmpx8 operator<(floatx8 a, floatx8 b) { cmpx8 result = _mm256_cmp_ps_mask(a, b, _CMP_LT_OQ); return result; }
static cmpx8 operator<=(floatx8 a, floatx8 b) { cmpx8 result = _mm256_cmp_ps_mask(a, b, _CMP_LE_OQ); return result; }
#else
static cmpx8 operator==(floatx8 a, floatx8 b) { cmpx8 result = _mm256_cmp_ps(a, b, _CMP_EQ_OQ); return result; }
static cmpx8 operator!=(floatx8 a, floatx8 b) { cmpx8 result = _mm256_cmp_ps(a, b, _CMP_NEQ_OQ); return result; }
static cmpx8 operator>(floatx8 a, floatx8 b) { cmpx8 result = _mm256_cmp_ps(a, b, _CMP_GT_OQ); return result; }
static cmpx8 operator>=(floatx8 a, floatx8 b) { cmpx8 result = _mm256_cmp_ps(a, b, _CMP_GE_OQ); return result; }
static cmpx8 operator<(floatx8 a, floatx8 b) { cmpx8 result = _mm256_cmp_ps(a, b, _CMP_LT_OQ); return result; }
static cmpx8 operator<=(floatx8 a, floatx8 b) { cmpx8 result = _mm256_cmp_ps(a, b, _CMP_LE_OQ); return result; }
#endif

static floatx8 operator>>(floatx8 a, int b) { return reinterpretIntAsFloat(reinterpretFloatAsInt(a) >> b); }
static floatx8& operator>>=(floatx8& a, int b) { a = a >> b; return a; }
static floatx8 operator<<(floatx8 a, int b) { return reinterpretIntAsFloat(reinterpretFloatAsInt(a) << b); }
static floatx8& operator<<=(floatx8& a, int b) { a = a << b; return a; }

static floatx8 operator-(floatx8 a) { floatx8 result = { _mm256_xor_ps(a, _mm256_set1_ps(-0)) }; return result; }




static float addElements(floatx8 a) { __m256 aa = _mm256_hadd_ps(a, a); aa = _mm256_hadd_ps(aa, aa); return aa.m256_f32[0] + aa.m256_f32[4]; }

static floatx8 fmadd(floatx8 a, floatx8 b, floatx8 c) { floatx8 result = { _mm256_fmadd_ps(a, b, c) }; return result; }
static floatx8 fmsub(floatx8 a, floatx8 b, floatx8 c) { floatx8 result = { _mm256_fmsub_ps(a, b, c) }; return result; }

static floatx8 sqrt(floatx8 a) { floatx8 result = { _mm256_sqrt_ps(a) }; return result; }
static floatx8 rsqrt(floatx8 a) { floatx8 result = { _mm256_rsqrt_ps(a) }; return result; }

#if defined(SIMD_AVX_512)
static floatx8 ifThen(cmpx8 cond, floatx8 ifCase, floatx8 elseCase) { return _mm256_mask_blend_ps(cond, ifCase, elseCase); }

static bool allTrue(cmpx8 a) { return a == (1 << 8) - 1; }
static bool allFalse(cmpx8 a) { return a == 0; }
static bool anyTrue(cmpx8 a) { return a > 0; }
static bool anyFalse(cmpx8 a) { return !allTrue(a); }
#else
static floatx8 ifThen(cmpx8 cond, floatx8 ifCase, floatx8 elseCase) { return andNot(cond, elseCase) | (cond & ifCase); }

static int toBitMask(cmpx8 a) { int result = _mm256_movemask_ps(a); return result; }
static bool allTrue(cmpx8 a) { return toBitMask(a) == (1 << 8) - 1; }
static bool allFalse(cmpx8 a) { return toBitMask(a) == 0; }
static bool anyTrue(cmpx8 a) { return toBitMask(a) > 0; }
static bool anyFalse(cmpx8 a) { return !allTrue(a); }
#endif

static floatx8 abs(floatx8 a) { floatx8 result = andNot(-0.f, a); return result; }
static floatx8 floor(floatx8 a) { floatx8 result = { _mm256_floor_ps(a) }; return result; }
static floatx8 minimum(floatx8 a, floatx8 b) { floatx8 result = { _mm256_min_ps(a, b) }; return result; }
static floatx8 maximum(floatx8 a, floatx8 b) { floatx8 result = { _mm256_max_ps(a, b) }; return result; }

static floatx8 lerp(floatx8 l, floatx8 u, floatx8 t) { return l + t * (u - l); }
static floatx8 inverseLerp(floatx8 l, floatx8 u, floatx8 v) { return (v - l) / (u - l); }
static floatx8 remap(floatx8 v, floatx8 oldL, floatx8 oldU, floatx8 newL, floatx8 newU) { return lerp(newL, newU, inverseLerp(oldL, oldU, v)); }
static floatx8 clamp(floatx8 v, floatx8 l, floatx8 u) { return minimum(u, maximum(l, v)); }
static floatx8 clamp01(floatx8 v) { return clamp(v, 0.f, 1.f); }

static floatx8 signOf(floatx8 f) { return ifThen(f < 0.f, floatx8(-1), ifThen(f == 0.f, zerox8(), floatx8(1))); }
static floatx8 signbit(floatx8 f) { return (f & -0.f) >> 31; }

static floatx8 exp2(floatx8 x)
{
	x = minimum(x, 129.00000f);
	x = maximum(x, -126.99999f);

	intx8 ipart = convertFloatToInt(x - 0.5f);
	floatx8 fpart = x - convertIntToFloat(ipart);
	floatx8 expipart = reinterpretIntAsFloat((ipart + 127) << 23);
	floatx8 expfpart = POLY3(fpart, 9.9992520e-1f, 6.9583356e-1f, 2.2606716e-1f, 7.8024521e-2f);
	return expipart * expfpart;
}

static floatx8 log2(floatx8 x)
{
	intx8 exp = 0x7F800000;
	intx8 mant = 0x007FFFFF;

	floatx8 one = 1;
	intx8 i = reinterpretFloatAsInt(x);

	floatx8 e = convertIntToFloat(((i & exp) >> 23) - 127);
	floatx8 m = reinterpretIntAsFloat(i & mant) | one;
	floatx8 p = POLY4(m, 2.8882704548164776201f, -2.52074962577807006663f, 1.48116647521213171641f, -0.465725644288844778798f, 0.0596515482674574969533f);

	return fmadd(p, m - one, e);
}

static floatx8 pow(floatx8 x, floatx8 y)
{
	return exp2(log2(x) * y);
}


#endif

#if defined(SIMD_AVX_512)

typedef __mmask16 cmpx16;

struct floatx16
{
	__m512 f;

	floatx16(float f_) { f = _mm512_set1_ps(f_); }
	floatx16(__m512 f_) { f = f_; }
	floatx16(float* f_) { f = _mm512_loadu_ps(f_); }

	operator __m512() { return f; }

	void store(float* f_) { _mm512_storeu_ps(f_, f); }
};

struct intx16
{
	__m512i i;

	intx16(int i_) { i = _mm512_set1_epi32(i_); }
	intx16(__m512i i_) { i = i_; }
	intx16(int* i_) { i = _mm512_loadu_epi32(i_); }

	operator __m512i() { return i; }

	void store(int* i_) { _mm512_storeu_epi32(i_, i); }
};

static floatx16 truex16() { const float nnan = (const float&)0xFFFFFFFF; return nnan; }
static floatx16 zerox16() { return _mm512_setzero_ps(); }

static floatx16 convertIntToFloat(intx16 i) { return _mm512_cvtepi32_ps(i); }
static intx16 convertFloatToInt(floatx16 f) { return _mm512_cvtps_epi32(f); }
static floatx16 reinterpretIntAsFloat(intx16 i) { return _mm512_castsi512_ps(i); }
static intx16 reinterpretFloatAsInt(floatx16 f) { return _mm512_castps_si512(f); }


// Int operators.
static intx16 andNot(intx16 a, intx16 b) { intx16 result = { _mm512_andnot_si512(a, b) }; return result; }

static intx16 operator+(intx16 a, intx16 b) { intx16 result = { _mm512_add_epi32(a, b) }; return result; }
static intx16& operator+=(intx16& a, intx16 b) { a = a + b; return a; }
static intx16 operator-(intx16 a, intx16 b) { intx16 result = { _mm512_sub_epi32(a, b) }; return result; }
static intx16& operator-=(intx16& a, intx16 b) { a = a - b; return a; }
static intx16 operator*(intx16 a, intx16 b) { intx16 result = { _mm512_mul_epi32(a, b) }; return result; }
static intx16& operator*=(intx16& a, intx16 b) { a = a * b; return a; }
static intx16 operator/(intx16 a, intx16 b) { intx16 result = { _mm512_div_epi32(a, b) }; return result; }
static intx16& operator/=(intx16& a, intx16 b) { a = a / b; return a; }
static intx16 operator&(intx16 a, intx16 b) { intx16 result = { _mm512_and_epi32(a, b) }; return result; }
static intx16& operator&=(intx16& a, intx16 b) { a = a & b; return a; }
static intx16 operator|(intx16 a, intx16 b) { intx16 result = { _mm512_or_epi32(a, b) }; return result; }
static intx16& operator|=(intx16& a, intx16 b) { a = a | b; return a; }
static intx16 operator^(intx16 a, intx16 b) { intx16 result = { _mm512_xor_epi32(a, b) }; return result; }
static intx16& operator^=(intx16& a, intx16 b) { a = a ^ b; return a; }

static intx16 operator~(intx16 a) { a = andNot(a, reinterpretFloatAsInt(truex16())); return a; }

static cmpx16 operator==(intx16 a, intx16 b) { cmpx16 result = _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_EQ); return result; }
static cmpx16 operator!=(intx16 a, intx16 b) { cmpx16 result = _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_NE); return result; }
static cmpx16 operator>(intx16 a, intx16 b) { cmpx16 result = _mm512_cmp_epi32_mask(b, a, _MM_CMPINT_LT); return result; }
static cmpx16 operator>=(intx16 a, intx16 b) { cmpx16 result = _mm512_cmp_epi32_mask(b, a, _MM_CMPINT_LE); return result; }
static cmpx16 operator<(intx16 a, intx16 b) { cmpx16 result = _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_LT); return result; }
static cmpx16 operator<=(intx16 a, intx16 b) { cmpx16 result = _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_LE); return result; }


static intx16 operator>>(intx16 a, int b) { intx16 result = { _mm512_srli_epi32(a, b) }; return result; }
static intx16& operator>>=(intx16& a, int b) { a = a >> b; return a; }
static intx16 operator<<(intx16 a, int b) { intx16 result = { _mm512_slli_epi32(a, b) }; return result; }
static intx16& operator<<=(intx16& a, int b) { a = a << b; return a; }

static intx16 operator-(intx16 a) { intx16 result = { _mm512_sub_epi32(_mm512_setzero_si512(), a) }; return result; }



// Float operators.
static floatx16 andNot(floatx16 a, floatx16 b) { floatx16 result = { _mm512_andnot_ps(a, b) }; return result; }

static floatx16 operator+(floatx16 a, floatx16 b) { floatx16 result = { _mm512_add_ps(a, b) }; return result; }
static floatx16& operator+=(floatx16& a, floatx16 b) { a = a + b; return a; }
static floatx16 operator-(floatx16 a, floatx16 b) { floatx16 result = { _mm512_sub_ps(a, b) }; return result; }
static floatx16& operator-=(floatx16& a, floatx16 b) { a = a - b; return a; }
static floatx16 operator*(floatx16 a, floatx16 b) { floatx16 result = { _mm512_mul_ps(a, b) }; return result; }
static floatx16& operator*=(floatx16& a, floatx16 b) { a = a * b; return a; }
static floatx16 operator/(floatx16 a, floatx16 b) { floatx16 result = { _mm512_div_ps(a, b) }; return result; }
static floatx16& operator/=(floatx16& a, floatx16 b) { a = a / b; return a; }
static floatx16 operator&(floatx16 a, floatx16 b) { floatx16 result = { _mm512_and_ps(a, b) }; return result; }
static floatx16& operator&=(floatx16& a, floatx16 b) { a = a & b; return a; }
static floatx16 operator|(floatx16 a, floatx16 b) { floatx16 result = { _mm512_or_ps(a, b) }; return result; }
static floatx16& operator|=(floatx16& a, floatx16 b) { a = a | b; return a; }
static floatx16 operator^(floatx16 a, floatx16 b) { floatx16 result = { _mm512_xor_ps(a, b) }; return result; }
static floatx16& operator^=(floatx16& a, floatx16 b) { a = a ^ b; return a; }

static floatx16 operator~(floatx16 a) { a = andNot(a, truex16()); return a; }

static cmpx16 operator==(floatx16 a, floatx16 b) { cmpx16 result = _mm512_cmp_ps_mask(a, b, _CMP_EQ_OQ); return result; }
static cmpx16 operator!=(floatx16 a, floatx16 b) { cmpx16 result = _mm512_cmp_ps_mask(a, b, _CMP_NEQ_OQ); return result; }
static cmpx16 operator>(floatx16 a, floatx16 b) { cmpx16 result = _mm512_cmp_ps_mask(a, b, _CMP_GT_OQ); return result; }
static cmpx16 operator>=(floatx16 a, floatx16 b) { cmpx16 result = _mm512_cmp_ps_mask(a, b, _CMP_GE_OQ); return result; }
static cmpx16 operator<(floatx16 a, floatx16 b) { cmpx16 result = _mm512_cmp_ps_mask(a, b, _CMP_LT_OQ); return result; }
static cmpx16 operator<=(floatx16 a, floatx16 b) { cmpx16 result = _mm512_cmp_ps_mask(a, b, _CMP_LE_OQ); return result; }


static floatx16 operator>>(floatx16 a, int b) { return reinterpretIntAsFloat(reinterpretFloatAsInt(a) >> b); }
static floatx16& operator>>=(floatx16& a, int b) { a = a >> b; return a; }
static floatx16 operator<<(floatx16 a, int b) { return reinterpretIntAsFloat(reinterpretFloatAsInt(a) << b); }
static floatx16& operator<<=(floatx16& a, int b) { a = a << b; return a; }

static floatx16 operator-(floatx16 a) { floatx16 result = { _mm512_xor_ps(a, _mm512_set1_ps(-0)) }; return result; }




static float addElements(floatx16 a) { return _mm512_reduce_add_ps(a); }

static floatx16 fmadd(floatx16 a, floatx16 b, floatx16 c) { floatx16 result = { _mm512_fmadd_ps(a, b, c) }; return result; }
static floatx16 fmsub(floatx16 a, floatx16 b, floatx16 c) { floatx16 result = { _mm512_fmsub_ps(a, b, c) }; return result; }

static floatx16 sqrt(floatx16 a) { floatx16 result = { _mm512_sqrt_ps(a) }; return result; }
static floatx16 rsqrt(floatx16 a) { floatx16 result = { 1.f / _mm512_sqrt_ps(a) }; return result; }

static floatx16 ifThen(cmpx16 cond, floatx16 ifCase, floatx16 elseCase) { return _mm512_mask_blend_ps(cond, ifCase, elseCase); }

static bool allTrue(cmpx16 a) { return a == (1 << 16) - 1; }
static bool allFalse(cmpx16 a) { return a == 0; }
static bool anyTrue(cmpx16 a) { return a > 0; }
static bool anyFalse(cmpx16 a) { return !allTrue(a); }


static floatx16 abs(floatx16 a) { floatx16 result = andNot(-0.f, a); return result; }
static floatx16 floor(floatx16 a) { floatx16 result = { _mm512_floor_ps(a) }; return result; }
static floatx16 minimum(floatx16 a, floatx16 b) { floatx16 result = { _mm512_min_ps(a, b) }; return result; }
static floatx16 maximum(floatx16 a, floatx16 b) { floatx16 result = { _mm512_max_ps(a, b) }; return result; }

static floatx16 lerp(floatx16 l, floatx16 u, floatx16 t) { return l + t * (u - l); }
static floatx16 inverseLerp(floatx16 l, floatx16 u, floatx16 v) { return (v - l) / (u - l); }
static floatx16 remap(floatx16 v, floatx16 oldL, floatx16 oldU, floatx16 newL, floatx16 newU) { return lerp(newL, newU, inverseLerp(oldL, oldU, v)); }
static floatx16 clamp(floatx16 v, floatx16 l, floatx16 u) { return minimum(u, maximum(l, v)); }
static floatx16 clamp01(floatx16 v) { return clamp(v, 0.f, 1.f); }

static floatx16 signOf(floatx16 f) { return ifThen(f < 0.f, floatx16(-1), ifThen(f == 0.f, zerox16(), floatx16(1))); }
static floatx16 signbit(floatx16 f) { return (f & -0.f) >> 31; }

static floatx16 exp2(floatx16 x)
{
	x = minimum(x, 129.00000f);
	x = maximum(x, -126.99999f);

	intx16 ipart = convertFloatToInt(x - 0.5f);
	floatx16 fpart = x - convertIntToFloat(ipart);
	floatx16 expipart = reinterpretIntAsFloat((ipart + 127) << 23);
	floatx16 expfpart = POLY3(fpart, 9.9992520e-1f, 6.9583356e-1f, 2.2606716e-1f, 7.8024521e-2f);
	return expipart * expfpart;
}

static floatx16 log2(floatx16 x)
{
	intx16 exp = 0x7F800000;
	intx16 mant = 0x007FFFFF;

	floatx16 one = 1;
	intx16 i = reinterpretFloatAsInt(x);

	floatx16 e = convertIntToFloat(((i & exp) >> 23) - 127);
	floatx16 m = reinterpretIntAsFloat(i & mant) | one;
	floatx16 p = POLY4(m, 2.8882704548164776201f, -2.52074962577807006663f, 1.48116647521213171641f, -0.465725644288844778798f, 0.0596515482674574969533f);

	return fmadd(p, m - one, e);
}

static floatx16 pow(floatx16 x, floatx16 y)
{
	return exp2(log2(x) * y);
}


#endif


#if defined(SIMD_AVX_512)
typedef floatx16 floatx;
typedef intx16 intx;
typedef cmpx16 cmpx;
#define SIMD_LANES 16
#elif defined(SIMD_AVX_2)
typedef floatx8 floatx;
typedef intx8 intx;
typedef cmpx8 cmpx;
#define SIMD_LANES 8
#else
typedef floatx4 floatx;
typedef intx4 intx;
typedef floatx4 cmpx;
#define SIMD_LANES 4
#endif








