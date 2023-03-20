#include "grass_rs.hlsli"
#include "depth_only_rs.hlsli"
#include "grass_vertex.hlsli"
#include "camera.hlsli"


ConstantBuffer<grass_cb> cb							: register(b0);
ConstantBuffer<depth_only_object_id_cb> id			: register(b1);

ConstantBuffer<camera_cb> camera					: register(b0, space1);

StructuredBuffer<grass_blade> blades				: register(t0);


struct vs_output
{
	float3 ndc						: NDC;
	float3 prevFrameNDC				: PREV_FRAME_NDC;

	nointerpolation uint objectID	: OBJECT_ID;

	float4 position					: SV_POSITION;
};

[RootSignature(GRASS_DEPTH_ONLY_RS)]
vs_output main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
	grass_bend_settings bendSettings;
	bendSettings.relTipOffsetZ = 0.7f;
	bendSettings.controlPointZ = bendSettings.relTipOffsetZ * 0.5f;
	bendSettings.controlPointY = 0.8f;



	grass_blade blade = blades[instanceID];

	float2 uv = grassUV(blade, vertexID, cb.numVertices);
	float2 wind = cb.windDirection * blade.windStrength();
	float2 prevFrameWind = cb.windDirection * blade.prevFrameWindStrength();
	float3 position = grassPosition(blade, uv, blade.height, cb.halfWidth, bendSettings, wind);

	float3 prevFramePosition = position;
	prevFramePosition.xz += uv.y * (prevFrameWind - wind);

	vs_output OUT;
	OUT.position = mul(camera.viewProj, float4(position, 1.f));
	OUT.ndc = OUT.position.xyw;
	OUT.prevFrameNDC = mul(camera.prevFrameViewProj, float4(prevFramePosition, 1.f)).xyw;
	OUT.objectID = id.id;
	return OUT;
}
