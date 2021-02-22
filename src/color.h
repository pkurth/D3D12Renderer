#pragma once

#include "math.h"
#include "random.h"

// RGB is in [0,1]^3.
// HSV is in [0,2pi]x[0,1]x[0,1]

static vec3 rgb2hsv(vec3 rgb)
{
	float cmax = max(max(rgb.x, rgb.y), rgb.z);
	float cmin = min(min(rgb.x, rgb.y), rgb.z);
	float delta = cmax - cmin;
	float degrees =
		(delta == 0.f) ? 0.f :
		(cmax == rgb.x) ? (60.f * fmod((rgb.y - rgb.z) / delta, 6.f)) :
		(cmax == rgb.y) ? (60.f * ((rgb.z - rgb.x) / delta + 2.f)) :
		(60.f * ((rgb.x - rgb.y) / delta + 4.f));
	float h = angleToZeroToTwoPi(deg2rad(degrees));
	float s = (cmax == 0) ? 0.f : (delta / cmax);
	float v = cmax;

	return vec3(h, s, v);
}

static vec3 hsv2rgb(vec3 hsv)
{
	float h = rad2deg(hsv.x);
	float s = hsv.y;
	float v = hsv.z;

	float c = v * s;
	float x = c * (1.f - abs(fmod(h / 60.f, 2.f) - 1.f));
	float m = v - c;

	vec3 rgb =
		(h < 60.f) ? vec3(c, x, 0.f) :
		(h < 120.f) ? vec3(x, c, 0.f) :
		(h < 180.f) ? vec3(0.f, c, x) :
		(h < 240.f) ? vec3(0.f, x, c) :
		(h < 300.f) ? vec3(x, 0.f, c) :
		vec3(c, 0.f, x);

	rgb += vec3(m, m, m);
	return rgb;
}

static vec3 randomRGB(random_number_generator& rng)
{
	vec3 hsv =
	{
		rng.randomFloatBetween(0.f, 2.f * M_PI),
		1.f,
		1.f,
	};
	return hsv2rgb(hsv);
}

