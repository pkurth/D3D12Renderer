#include "cs.hlsli"
#include "post_processing_rs.hlsli"
#include "brdf.hlsli"
#include "normal.hlsli"
#include "camera.hlsli"

ConstantBuffer<specular_ambient_cb> cb		: register(b0);
ConstantBuffer<camera_cb> camera			: register(b1);

RWTexture2D<float4> output					: register(u0);

Texture2D<float4> scene						: register(t0);
Texture2D<float2> worldNormals				: register(t1);
Texture2D<float4> reflectance				: register(t2);

Texture2D<float4> reflection				: register(t3);


TextureCube<float4> environmentTexture		: register(t4);
Texture2D<float4> brdf						: register(t5);

SamplerState clampSampler					: register(s0);


[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
[RootSignature(SPECULAR_AMBIENT_RS)]
void main(cs_input IN)
{
	float2 uv = (IN.dispatchThreadID.xy + 0.5f) * cb.invDimensions;

	float4 color = scene[IN.dispatchThreadID.xy];
	if (color.a > 0.f) // Alpha of 0 indicates sky.
	{
		float4 refl = reflectance[IN.dispatchThreadID.xy];

		surface_info surface;
		surface.N = normalize(unpackNormal(worldNormals[IN.dispatchThreadID.xy]));
		surface.V = -normalize(restoreWorldDirection(camera.invViewProj, uv, camera.position.xyz));
		surface.roughness = refl.a;

		surface.inferRemainingProperties();

		float4 ssr = reflection.SampleLevel(clampSampler, uv, 0);

		float3 specular = specularIBL(refl.rgb, surface, environmentTexture, brdf, clampSampler);
		specular = lerp(specular, ssr.rgb, ssr.a);

		color.rgb += specular;
	}
	output[IN.dispatchThreadID.xy] = color;
}
