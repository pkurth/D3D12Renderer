#pragma once

#include "math.h"

struct pbr_decal
{
	trs transform;

	vec4 albedoTint;
	float roughness;
	float metallic;
};
