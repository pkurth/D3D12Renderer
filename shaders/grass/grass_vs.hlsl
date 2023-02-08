#include "grass_rs.hlsli"
#include "camera.hlsli"
#include "random.hlsli"

ConstantBuffer<grass_cb> cb				: register(b0);
ConstantBuffer<camera_cb> camera		: register(b1);
StructuredBuffer<grass_blade> blades	: register(t0);

struct vs_output
{
	float2 uv		: TEXCOORDS;
	float3 normal	: NORMAL;
	float4 position : SV_POSITION;
};

static float getRelY(uint vertexID, uint lod)
{
	uint numLevels = cb.numVertices >> 1 >> lod;

	uint vertical = vertexID >> 1 >> lod;
	return (float)vertical * (1.f / numLevels);
}

vs_output main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
	grass_blade blade = blades[instanceID];

	uint leftRight = vertexID & 1;
	float relX = (float)leftRight * 2.f - 1.f;
	relX *= (vertexID != cb.numVertices - 1);

	float relY0 = getRelY(vertexID, 0);
	float relY1 = getRelY(vertexID, 1);

	float relY = lerp(relY0, relY1, blade.lod());

	float2 uv = float2(relX, relY);


	float relTipOffsetZ = 0.7f;
	float controlPointZ = relTipOffsetZ * 0.5f;
	float controlPointY = 0.8f;

	float relY2 = relY * relY;
	float2 yz = float2(controlPointY, controlPointZ) * (2.f * relY - 2.f * relY2) + float2(1.f, relTipOffsetZ) * relY2;
	//yz = float2(relY, 0.f); // REMOVE
	float2 d_yz = float2(controlPointY, controlPointZ) * (2.f - 4.f * relY) + float2(1.f, relTipOffsetZ) * (2.f * relY);

	yz *= cb.height;

	float x = relX * cb.halfWidth;
	float y = yz.x;
	float z = yz.y;

	float nx = 0.f;
	float ny = -d_yz.y;
	float nz = d_yz.x;


	//blade.facing = float2(0.f, 1.f); // REMOVE

	// Apply rotation.
	float3 position = float3(
		blade.facing.y * x - blade.facing.x * z,
		y, 
		blade.facing.x * x + blade.facing.y * z);

	// Add wind.
	float windStrength = fbm(blade.position.xz * 0.6f + cb.time * 0.3f + 10000.f).x + 0.6f;
	float3 xzOffset = cb.windDirection * windStrength * relY;
	//xzOffset = float3(0.f, 0.f, 0.f); // REMOVE
	position += xzOffset;

	position += blade.position;


	float3 normal = float3(
		blade.facing.y * nx - blade.facing.x * nz,
		ny,
		blade.facing.x * nx + blade.facing.y * nz);



	vs_output OUT;
	OUT.uv = uv;
	OUT.normal = normal;
	OUT.position = mul(camera.viewProj, float4(position, 1.f));
	return OUT;
}
