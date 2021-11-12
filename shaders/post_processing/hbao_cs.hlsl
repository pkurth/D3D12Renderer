#include "cs.hlsli"
#include "post_processing_rs.hlsli"
#include "camera.hlsli"
#include "normal.hlsli"

ConstantBuffer<hbao_cb> cb				: register(b0);
ConstantBuffer<camera_cb> camera		: register(b1);

Texture2D<float> depthBuffer			: register(t0);
Texture2D<float2> normalTexture			: register(t1);

RWTexture2D<float> resultTexture		: register(u0);

SamplerState linearSampler				: register(s0);

static const float2 sampleDirections[32] =
{
	float2(0.3659715790983448f, -0.9306260276245577f),
	float2(-0.9999833371934095f, -0.005772810020429772f),
	float2(-0.2614489710714677f, -0.9652172996406928f),
	float2(-0.6421558525368186f, -0.7665741066933527f),
	float2(-0.655479940981266f, 0.7552125839597722f),
	float2(-0.07920499519917198f, -0.9968583493834513f),
	float2(-0.7238770228419326f, 0.6899290222925111f),
	float2(-0.9935787234255465f, 0.11314292004390468f),
	float2(0.904167511638826f, 0.4271780786707733f),
	float2(-0.09542247728397345f, 0.9954368643108359f),
	float2(0.9921624951558193f, 0.12495432447970235f),
	float2(-0.970041527120819f, 0.2429391604108099f),
	float2(-0.5893412584414545f, -0.8078842003026442f),
	float2(-0.6293914687063439f, 0.7770883985234056f),
	float2(-0.9595660401111906f, -0.28148359573042425f),
	float2(-0.9610157207306117f, 0.2764937332176319f),
	float2(0.4944610411693459f, -0.869199792202993f),
	float2(0.10014828130541988f, 0.994972523113865f),
	float2(-0.29715312460309856f, 0.9548298385254911f),
	float2(0.9538395321602133f, 0.30031674426908894f),
	float2(-0.9977463494755803f, -0.06709859989711618f),
	float2(-0.8778395421253317f, 0.4789548395007488f),
	float2(0.557554723126058f, 0.8301401873899521f),
	float2(-0.989295975868064f, 0.14592282936968815f),
	float2(0.9878087082656999f, 0.1556725919179397f),
	float2(0.23683594573354044f, -0.971549656378149f),
	float2(-0.8055372148585417f, 0.5925451843344469f),
	float2(0.2991435323607186f, 0.9542081256448992f),
	float2(0.4869690691924386f, 0.8734192152968985f),
	float2(-0.1937220499646552f, 0.981056454724952f),
	float2(-0.599313287615268f, -0.8005145740633202f),
	float2(-0.35297338935059924f, 0.9356333611037767f),
};

// https://www.derschmale.com/source/hbao/HBAOFragmentShader.hlsl

static float2 snapToTexel(float2 uv, float2 screenDims, float2 invScreenDims)
{
	return round(uv * screenDims) * invScreenDims;
}

[RootSignature(HBAO_RS)]
[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
void main(cs_input IN)
{
	if (IN.dispatchThreadID.x >= cb.screenWidth || IN.dispatchThreadID.y >= cb.screenHeight) 
	{
		return;
	}

	float2 screenDims = float2(cb.screenWidth, cb.screenHeight);
	float2 invScreenDims = rcp(screenDims);;

	float2 centerUV = (IN.dispatchThreadID.xy + float2(0.5f, 0.5f)) * invScreenDims;
	float centerDepth = depthBuffer.SampleLevel(linearSampler, centerUV, cb.depthBufferMipLevel);

	float3 centerPos = camera.restoreViewSpacePositionEyeDepth(centerUV, centerDepth);
	float3 centerNormal = mul(camera.view, float4(unpackNormal(normalTexture.SampleLevel(linearSampler, centerUV, 0)), 0.f)).xyz;

	float3 random = float3(1.f, 0.f, 0.f); // TODO: cos(angle), sin(angle), jitter.
	float2 rotationX = normalize(random.xy - 0.5f);
	float jitter = random.z;

	float2x2 rotation = float2x2(
		rotationX,
		rotationX.yx * float2(-1.f, 1.f)
	);

	float2 projectedRadii = (cb.halfRadius * camera.proj._m00_m11) / -centerPos.z; // With half radius we scale the [-1, 1] by half.

	float screenRadius = projectedRadii.x * screenDims.x;

	float result = 1.f;
	if (screenRadius >= 1.f)
	{
		uint numStepsPerRay = min(cb.maxNumSteps, screenRadius);
		float totalOcclusion = 0.f;

		for (uint rayIndex = 0; rayIndex < cb.numSampleDirections; ++rayIndex)
		{
			float2 direction = mul(rotation, sampleDirections[rayIndex]);
			float2 uvStep = direction * invScreenDims;

			direction *= projectedRadii;

			float2 uv = centerUV + uvStep;
			float depth = depthBuffer.SampleLevel(linearSampler, uv, cb.depthBufferMipLevel);
			float3 tangent = camera.restoreViewSpacePositionEyeDepth(uv, depth) - centerPos;
			tangent -= dot(centerNormal, tangent) * centerNormal;

			float2 snappedUVStep = snapToTexel(direction.xy / (numStepsPerRay - 1), screenDims, invScreenDims);

			// Jitter the starting position for ray marching between the nearest neighbour and the sample step size.
			float2 jitteredOffset = lerp(uvStep, snappedUVStep, jitter);
			uv = snapToTexel(centerUV + jitteredOffset, screenDims, invScreenDims);

			float topOcclusion = cb.bias;
			float occlusion = 0.f;

			for (uint stepIndex = 0; stepIndex < numStepsPerRay; ++stepIndex)
			{
				float depth = depthBuffer.SampleLevel(linearSampler, uv, cb.depthBufferMipLevel);
				float3 samplePos = camera.restoreViewSpacePositionEyeDepth(uv, depth);

				float3 horizonVector = samplePos - centerPos;
				float horizonVectorLength = length(horizonVector);

				float sampleOcclusion = 0.5f;

				if (dot(tangent, horizonVector) >= 0.f)
				{
					sampleOcclusion = dot(centerNormal, horizonVector) / horizonVectorLength;

					// This adds occlusion only if angle of the horizon vector is higher than the previous highest one without branching.
					float diff = max(sampleOcclusion - topOcclusion, 0.f);
					topOcclusion = max(sampleOcclusion, topOcclusion);

					// Attenuate occlusion contribution using distance function 1 - (d/f)^2.
					float distanceFactor = saturate(horizonVectorLength / cb.falloff);
					distanceFactor = 1.f - distanceFactor * distanceFactor;
					sampleOcclusion = diff * distanceFactor;
				}

				occlusion += sampleOcclusion;

				uv += snappedUVStep;
			}

			totalOcclusion += occlusion;
		}

		result = 1.f - saturate(totalOcclusion * cb.strength / cb.numSampleDirections);
	}

	resultTexture[IN.dispatchThreadID.xy] = result;
}
