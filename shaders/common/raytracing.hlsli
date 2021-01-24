#ifndef RAYTRACING_H
#define RAYTRACING_H


struct raytracing_cb
{
	uint32 maxRecursionDepth;
	float fadeoutDistance;
	float maxRayDistance;
	float environmentIntensity;
	float skyIntensity;
};

struct path_tracing_cb
{
	uint32 frameCount;
	uint32 numAccumulatedFrames;
	uint32 maxRecursionDepth;
	uint32 startRussianRouletteAfter;

	uint32 useThinLensCamera;
	float focalLength;
	float lensRadius;

	uint32 useRealMaterials;
	uint32 enableDirectLighting;
	float lightIntensityScale;
	float pointLightRadius;

	uint32 multipleImportanceSampling;
};


#ifdef HLSL

static float3 hitWorldPosition()
{
	return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

static float3 transformPositionToWorld(float3 position)
{
	float3x4 M = ObjectToWorld3x4();
	return mul(M, float4(position, 1.f)).xyz;
}

static float3 transformDirectionToWorld(float3 direction)
{
	float3x4 M = ObjectToWorld3x4();
	return normalize(mul(M, float4(direction, 0.f)).xyz);
}

static float2 interpolateAttribute(float2 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attribs)
{
	return vertexAttribute[0] +
		attribs.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
		attribs.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

static float3 interpolateAttribute(float3 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attribs)
{
	return vertexAttribute[0] +
		attribs.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
		attribs.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

static float4 interpolateAttribute(float4 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attribs)
{
	return vertexAttribute[0] +
		attribs.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
		attribs.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

static uint3 load3x16BitIndices(ByteAddressBuffer meshIndices)
{
	const uint indexSizeInBytes = 2;
	const uint indicesPerTriangle = 3;
	const uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
	uint baseIndex = PrimitiveIndex() * triangleIndexStride;

	uint3 indices;

	// ByteAdressBuffer loads must be aligned at a 4 byte boundary.
	// Since we need to read three 16 bit indices: { 0, 1, 2 } 
	// aligned at a 4 byte boundary as: { 0 1 } { 2 0 } { 1 2 } { 0 1 } ...
	// we will load 8 bytes (~ 4 indices { a b | c d }) to handle two possible index triplet layouts,
	// based on first index's baseIndex being aligned at the 4 byte boundary or not:
	//  Aligned:     { 0 1 | 2 - }
	//  Not aligned: { - 0 | 1 2 }
	const uint dwordAlignedOffset = baseIndex & ~3;
	const uint2 four16BitIndices = meshIndices.Load2(dwordAlignedOffset);

	// Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
	if (dwordAlignedOffset == baseIndex)
	{
		indices.x = four16BitIndices.x & 0xffff;
		indices.y = (four16BitIndices.x >> 16) & 0xffff;
		indices.z = four16BitIndices.y & 0xffff;
	}
	else // Not aligned: { - 0 | 1 2 } => retrieve last three 16bit indices
	{
		indices.x = (four16BitIndices.x >> 16) & 0xffff;
		indices.y = four16BitIndices.y & 0xffff;
		indices.z = (four16BitIndices.y >> 16) & 0xffff;
	}

	return indices;
}

static uint3 load3x32BitIndices(ByteAddressBuffer meshIndices)
{
	const uint indexSizeInBytes = 4;
	const uint indicesPerTriangle = 3;
	const uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
	uint baseIndex = PrimitiveIndex() * triangleIndexStride;

	uint3 indices = meshIndices.Load3(baseIndex);

	return indices;
}

#endif


#endif
