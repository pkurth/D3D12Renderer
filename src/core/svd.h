#pragma once

#include "core/math.h"

struct svd3
{
	mat3 U;
	mat3 V;
	vec3 singularValues;
};

svd3 computeSVD(const mat3& A);
