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

static vec3 linearToSRGB(vec3 color)
{
	// Approximately pow(color, 1.0 / 2.2).
	float r = color.r < 0.0031308f ? 12.92f * color.r : 1.055f * pow(abs(color.r), 1.f / 2.4f) - 0.055f;
	float g = color.g < 0.0031308f ? 12.92f * color.g : 1.055f * pow(abs(color.g), 1.f / 2.4f) - 0.055f;
	float b = color.b < 0.0031308f ? 12.92f * color.b : 1.055f * pow(abs(color.b), 1.f / 2.4f) - 0.055f;
	return vec3(r, g, b);
}

static vec3 sRGBToLinear(vec3 color)
{
	// Approximately pow(color, 2.2).
	float r = color.r < 0.04045f ? color.r / 12.92f : pow(abs(color.r + 0.055f) / 1.055f, 2.4f);
	float g = color.g < 0.04045f ? color.g / 12.92f : pow(abs(color.g + 0.055f) / 1.055f, 2.4f);
	float b = color.b < 0.04045f ? color.b / 12.92f : pow(abs(color.b + 0.055f) / 1.055f, 2.4f);
	return vec3(r, g, b);
}

static vec3 rec709ToRec2020(vec3 color)
{
	static const mat3 conversion =
	{
		0.627402f, 0.329292f, 0.043306f,
		0.069095f, 0.919544f, 0.011360f,
		0.016394f, 0.088028f, 0.895578f
	};
	return conversion * color;
}

static vec3 rec2020ToRec709(vec3 color)
{
	static const mat3 conversion =
	{
		1.660496f, -0.587656f, -0.072840f,
		-0.124547f, 1.132895f, -0.008348f,
		-0.018154f, -0.100597f, 1.118751f
	};
	return conversion * color;
}

static vec3 linearToST2084(vec3 color)
{
	float m1 = 2610.f / 4096.f / 4.f;
	float m2 = 2523.f / 4096.f * 128.f;
	float c1 = 3424.f / 4096.f;
	float c2 = 2413.f / 4096.f * 32.f;
	float c3 = 2392.f / 4096.f * 32.f;
	vec3 cp = pow(abs(color), m1);
	return pow((c1 + c2 * cp) / (1.f + c3 * cp), m2);
}

static vec3 YxyToXYZ(vec3 Yxy)
{
	float Y = Yxy.r;
	float x = Yxy.g;
	float y = Yxy.b;

	float X = x * (Y / y);
	float Z = (1.f - x - y) * (Y / y);

	return vec3(X, Y, Z);
}

static vec3 XYZToRGB(vec3 XYZ)
{
	// CIE/E.
	static const mat3 M =
	{
		2.3706743f, -0.9000405f, -0.4706338f,
		-0.5138850f, 1.4253036f, 0.0885814f,
		0.0052982f, -0.0146949f, 1.0093968f
	};

	return M * XYZ;
}

static vec3 YxyToRGB(vec3 Yxy)
{
	vec3 XYZ = YxyToXYZ(Yxy);
	vec3 RGB = XYZToRGB(XYZ);
	return RGB;
}

