#include "pch.h"
#include "heightmap_collider.h"



//heightmap_area heightmap_collider::getHeightsInArea(vec2 minCorner, vec2 maxCorner)
//{
//	vec2 sMinCorner = positionInSegmentSpace(minCorner);
//	vec2 sMaxCorner = positionInSegmentSpace(maxCorner);
//
//	int32 minX = max((int32)floor(sMinCorner.x), 0);
//	int32 minY = max((int32)floor(sMinCorner.y), 0);
//	int32 maxX = min((int32)ceil(sMaxCorner.x), (int32)numVerticesPerDim - 1);
//	int32 maxY = min((int32)ceil(sMaxCorner.y), (int32)numVerticesPerDim - 1);
//
//	int32 minIndex = minY * numVerticesPerDim + minX;
//
//	heightmap_area result;
//	result.heights = heights + minIndex;
//	result.numX = maxX - minX + 1;
//	result.numY = maxY - minY + 1;
//	result.strideX = (int32)numVerticesPerDim;
//	result.minHeight = 0.f; // TODO
//	result.maxHeight = 1.f; // TODO
//	result.minCorner = minCorner + vec2((float)minX, (float)minY) * segmentSize;
//	result.segmentSize = segmentSize;
//	return result;
//}

//vec2 heightmap_collider::positionInSegmentSpace(vec2 position)
//{
//	return (position - vec2(minCorner.x, minCorner.z)) * invSegmentSize;
//}

void terrain_collider_context::update(vec3 minCorner, float amplitudeScale)
{
	this->minCorner = minCorner;
	this->invAmplitudeScale = 1.f / amplitudeScale;
}
