#include "cs.hlsli"
#include "post_processing_rs.hlsli"


ConstantBuffer<present_cb> present	: register(b0);
RWTexture2D<float4> output	: register(u0);
Texture2D<float4>	input	: register(t0);


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



[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
[RootSignature(PRESENT_RS)]
void main(cs_input IN)
{
	float3 scene = input[IN.dispatchThreadID.xy + int2(0, 0)].rgb;

	if (present.sharpenStrength > 0.f)
	{
		float3 top = input[IN.dispatchThreadID.xy + int2(0, -1)].rgb;
		float3 left = input[IN.dispatchThreadID.xy + int2(-1, 0)].rgb;
		float3 right = input[IN.dispatchThreadID.xy + int2(1, 0)].rgb;
		float3 bottom = input[IN.dispatchThreadID.xy + int2(0, 1)].rgb;

		scene = max(scene + (4.f * scene - top - bottom - left - right) * present.sharpenStrength, 0.f);
	}

	if (present.displayMode == present_sdr)
	{
		scene = linearToSRGB(scene);
	}
	else if (present.displayMode == present_hdr)
	{
		const float st2084max = 10000.f;
		const float hdrScalar = present.standardNits / st2084max;

		// The HDR scene is in Rec.709, but the display is Rec.2020.
		scene = rec709ToRec2020(scene);

		// Apply the ST.2084 curve to the scene.
		scene = linearToST2084(scene * hdrScalar);
	}

	output[IN.dispatchThreadID.xy + int2(present.offset >> 16, present.offset & 0xFFFF)] = float4(scene, 1.f);
}
