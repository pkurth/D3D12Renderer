#include "pch.h"
#include "collision_gjk.h"



gjk_internal_success updateGJKSimplex(gjk_simplex& s, const gjk_support_point& a, vec3& dir)
{
	if (s.numPoints == 2)
	{
		// Triangle case.
		vec3 ao = -a.minkowski;
		vec3 ab = s.b.minkowski - a.minkowski;
		vec3 ac = s.c.minkowski - a.minkowski;
		vec3 abc = cross(ab, ac);

		vec3 abp = cross(ab, abc);
		if (dot(ao, abp) > 0.f)
		{
			s.c = a;
			dir = crossABA(ab, ao);
			return gjk_dont_stop;
		}
		vec3 acp = cross(abc, ac);
		if (dot(ao, acp) > 0.f)
		{
			s.b = a;
			dir = crossABA(ac, ao);
			return gjk_dont_stop;
		}

		// Sort so that normal abc of triangle points outside (negative new search dir).
		if (dot(ao, abc) >= 0.f)
		{
			s.d = s.b;
			s.b = a;
			s.numPoints = 3;
			dir = abc;
			return gjk_dont_stop;
		}
		if (dot(ao, -abc) >= 0.f)
		{
			s.d = s.c;
			s.c = s.b;
			s.b = a;
			s.numPoints = 3;
			dir = -abc;
			return gjk_dont_stop;
		}

		//ASSERT(false);
		std::cerr << "GJK ERROR 0\n";
		return gjk_unexpected_error;
	}
	if (s.numPoints == 3)
	{
		// Tetrahedron case.
		vec3 ao = -a.minkowski;
		vec3 ab = s.b.minkowski - a.minkowski;
		vec3 ac = s.c.minkowski - a.minkowski;
		vec3 ad = s.d.minkowski - a.minkowski;

		vec3 bcd = cross(s.c.minkowski - s.b.minkowski, s.d.minkowski - s.b.minkowski);
		if (dot(bcd, dir) > 0.00001f || dot(bcd, s.b.minkowski) < -0.00001f)
		{
			std::cerr << "GJK ERROR 0\n";
			return gjk_unexpected_error;
		}

		// Normals of faces (point outside).
		vec3 abc = cross(ac, ab);
		vec3 abd = cross(ab, ad);
		vec3 adc = cross(ad, ac);

		int32 flags = 0;
		const int32 overABCFlag = 1;
		const int32 overABDFlag = 2;
		const int32 overADCFlag = 4;

		flags |= (dot(abc, ao) > 0.f) ? overABCFlag : 0;
		flags |= (dot(abd, ao) > 0.f) ? overABDFlag : 0;
		flags |= (dot(adc, ao) > 0.f) ? overADCFlag : 0;

		if (flags == (overABCFlag | overABDFlag | overADCFlag))
		{
			std::cerr << "GJK ERROR 0\n";
			return gjk_unexpected_error;
		}

		if (flags == 0)
		{
			return gjk_stop;
		}

		if (flags == overABCFlag)
		{
		overABC1:
			if (dot(cross(abc, ab), ao) > 0.f)
			{
				// New line: ab
				s.c = a;
				s.numPoints = 2;
				dir = crossABA(ab, ao);
				return gjk_dont_stop;
			}
		overABC2:
			if (dot(cross(ac, abc), ao) > 0.f)
			{
				// New line: ac
				s.b = a;
				s.numPoints = 2;
				dir = crossABA(ac, ao);
				return gjk_dont_stop;
			}

			// Stay in triangle case: bca
			s.d = a;
			dir = abc;
			return gjk_dont_stop;
		}

		if (flags == overABDFlag)
		{
		overABD1:
			if (dot(cross(abd, ad), ao) > 0.f)
			{
				// New line: ad
				s.b = s.d;
				s.c = a;
				s.numPoints = 2;
				dir = crossABA(ad, ao);
				return gjk_dont_stop;
			}
		overABD2:
			if (dot(cross(ab, abd), ao) > 0.f)
			{
				// New line: ab
				s.c = a;
				s.numPoints = 2;
				dir = crossABA(ab, ao);
				return gjk_dont_stop;
			}

			// Stay in triangle case: bad
			s.c = a;
			dir = abd;
			return gjk_dont_stop;
		}

		if (flags == overADCFlag)
		{
		overADC1:
			if (dot(cross(adc, ac), ao) > 0.f)
			{
				// New line: ac
				s.b = a;
				s.numPoints = 2;
				dir = crossABA(ac, ao);
				return gjk_dont_stop;
			}
		overADC2:
			if (dot(cross(ad, adc), ao) > 0.f)
			{
				// New line: ad
				s.b = a;
				s.c = s.d;
				s.numPoints = 2;
				dir = crossABA(ad, ao);
				return gjk_dont_stop;
			}

			// Stay in triangle case: acd
			s.b = a;
			dir = adc;
			return gjk_dont_stop;
		}

		if (flags == (overABCFlag | overABDFlag))
		{
			if (dot(cross(abc, ab), ao) > 0.f)
			{
				goto overABD1;
			}
			goto overABC2;
		}

		if (flags == (overABDFlag | overADCFlag))
		{
			if (dot(cross(abd, ad), ao) > 0.f)
			{
				goto overADC1;
			}
			goto overABD2;
		}

		if (flags == (overADCFlag | overABCFlag))
		{
			if (dot(cross(adc, ac), ao) > 0.f)
			{
				goto overABC1;
			}
			goto overADC2;
		}

		//ASSERT(false);
		std::cerr << "GJK ERROR 1\n";
		return gjk_unexpected_error;
	}

	//ASSERT(false);
	std::cerr << "GJK ERROR 2\n";
	return gjk_unexpected_error;
}

