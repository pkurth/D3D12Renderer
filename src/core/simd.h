#pragma once


#include <emmintrin.h>
#include <immintrin.h>

#define SIMD_SSE_2 // Not quite correct but I think it is safe to assume that every processor has SSE2.

#if defined(__AVX__)
#if defined(__AVX512BW__)
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


#if defined(SIMD_SSE_2)

typedef __m128 cmpx4;

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

	void store(float* f_) { _mm_storeu_ps(f_, f); }

#if defined(SIMD_AVX_512)
	void scatter(float* baseAddress, __m128i indices) { _mm_i32scatter_ps(baseAddress, indices, f, 4); }
	void scatter(float* baseAddress, int a, int b, int c, int d) { scatter(baseAddress, _mm_setr_epi32(a, b, c, d)); }
#else
	void scatter(float* baseAddress, int a, int b, int c, int d)
	{
		baseAddress[a] = this->f.m128_f32[0];
		baseAddress[b] = this->f.m128_f32[1];
		baseAddress[c] = this->f.m128_f32[2];
		baseAddress[d] = this->f.m128_f32[3];
	}

	void scatter(float* baseAddress, __m128i indices)
	{
		baseAddress[indices.m128i_i32[0]] = this->f.m128_f32[0];
		baseAddress[indices.m128i_i32[1]] = this->f.m128_f32[1];
		baseAddress[indices.m128i_i32[2]] = this->f.m128_f32[2];
		baseAddress[indices.m128i_i32[3]] = this->f.m128_f32[3];
	}
#endif
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

	void store(int* i_) { _mm_storeu_si128((__m128i*)i_, i); }

#if defined(SIMD_AVX_512)
	void scatter(int* baseAddress, __m128i indices) { _mm_i32scatter_epi32(baseAddress, indices, i, 4); }
	void scatter(int* baseAddress, int a, int b, int c, int d) { scatter(baseAddress, _mm_setr_epi32(a, b, c, d)); }
#else
	void scatter(int* baseAddress, int a, int b, int c, int d)
	{
		baseAddress[a] = this->i.m128i_i32[0];
		baseAddress[b] = this->i.m128i_i32[1];
		baseAddress[c] = this->i.m128i_i32[2];
		baseAddress[d] = this->i.m128i_i32[3];
	}

	void scatter(int* baseAddress, __m128i indices)
	{
		baseAddress[indices.m128i_i32[0]] = this->i.m128i_i32[0];
		baseAddress[indices.m128i_i32[1]] = this->i.m128i_i32[1];
		baseAddress[indices.m128i_i32[2]] = this->i.m128i_i32[2];
		baseAddress[indices.m128i_i32[3]] = this->i.m128i_i32[3];
	}
#endif
};

static floatx4 truex4() { const float nnan = (const float&)0xFFFFFFFF; return nnan; }
static floatx4 zerox4() { return _mm_setzero_ps(); }

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

static intx4 operator~(intx4 a) { a = andNot(a, reinterpret(truex4())); return a; }

static intx4 operator>>(intx4 a, int b) { return _mm_srli_epi32(a, b); }
static intx4& operator>>=(intx4& a, int b) { a = a >> b; return a; }
static intx4 operator<<(intx4 a, int b) { return _mm_slli_epi32(a, b); }
static intx4& operator<<=(intx4& a, int b) { a = a << b; return a; }

static intx4 operator-(intx4 a) { return _mm_sub_epi32(_mm_setzero_si128(), a); }



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

static floatx4 operator~(floatx4 a) { a = andNot(a, truex4()); return a; }


static cmpx4 operator==(intx4 a, intx4 b) { return reinterpret(_mm_cmpeq_epi32(a, b)); }
static cmpx4 operator!=(intx4 a, intx4 b) { return ~(a == b); }
static cmpx4 operator>(intx4 a, intx4 b) { return reinterpret(_mm_cmpgt_epi32(a, b)); }
static cmpx4 operator>=(intx4 a, intx4 b) { return (a > b) | (a == b); }
static cmpx4 operator<(intx4 a, intx4 b) { return reinterpret(_mm_cmplt_epi32(a, b)); }
static cmpx4 operator<=(intx4 a, intx4 b) { return (a < b) | (a == b); }

static cmpx4 operator==(floatx4 a, floatx4 b) { return _mm_cmpeq_ps(a, b); }
static cmpx4 operator!=(floatx4 a, floatx4 b) { return _mm_cmpneq_ps(a, b); }
static cmpx4 operator>(floatx4 a, floatx4 b) { return _mm_cmpgt_ps(a, b); }
static cmpx4 operator>=(floatx4 a, floatx4 b) { return _mm_cmpge_ps(a, b); }
static cmpx4 operator<(floatx4 a, floatx4 b) { return _mm_cmplt_ps(a, b); }
static cmpx4 operator<=(floatx4 a, floatx4 b) { return _mm_cmple_ps(a, b); }

static floatx4 operator>>(floatx4 a, int b) { return reinterpret(reinterpret(a) >> b); }
static floatx4& operator>>=(floatx4& a, int b) { a = a >> b; return a; }
static floatx4 operator<<(floatx4 a, int b) { return reinterpret(reinterpret(a) << b); }
static floatx4& operator<<=(floatx4& a, int b) { a = a << b; return a; }

static floatx4 operator-(floatx4 a) { return _mm_xor_ps(a, _mm_set1_ps(-0)); }




static float addElements(floatx4 a) { __m128 aa = _mm_hadd_ps(a, a); aa = _mm_hadd_ps(aa, aa); return aa.m128_f32[0]; }

static floatx4 fmadd(floatx4 a, floatx4 b, floatx4 c) { return _mm_fmadd_ps(a, b, c); }
static floatx4 fmsub(floatx4 a, floatx4 b, floatx4 c) { return _mm_fmsub_ps(a, b, c); }

static floatx4 sqrt(floatx4 a) { return _mm_sqrt_ps(a); }
static floatx4 rsqrt(floatx4 a) { return _mm_rsqrt_ps(a); }

static floatx4 ifThen(cmpx4 cond, floatx4 ifCase, floatx4 elseCase) { return _mm_blendv_ps(elseCase, ifCase, cond); }

static int toBitMask(floatx4 a) { int result = _mm_movemask_ps(a); return result; }
static bool allTrue(floatx4 a) { return toBitMask(a) == (1 << 4) - 1; }
static bool allFalse(floatx4 a) { return toBitMask(a) == 0; }
static bool anyTrue(floatx4 a) { return toBitMask(a) > 0; }
static bool anyFalse(floatx4 a) { return !allTrue(a); }

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

static floatx4 signOf(floatx4 f) { return ifThen(f < 0.f, floatx4(-1), ifThen(f == 0.f, zerox4(), floatx4(1))); }
static floatx4 signbit(floatx4 f) { return (f & -0.f) >> 31; }


static floatx4 exp2(floatx4 x)
{
	x = minimum(x, 129.00000f);
	x = maximum(x, -126.99999f);

	intx4 ipart = convert(x - 0.5f);
	floatx4 fpart = x - convert(ipart);
	floatx4 expipart = reinterpret((ipart + 127) << 23);
	floatx4 expfpart = POLY3(fpart, 9.9992520e-1f, 6.9583356e-1f, 2.2606716e-1f, 7.8024521e-2f);
	return expipart * expfpart;
}

static floatx4 log2(floatx4 x)
{
	intx4 exp = 0x7F800000;
	intx4 mant = 0x007FFFFF;

	floatx4 one = 1;
	intx4 i = reinterpret(x);

	floatx4 e = convert(((i & exp) >> 23) - 127);
	floatx4 m = reinterpret(i & mant) | one;
	floatx4 p = POLY4(m, 2.8882704548164776201f, -2.52074962577807006663f, 1.48116647521213171641f, -0.465725644288844778798f, 0.0596515482674574969533f);

	return fmadd(p, m - one, e);
}

static floatx4 pow(floatx4 x, floatx4 y)
{
	return exp2(log2(x) * y);
}

static floatx4 pow(floatx4 x, float y)
{
	return exp2(log2(x) * floatx4(y));
}

static floatx4 exp(floatx4 x)
{
	floatx4 a = 12102203.f; // (1 << 23) / log(2).
	intx4 b = 127 * (1 << 23) - 298765;
	intx4 t = convert(a * x) + b;
	return reinterpret(t);
}

static floatx4 tanh(floatx4 x)
{
	floatx4 a = exp(x);
	floatx4 b = exp(-x);
	return (a - b) / (a + b);
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

	floatx8() {}
	floatx8(float f_) { f = _mm256_set1_ps(f_); }
	floatx8(__m256 f_) { f = f_; }
	floatx8(float a, float b, float c, float d, float e, float f, float g, float h) { this->f = _mm256_setr_ps(a, b, c, d, e, f, g, h); }
	floatx8(const float* f_) { f = _mm256_loadu_ps(f_); }

	floatx8(const float* baseAddress, __m256i indices) { f = _mm256_i32gather_ps(baseAddress, indices, 4); }
	floatx8(const float* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h) : floatx8(baseAddress, _mm256_setr_epi32(a, b, c, d, e, f, g, h)) {}

	operator __m256() { return f; }

	void store(float* f_) { _mm256_storeu_ps(f_, f); }

#if defined(SIMD_AVX_512)
	void scatter(float* baseAddress, __m256i indices) { _mm256_i32scatter_ps(baseAddress, indices, f, 4); }
	void scatter(float* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h) { scatter(baseAddress, _mm256_setr_epi32(a, b, c, d, e, f, g, h)); }
#else
	void scatter(float* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h)
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

	void scatter(float* baseAddress, __m256i indices)
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

	void store(int* i_) { _mm256_storeu_epi32(i_, i); }

#if defined(SIMD_AVX_512)
	void scatter(int* baseAddress, __m256i indices) { _mm256_i32scatter_epi32(baseAddress, indices, i, 4); }
	void scatter(int* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h) { scatter(baseAddress, _mm256_setr_epi32(a, b, c, d, e, f, g, h)); }
#else
	void scatter(int* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h) 
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

	void scatter(int* baseAddress, __m256i indices)
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
};

static floatx8 truex8() { const float nnan = (const float&)0xFFFFFFFF; return nnan; }
static floatx8 zerox8() { return _mm256_setzero_ps(); }

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

static intx8 operator~(intx8 a) { a = andNot(a, reinterpret(truex8())); return a; }

static intx8 operator>>(intx8 a, int b) { return _mm256_srli_epi32(a, b); }
static intx8& operator>>=(intx8& a, int b) { a = a >> b; return a; }
static intx8 operator<<(intx8 a, int b) { return _mm256_slli_epi32(a, b); }
static intx8& operator<<=(intx8& a, int b) { a = a << b; return a; }

static intx8 operator-(intx8 a) { return _mm256_sub_epi32(_mm256_setzero_si256(), a); }



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

static floatx8 operator~(floatx8 a) { a = andNot(a, truex8()); return a; }


#if defined(SIMD_AVX_512)
static cmpx8 operator==(intx8 a, intx8 b) { return _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_EQ); }
static cmpx8 operator!=(intx8 a, intx8 b) { return _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_NE); }
static cmpx8 operator>(intx8 a, intx8 b) { return _mm256_cmp_epi32_mask(b, a, _MM_CMPINT_LT); }
static cmpx8 operator>=(intx8 a, intx8 b) { return _mm256_cmp_epi32_mask(b, a, _MM_CMPINT_LE); }
static cmpx8 operator<(intx8 a, intx8 b) { return _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_LT); }
static cmpx8 operator<=(intx8 a, intx8 b) { return _mm256_cmp_epi32_mask(a, b, _MM_CMPINT_LE); }
#else
static cmpx8 operator==(intx8 a, intx8 b) { return reinterpret(_mm256_cmpeq_epi32(a, b)); }
static cmpx8 operator!=(intx8 a, intx8 b) { return ~(a == b); }
static cmpx8 operator>(intx8 a, intx8 b) { return reinterpret(_mm256_cmpgt_epi32(a, b)); }
static cmpx8 operator>=(intx8 a, intx8 b) { return (a > b) | (a == b); }
static cmpx8 operator<(intx8 a, intx8 b) { return reinterpret(_mm256_cmpgt_epi32(b, a)); }
static cmpx8 operator<=(intx8 a, intx8 b) { return (a < b) | (a == b); }
#endif


#if defined(SIMD_AVX_512)
static cmpx8 operator==(floatx8 a, floatx8 b) { return _mm256_cmp_ps_mask(a, b, _CMP_EQ_OQ); }
static cmpx8 operator!=(floatx8 a, floatx8 b) { return _mm256_cmp_ps_mask(a, b, _CMP_NEQ_OQ); }
static cmpx8 operator>(floatx8 a, floatx8 b) { return _mm256_cmp_ps_mask(a, b, _CMP_GT_OQ); }
static cmpx8 operator>=(floatx8 a, floatx8 b) { return _mm256_cmp_ps_mask(a, b, _CMP_GE_OQ); }
static cmpx8 operator<(floatx8 a, floatx8 b) { return _mm256_cmp_ps_mask(a, b, _CMP_LT_OQ); }
static cmpx8 operator<=(floatx8 a, floatx8 b) { return _mm256_cmp_ps_mask(a, b, _CMP_LE_OQ); }
#else
static cmpx8 operator==(floatx8 a, floatx8 b) { return _mm256_cmp_ps(a, b, _CMP_EQ_OQ); }
static cmpx8 operator!=(floatx8 a, floatx8 b) { return _mm256_cmp_ps(a, b, _CMP_NEQ_OQ); }
static cmpx8 operator>(floatx8 a, floatx8 b) { return _mm256_cmp_ps(a, b, _CMP_GT_OQ); }
static cmpx8 operator>=(floatx8 a, floatx8 b) { return _mm256_cmp_ps(a, b, _CMP_GE_OQ); }
static cmpx8 operator<(floatx8 a, floatx8 b) { return _mm256_cmp_ps(a, b, _CMP_LT_OQ); }
static cmpx8 operator<=(floatx8 a, floatx8 b) { return _mm256_cmp_ps(a, b, _CMP_LE_OQ); }
#endif

static floatx8 operator>>(floatx8 a, int b) { return reinterpret(reinterpret(a) >> b); }
static floatx8& operator>>=(floatx8& a, int b) { a = a >> b; return a; }
static floatx8 operator<<(floatx8 a, int b) { return reinterpret(reinterpret(a) << b); }
static floatx8& operator<<=(floatx8& a, int b) { a = a << b; return a; }

static floatx8 operator-(floatx8 a) { return _mm256_xor_ps(a, _mm256_set1_ps(-0)); }




static float addElements(floatx8 a) { __m256 aa = _mm256_hadd_ps(a, a); aa = _mm256_hadd_ps(aa, aa); return aa.m256_f32[0] + aa.m256_f32[4]; }

static floatx8 fmadd(floatx8 a, floatx8 b, floatx8 c) { return _mm256_fmadd_ps(a, b, c); }
static floatx8 fmsub(floatx8 a, floatx8 b, floatx8 c) { return _mm256_fmsub_ps(a, b, c); }

static floatx8 sqrt(floatx8 a) { return _mm256_sqrt_ps(a); }
static floatx8 rsqrt(floatx8 a) { return _mm256_rsqrt_ps(a); }

#if defined(SIMD_AVX_512)
static floatx8 ifThen(cmpx8 cond, floatx8 ifCase, floatx8 elseCase) { return _mm256_mask_blend_ps(cond, elseCase, ifCase); }

static bool allTrue(cmpx8 a) { return a == (1 << 8) - 1; }
static bool allFalse(cmpx8 a) { return a == 0; }
static bool anyTrue(cmpx8 a) { return a > 0; }
static bool anyFalse(cmpx8 a) { return !allTrue(a); }
#else
static floatx8 ifThen(cmpx8 cond, floatx8 ifCase, floatx8 elseCase) { return _mm256_blendv_ps(elseCase, ifCase, cond); }

static int toBitMask(cmpx8 a) { return _mm256_movemask_ps(a); }
static bool allTrue(cmpx8 a) { return toBitMask(a) == (1 << 8) - 1; }
static bool allFalse(cmpx8 a) { return toBitMask(a) == 0; }
static bool anyTrue(cmpx8 a) { return toBitMask(a) > 0; }
static bool anyFalse(cmpx8 a) { return !allTrue(a); }
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

static floatx8 signOf(floatx8 f) { return ifThen(f < 0.f, floatx8(-1), ifThen(f == 0.f, zerox8(), floatx8(1))); }
static floatx8 signbit(floatx8 f) { return (f & -0.f) >> 31; }

static floatx8 exp2(floatx8 x)
{
	x = minimum(x, 129.00000f);
	x = maximum(x, -126.99999f);

	intx8 ipart = convert(x - 0.5f);
	floatx8 fpart = x - convert(ipart);
	floatx8 expipart = reinterpret((ipart + 127) << 23);
	floatx8 expfpart = POLY3(fpart, 9.9992520e-1f, 6.9583356e-1f, 2.2606716e-1f, 7.8024521e-2f);
	return expipart * expfpart;
}

static floatx8 log2(floatx8 x)
{
	intx8 exp = 0x7F800000;
	intx8 mant = 0x007FFFFF;

	floatx8 one = 1;
	intx8 i = reinterpret(x);

	floatx8 e = convert(((i & exp) >> 23) - 127);
	floatx8 m = reinterpret(i & mant) | one;
	floatx8 p = POLY4(m, 2.8882704548164776201f, -2.52074962577807006663f, 1.48116647521213171641f, -0.465725644288844778798f, 0.0596515482674574969533f);

	return fmadd(p, m - one, e);
}

static floatx8 pow(floatx8 x, floatx8 y)
{
	return exp2(log2(x) * y);
}

static floatx8 exp(floatx8 x)
{
	floatx8 a = 12102203.f; // (1 << 23) / log(2).
	intx8 b = 127 * (1 << 23) - 298765;
	intx8 t = convert(a * x) + b;
	return reinterpret(t);
}

static floatx8 tanh(floatx8 x)
{
	floatx8 a = exp(x);
	floatx8 b = exp(-x);
	return (a - b) / (a + b);
}


#endif

#if defined(SIMD_AVX_512)

typedef __mmask16 cmpx16;

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

	void store(float* f_) { _mm512_storeu_ps(f_, f); }

	void scatter(float* baseAddress, __m512i indices) { _mm512_i32scatter_ps(baseAddress, indices, f, 4); }
	void scatter(float* baseAddress, int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l, int m, int n, int o, int p) { scatter(baseAddress, _mm512_setr_epi32(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)); }
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

	void store(int* i_) { _mm512_storeu_epi32(i_, i); }
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

static cmpx16 operator==(intx16 a, intx16 b) { return _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_EQ); }
static cmpx16 operator!=(intx16 a, intx16 b) { return _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_NE); }
static cmpx16 operator>(intx16 a, intx16 b) { return _mm512_cmp_epi32_mask(b, a, _MM_CMPINT_LT); }
static cmpx16 operator>=(intx16 a, intx16 b) { return _mm512_cmp_epi32_mask(b, a, _MM_CMPINT_LE); }
static cmpx16 operator<(intx16 a, intx16 b) { return _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_LT); }
static cmpx16 operator<=(intx16 a, intx16 b) { return _mm512_cmp_epi32_mask(a, b, _MM_CMPINT_LE); }


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

static cmpx16 operator==(floatx16 a, floatx16 b) { return _mm512_cmp_ps_mask(a, b, _CMP_EQ_OQ); }
static cmpx16 operator!=(floatx16 a, floatx16 b) { return _mm512_cmp_ps_mask(a, b, _CMP_NEQ_OQ); }
static cmpx16 operator>(floatx16 a, floatx16 b) { return _mm512_cmp_ps_mask(a, b, _CMP_GT_OQ); }
static cmpx16 operator>=(floatx16 a, floatx16 b) { return _mm512_cmp_ps_mask(a, b, _CMP_GE_OQ); }
static cmpx16 operator<(floatx16 a, floatx16 b) { return _mm512_cmp_ps_mask(a, b, _CMP_LT_OQ); }
static cmpx16 operator<=(floatx16 a, floatx16 b) { return _mm512_cmp_ps_mask(a, b, _CMP_LE_OQ); }


static floatx16 operator>>(floatx16 a, int b) { return reinterpret(reinterpret(a) >> b); }
static floatx16& operator>>=(floatx16& a, int b) { a = a >> b; return a; }
static floatx16 operator<<(floatx16 a, int b) { return reinterpret(reinterpret(a) << b); }
static floatx16& operator<<=(floatx16& a, int b) { a = a << b; return a; }

static floatx16 operator-(floatx16 a) { return _mm512_xor_ps(a, _mm512_set1_ps(-0)); }




static float addElements(floatx16 a) { return _mm512_reduce_add_ps(a); }

static floatx16 fmadd(floatx16 a, floatx16 b, floatx16 c) { return _mm512_fmadd_ps(a, b, c); }
static floatx16 fmsub(floatx16 a, floatx16 b, floatx16 c) { return _mm512_fmsub_ps(a, b, c); }

static floatx16 sqrt(floatx16 a) { return _mm512_sqrt_ps(a); }
static floatx16 rsqrt(floatx16 a) { return 1.f / _mm512_sqrt_ps(a); }

static floatx16 ifThen(cmpx16 cond, floatx16 ifCase, floatx16 elseCase) { return _mm512_mask_blend_ps(cond, elseCase, ifCase); }

static bool allTrue(cmpx16 a) { return a == (1 << 16) - 1; }
static bool allFalse(cmpx16 a) { return a == 0; }
static bool anyTrue(cmpx16 a) { return a > 0; }
static bool anyFalse(cmpx16 a) { return !allTrue(a); }


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

static floatx16 exp2(floatx16 x)
{
	x = minimum(x, 129.00000f);
	x = maximum(x, -126.99999f);

	intx16 ipart = convert(x - 0.5f);
	floatx16 fpart = x - convert(ipart);
	floatx16 expipart = reinterpret((ipart + 127) << 23);
	floatx16 expfpart = POLY3(fpart, 9.9992520e-1f, 6.9583356e-1f, 2.2606716e-1f, 7.8024521e-2f);
	return expipart * expfpart;
}

static floatx16 log2(floatx16 x)
{
	intx16 exp = 0x7F800000;
	intx16 mant = 0x007FFFFF;

	floatx16 one = 1;
	intx16 i = reinterpret(x);

	floatx16 e = convert(((i & exp) >> 23) - 127);
	floatx16 m = reinterpret(i & mant) | one;
	floatx16 p = POLY4(m, 2.8882704548164776201f, -2.52074962577807006663f, 1.48116647521213171641f, -0.465725644288844778798f, 0.0596515482674574969533f);

	return fmadd(p, m - one, e);
}

static floatx16 pow(floatx16 x, floatx16 y)
{
	return exp2(log2(x) * y);
}

static floatx16 exp(floatx16 x)
{
	floatx16 a = 12102203.f; // (1 << 23) / log(2).
	intx16 b = 127 * (1 << 23) - 298765;
	intx16 t = convert(a * x) + b;
	return reinterpret(t);
}

static floatx16 tanh(floatx16 x)
{
	floatx16 a = exp(x);
	floatx16 b = exp(-x);
	return (a - b) / (a + b);
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








