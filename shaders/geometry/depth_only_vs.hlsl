#include "depth_only_rs.hlsli"
#include "camera.hlsli"

StructuredBuffer<float4x4> transforms			: register(t0);
StructuredBuffer<float4x4> prevFrameTransforms	: register(t1);
StructuredBuffer<uint> objectIDs				: register(t2);
ConstantBuffer<camera_cb> camera				: register(b0, space1);

struct vs_input
{
	float3 position				: POSITION;
	float3 prevFramePosition	: PREV_POSITION;

#ifdef ALPHA_CUTOUT
	float2 uv					: TEXCOORDS;
#endif

	uint instanceID				: SV_InstanceID;
};

struct vs_output
{
#ifdef ALPHA_CUTOUT
	float2 uv						: TEXCOORDS;
#endif

	float3 ndc						: NDC;
	float3 prevFrameNDC				: PREV_FRAME_NDC;

	nointerpolation uint objectID	: OBJECT_ID;

	float4 position					: SV_POSITION;
};

#ifndef RS
#define RS DEPTH_ONLY_RS
#endif

[RootSignature(RS)]
vs_output main(vs_input IN)
{
	float4 worldPosition = mul(transforms[IN.instanceID], float4(IN.position, 1.f));
	float4 prevFrameWorldPosition = mul(prevFrameTransforms[IN.instanceID], float4(IN.prevFramePosition, 1.f));

	vs_output OUT;
	OUT.position = mul(camera.viewProj, worldPosition);
	OUT.ndc = OUT.position.xyw;
	OUT.prevFrameNDC = mul(camera.prevFrameViewProj, prevFrameWorldPosition).xyw;
	OUT.objectID = objectIDs[IN.instanceID];

#ifdef ALPHA_CUTOUT
	OUT.uv = IN.uv;
#endif

	return OUT;
}
