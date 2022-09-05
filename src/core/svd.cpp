#include "pch.h"
#include "svd.h"

#define _gamma 5.828427124f // FOUR_GAMMA_SQUARED = sqrt(8)+3;
#define _cstar 0.923879532f // cos(pi/8)
#define _sstar 0.3826834323f // sin(p/8)

static void condSwap(bool c, float& X, float& Y)
{
	float Z = X;
	X = c ? Y : X;
	Y = c ? Z : Y;
}

static void condNegSwap(bool c, float& X, float& Y)
{
	float Z = -X;
	X = c ? Y : X;
	Y = c ? Z : Y;
}

static void approximateGivensQuaternion(float a11, float a12, float a22, float& ch, float& sh)
{
	// Given givens angle computed by approximateGivensAngles,
	// compute the corresponding rotation quaternion.
	ch = 2 * (a11 - a22);
	sh = a12;
	bool b = _gamma * sh * sh < ch* ch;
	float w = 1.f / sqrt(ch * ch + sh * sh);
	ch = b ? w * ch : _cstar;
	sh = b ? w * sh : _sstar;
}
static void QRGivensQuaternion(float a1, float a2, float& ch, float& sh)
{
	// a1 = pivot point on diagonal
	// a2 = lower triangular entry we want to annihilate
	float rho = sqrt(a1 * a1 + a2 * a2);

	sh = rho > EPSILON ? a2 : 0.f;
	ch = fabs(a1) + fmax(rho, EPSILON);
	bool b = a1 < 0;
	condSwap(b, sh, ch);
	float w = 1.f / sqrt(ch * ch + sh * sh);
	ch *= w;
	sh *= w;
}

static void jacobiConjugation(const int x, const int y, const int z,
	float& s11,
	float& s21, float& s22,
	float& s31, float& s32, float& s33,
	quat& q)
{
	float ch, sh;
	approximateGivensQuaternion(s11, s21, s22, ch, sh);

	float scale = ch * ch + sh * sh;
	float a = (ch * ch - sh * sh) / scale;
	float b = (2 * sh * ch) / scale;

	// make temp copy of S
	float _s11 = s11;
	float _s21 = s21; float _s22 = s22;
	float _s31 = s31; float _s32 = s32; float _s33 = s33;

	// perform conjugation S = Q'*S*Q
	// Q already implicitly solved from a, b
	s11 = a * (a * _s11 + b * _s21) + b * (a * _s21 + b * _s22);
	s21 = a * (-b * _s11 + a * _s21) + b * (-b * _s21 + a * _s22);	s22 = -b * (-b * _s11 + a * _s21) + a * (-b * _s21 + a * _s22);
	s31 = a * _s31 + b * _s32;								s32 = -b * _s31 + a * _s32; s33 = _s33;

	// update cumulative rotation qV
	float tmp[3];
	tmp[0] = q.x * sh;
	tmp[1] = q.y * sh;
	tmp[2] = q.z * sh;
	sh *= q.w;

	q.x *= ch;
	q.y *= ch;
	q.z *= ch;
	q.w *= ch;

	// (x,y,z) corresponds to ((0,1,2),(1,2,0),(2,0,1))
	// for (p,q) = ((0,1),(1,2),(0,2))
	q.v4.data[z] += sh;
	q.v4.w -= tmp[z]; // w
	q.v4.data[x] += tmp[y];
	q.v4.data[y] -= tmp[x];

	// re-arrange matrix for next iteration
	_s11 = s22;
	_s21 = s32; _s22 = s33;
	_s31 = s21; _s32 = s31; _s33 = s11;
	s11 = _s11;
	s21 = _s21; s22 = _s22;
	s31 = _s31; s32 = _s32; s33 = _s33;
}

static void jacobiEigenanlysis(
	float& s11,
	float& s21, float& s22,
	float& s31, float& s32, float& s33,
	// quaternion representation of V
	quat& q)
{
	q = quat::identity;
	for (int i = 0; i < 4; ++i)
	{
		// We wish to eliminate the maximum off-diagonal element
		// on every iteration, but cycling over all 3 possible rotations
		// in fixed order (p,q) = (1,2) , (2,3), (1,3) still retains
		//  asymptotic convergence.
		jacobiConjugation(0, 1, 2, s11, s21, s22, s31, s32, s33, q); // p,q = 0,1
		jacobiConjugation(1, 2, 0, s11, s21, s22, s31, s32, s33, q); // p,q = 1,2
		jacobiConjugation(2, 0, 1, s11, s21, s22, s31, s32, s33, q); // p,q = 0,2
	}
}

static float dist2(float x, float y, float z)
{
	return x * x + y * y + z * z;
}

static void sortSingularValues(mat3& B, mat3& V)
{
	float* Bm = B.m;
	float* Vm = V.m;
	float rho1 = dist2(Bm[0], Bm[1], Bm[2]);
	float rho2 = dist2(Bm[3], Bm[4], Bm[5]);
	float rho3 = dist2(Bm[6], Bm[7], Bm[8]);

	bool c = rho1 < rho2;
	condNegSwap(c, Bm[0], Bm[3]); condNegSwap(c, Vm[0], Vm[3]);
	condNegSwap(c, Bm[1], Bm[4]); condNegSwap(c, Vm[1], Vm[4]);
	condNegSwap(c, Bm[2], Bm[5]); condNegSwap(c, Vm[2], Vm[5]);
	condSwap(c, rho1, rho2);
	c = rho1 < rho3;
	condNegSwap(c, Bm[0], Bm[6]); condNegSwap(c, Vm[0], Vm[6]);
	condNegSwap(c, Bm[1], Bm[7]); condNegSwap(c, Vm[1], Vm[7]);
	condNegSwap(c, Bm[2], Bm[8]); condNegSwap(c, Vm[2], Vm[8]);
	condSwap(c, rho1, rho3);
	c = rho2 < rho3;
	condNegSwap(c, Bm[3], Bm[6]); condNegSwap(c, Vm[3], Vm[6]);
	condNegSwap(c, Bm[4], Bm[7]); condNegSwap(c, Vm[4], Vm[7]);
	condNegSwap(c, Bm[5], Bm[8]); condNegSwap(c, Vm[5], Vm[8]);
}

static void QRDecomposition(mat3& B, mat3& Q, mat3& R)
{
	float ch1, sh1, ch2, sh2, ch3, sh3;
	float a, b;

	// first givens rotation (ch,0,0,sh)
	QRGivensQuaternion(B.m00, B.m10, ch1, sh1);
	a = 1 - 2 * sh1 * sh1;
	b = 2 * ch1 * sh1;

	// Apply B = Q' * B.
	float* Rm = R.m;
	float* Bm = B.m;
	Rm[0] = a * Bm[0] + b * Bm[1];  Rm[3] = a * Bm[3] + b * Bm[4];  Rm[6] = a * Bm[6] + b * Bm[7];
	Rm[1] = -b * Bm[0] + a * Bm[1]; Rm[4] = -b * Bm[3] + a * Bm[4]; Rm[7] = -b * Bm[6] + a * Bm[7];
	Rm[2] = Bm[2];          Rm[5] = Bm[5];          Rm[8] = Bm[8];

	// Second givens rotation (ch,0,-sh,0).
	QRGivensQuaternion(Rm[0], Rm[2], ch2, sh2);
	a = 1 - 2 * sh2 * sh2;
	b = 2 * ch2 * sh2;

	// Apply B = Q' * B.
	Bm[0] = a * Rm[0] + b * Rm[2];  Bm[3] = a * Rm[3] + b * Rm[5];  Bm[6] = a * Rm[6] + b * Rm[8];
	Bm[1] = Rm[1];           Bm[4] = Rm[4];           Bm[7] = Rm[7];
	Bm[2] = -b * Rm[0] + a * Rm[2]; Bm[5] = -b * Rm[3] + a * Rm[5]; Bm[8] = -b * Rm[6] + a * Rm[8];

	// Third givens rotation (ch,sh,0,0).
	QRGivensQuaternion(Bm[4], Bm[5], ch3, sh3);
	a = 1 - 2 * sh3 * sh3;
	b = 2 * ch3 * sh3;
	// R is now set to desired value.
	Rm[0] = Bm[0];             Rm[3] = Bm[3];           Rm[6] = Bm[6];
	Rm[1] = a * Bm[1] + b * Bm[2];     Rm[4] = a * Bm[4] + b * Bm[5];   Rm[7] = a * Bm[7] + b * Bm[8];
	Rm[2] = -b * Bm[1] + a * Bm[2];    Rm[5] = -b * Bm[4] + a * Bm[5];  Rm[8] = -b * Bm[7] + a * Bm[8];

	// Construct the cumulative rotation Q=Q1 * Q2 * Q3.
	// The number of floating point operations for three quaternion multiplications
	// is more or less comparable to the explicit form of the joined matrix.
	// Certainly more memory-efficient!
	float sh12 = sh1 * sh1;
	float sh22 = sh2 * sh2;
	float sh32 = sh3 * sh3;

	float* Qm = Q.m;
	Qm[0] = (-1 + 2 * sh12) * (-1 + 2 * sh22);
	Qm[3] = 4 * ch2 * ch3 * (-1 + 2 * sh12) * sh2 * sh3 + 2 * ch1 * sh1 * (-1 + 2 * sh32);
	Qm[6] = 4 * ch1 * ch3 * sh1 * sh3 - 2 * ch2 * (-1 + 2 * sh12) * sh2 * (-1 + 2 * sh32);

	Qm[1] = 2 * ch1 * sh1 * (1 - 2 * sh22);
	Qm[4] = -8 * ch1 * ch2 * ch3 * sh1 * sh2 * sh3 + (-1 + 2 * sh12) * (-1 + 2 * sh32);
	Qm[7] = -2 * ch3 * sh3 + 4 * sh1 * (ch3 * sh1 * sh3 + ch1 * ch2 * sh2 * (-1 + 2 * sh32));

	Qm[2] = 2 * ch2 * sh2;
	Qm[5] = 2 * ch3 * (1 - 2 * sh22) * sh3;
	Qm[8] = (-1 + 2 * sh22) * (-1 + 2 * sh32);
}

svd3 computeSVD(const mat3& A)
{
	mat3 ATA = transpose(A) * A;

	mat3 U, V;

	quat q;
	jacobiEigenanlysis(ATA.m00, ATA.m10, ATA.m11, ATA.m20, ATA.m21, ATA.m22, q);
	V = quaternionToMat3(q);

	mat3 B = A * V;

	sortSingularValues(B, V);
	mat3 S;
	QRDecomposition(B, U, S);

	return { U, V, vec3(S.m00, S.m11, S.m22) };
}
