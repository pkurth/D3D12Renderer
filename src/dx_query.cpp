#include "pch.h"
#include "dx_query.h"
#include "dx_context.h"

void dx_timestamp_query_heap::initialize(uint32 maxCount)
{
	D3D12_QUERY_HEAP_DESC desc;
	desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	desc.Count = maxCount;
	desc.NodeMask = 0;

	checkResult(dxContext.device->CreateQueryHeap(&desc, IID_PPV_ARGS(&heap)));
}
