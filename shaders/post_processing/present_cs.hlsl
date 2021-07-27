#include "cs.hlsli"
#include "post_processing_rs.hlsli"
#include "color.hlsli"


ConstantBuffer<present_cb> present	: register(b0);
RWTexture2D<float4> output	: register(u0);
Texture2D<float4>	input	: register(t0);




[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
[RootSignature(PRESENT_RS)]
void main(cs_input IN)
{
	int xOffset = present.offset >> 16;
	int yOffset = present.offset & 0xFFFF;

	int2 center = IN.dispatchThreadID.xy - int2(xOffset, yOffset);

	float3 scene = input[center + int2(0, 0)].rgb;

	if (present.sharpenStrength > 0.f)
	{
		float3 top = input[center + int2(0, -1)].rgb;
		float3 left = input[center + int2(-1, 0)].rgb;
		float3 right = input[center + int2(1, 0)].rgb;
		float3 bottom = input[center + int2(0, 1)].rgb;

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

	output[IN.dispatchThreadID.xy] = float4(scene, 1.f);
}
