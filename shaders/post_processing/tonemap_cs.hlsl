#include "cs.hlsli"
#include "post_processing_rs.hlsli"


ConstantBuffer<tonemap_cb> tonemap	: register(b0);
RWTexture2D<float4> output	: register(u0);
Texture2D<float4>	input	: register(t0);

static float3 acesFilmic(float3 x, float A, float B, float C, float D, float E, float F)
{
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - (E / F);
}

static float3 filmicTonemapping(float3 color)
{
	color *= exp2(tonemap.exposure);

	return acesFilmic(color, tonemap.A, tonemap.B, tonemap.C, tonemap.D, tonemap.E, tonemap.F) /
		acesFilmic(tonemap.linearWhite, tonemap.A, tonemap.B, tonemap.C, tonemap.D, tonemap.E, tonemap.F);
}

[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
[RootSignature(TONEMAP_RS)]
void main(cs_input IN)
{
	output[IN.dispatchThreadID.xy] = float4(
		//filmicTonemapping(
			input[IN.dispatchThreadID.xy].rgb
		//)
		, 1.f);
}
