#ifndef COLOR_HLSLI
#define COLOR_HLSLI

static float3 linearToSRGB(float3 color)
{
	// Approximately pow(color, 1.0 / 2.2).
	return color < 0.0031308f ? 12.92f * color : 1.055f * pow(abs(color), 1.f / 2.4f) - 0.055f;
}

static float3 sRGBToLinear(float3 color)
{
	// Approximately pow(color, 2.2).
	return color < 0.04045f ? color / 12.92f : pow(abs(color + 0.055f) / 1.055f, 2.4f);
}

static float3 rec709ToRec2020(float3 color)
{
	static const float3x3 conversion =
	{
		0.627402f, 0.329292f, 0.043306f,
		0.069095f, 0.919544f, 0.011360f,
		0.016394f, 0.088028f, 0.895578f
	};
	return mul(conversion, color);
}

static float3 rec2020ToRec709(float3 color)
{
	static const float3x3 conversion =
	{
		1.660496f, -0.587656f, -0.072840f,
		-0.124547f, 1.132895f, -0.008348f,
		-0.018154f, -0.100597f, 1.118751f
	};
	return mul(conversion, color);
}

static float3 linearToST2084(float3 color)
{
	float m1 = 2610.f / 4096.f / 4.f;
	float m2 = 2523.f / 4096.f * 128.f;
	float c1 = 3424.f / 4096.f;
	float c2 = 2413.f / 4096.f * 32.f;
	float c3 = 2392.f / 4096.f * 32.f;
	float3 cp = pow(abs(color), m1);
	return pow((c1 + c2 * cp) / (1.f + c3 * cp), m2);
}

static float3 YxyToXYZ(float3 Yxy)
{
	float Y = Yxy.r;
	float x = Yxy.g;
	float y = Yxy.b;

	float X = x * (Y / y);
	float Z = (1.f - x - y) * (Y / y);

	return float3(X, Y, Z);
}

static float3 XYZToRGB(float3 XYZ)
{
	// CIE/E.
	static const float3x3 M = 
	{
		2.3706743f, -0.9000405f, -0.4706338f,
		-0.5138850f, 1.4253036f, 0.0885814f,
		0.0052982f, -0.0146949f, 1.0093968f
	};

	return mul(M, XYZ);
}

static float3 YxyToRGB(float3 Yxy)
{
	float3 XYZ = YxyToXYZ(Yxy);
	float3 RGB = XYZToRGB(XYZ);
	return RGB;
}

#endif
