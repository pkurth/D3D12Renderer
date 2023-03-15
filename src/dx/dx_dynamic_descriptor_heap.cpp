#include "pch.h"
#include "dx_dynamic_descriptor_heap.h"
#include "dx_command_list.h"
#include "dx_context.h"

void dx_dynamic_descriptor_heap::initialize(uint32 numDescriptorsPerHeap)
{
	this->numDescriptorsPerHeap = numDescriptorsPerHeap;
	descriptorTableBitMask = 0;
	staleDescriptorTableBitMask = 0;
	currentCPUDescriptorHandle = D3D12_DEFAULT;
	currentGPUDescriptorHandle = D3D12_DEFAULT;
	numFreeHandles = 0;

	descriptorHandleCache.resize(numDescriptorsPerHeap);
}

void dx_dynamic_descriptor_heap::stageDescriptors(uint32 rootParameterIndex, uint32 offset, uint32 numDescriptors, dx_cpu_descriptor_handle srcDescriptor)
{
	ASSERT(numDescriptors <= numDescriptorsPerHeap && rootParameterIndex < maxDescriptorTables);

	descriptor_table_cache& cache = descriptorTableCache[rootParameterIndex];

	ASSERT((offset + numDescriptors) <= cache.numDescriptors);

	D3D12_CPU_DESCRIPTOR_HANDLE* dstDescriptor = (cache.baseDescriptor + offset);
	for (uint32 i = 0; i < numDescriptors; ++i)
	{
		dstDescriptor[i] = srcDescriptor + i;
	}

	setBit(staleDescriptorTableBitMask, rootParameterIndex);
}

void dx_dynamic_descriptor_heap::commitStagedDescriptors(dx_command_list* commandList, bool graphics)
{
	uint32 numDescriptorsToCommit = computeStaleDescriptorCount();

	if (numDescriptorsToCommit > 0)
	{
		if (!currentDescriptorHeap || numFreeHandles < numDescriptorsToCommit)
		{
			currentDescriptorHeap = requestDescriptorHeap();
			currentCPUDescriptorHandle = currentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			currentGPUDescriptorHandle = currentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
			numFreeHandles = numDescriptorsPerHeap;

			commandList->setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, currentDescriptorHeap);

			// When updating the descriptor heap on the command list, all descriptor
			// tables must be (re)recopied to the new descriptor heap (not just
			// the stale descriptor tables).
			staleDescriptorTableBitMask = descriptorTableBitMask;
		}

		DWORD rootIndex;
		while (_BitScanForward(&rootIndex, staleDescriptorTableBitMask))
		{
			uint32 numSrcDescriptors = descriptorTableCache[rootIndex].numDescriptors;
			D3D12_CPU_DESCRIPTOR_HANDLE* srcDescriptorHandles = descriptorTableCache[rootIndex].baseDescriptor;
			D3D12_CPU_DESCRIPTOR_HANDLE destDescriptorRangeStart = currentCPUDescriptorHandle;

			dxContext.device->CopyDescriptors(
				1, &destDescriptorRangeStart, &numSrcDescriptors,
				numSrcDescriptors, srcDescriptorHandles, nullptr, 
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			if (graphics)
			{
				commandList->setGraphicsDescriptorTable(rootIndex, currentGPUDescriptorHandle);
			}
			else
			{
				commandList->setComputeDescriptorTable(rootIndex, currentGPUDescriptorHandle);
			}

			currentCPUDescriptorHandle += numSrcDescriptors;
			currentGPUDescriptorHandle += numSrcDescriptors;
			numFreeHandles -= numSrcDescriptors;

			unsetBit(staleDescriptorTableBitMask, rootIndex);
		}
	}
}

void dx_dynamic_descriptor_heap::commitStagedDescriptorsForDraw(dx_command_list* commandList)
{
	commitStagedDescriptors(commandList, true);
}

void dx_dynamic_descriptor_heap::commitStagedDescriptorsForDispatch(dx_command_list* commandList)
{
	commitStagedDescriptors(commandList, false);
}

void dx_dynamic_descriptor_heap::setCurrentDescriptorHeap(dx_command_list* commandList)
{
	commandList->setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, currentDescriptorHeap);
}

void dx_dynamic_descriptor_heap::parseRootSignature(const dx_root_signature& rootSignature)
{
	staleDescriptorTableBitMask = 0;

	descriptorTableBitMask = rootSignature.tableRootParameterMask;
	uint32 numDescriptorTables = rootSignature.numDescriptorTables;

	uint32 bitmask = descriptorTableBitMask;
	uint32 currentOffset = 0;
	DWORD rootIndex;
	uint32 descriptorTableIndex = 0;
	while (_BitScanForward(&rootIndex, bitmask))
	{
		uint32 numDescriptors = rootSignature.descriptorTableSizes[descriptorTableIndex++];

		descriptor_table_cache& cache = descriptorTableCache[rootIndex];
		cache.numDescriptors = numDescriptors;
		cache.baseDescriptor = &descriptorHandleCache[currentOffset];

		currentOffset += numDescriptors;

		// Flip the descriptor table bit so it's not scanned again for the current index.
		unsetBit(bitmask, rootIndex);
	}

	ASSERT(currentOffset <= numDescriptorsPerHeap); // The root signature requires more than the maximum number of descriptors per descriptor heap. Consider increasing the maximum number of descriptors per descriptor heap.
}

void dx_dynamic_descriptor_heap::reset()
{
	freeDescriptorHeaps = descriptorHeapPool;
	currentDescriptorHeap.Reset();
	currentCPUDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
	currentGPUDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
	numFreeHandles = 0;
	descriptorTableBitMask = 0;
	staleDescriptorTableBitMask = 0;

	// Reset the table cache.
	for (int i = 0; i < maxDescriptorTables; ++i)
	{
		descriptorTableCache[i].reset();
	}
}

com<ID3D12DescriptorHeap> dx_dynamic_descriptor_heap::requestDescriptorHeap()
{
	com<ID3D12DescriptorHeap> descriptorHeap;
	if (freeDescriptorHeaps.size() > 0)
	{
		descriptorHeap = freeDescriptorHeaps.back();
		freeDescriptorHeaps.pop_back();
	}
	else
	{
		descriptorHeap = createDescriptorHeap();
		descriptorHeapPool.push_back(descriptorHeap);
	}

	return descriptorHeap;
}

com<ID3D12DescriptorHeap> dx_dynamic_descriptor_heap::createDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptorHeapDesc.NumDescriptors = numDescriptorsPerHeap;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	com<ID3D12DescriptorHeap> descriptorHeap;
	checkResult(dxContext.device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap)));

	return descriptorHeap;
}

uint32 dx_dynamic_descriptor_heap::computeStaleDescriptorCount() const
{
	uint32 numStaleDescriptors = 0;
	DWORD i;
	DWORD staleDescriptorsBitMask = staleDescriptorTableBitMask;

	while (_BitScanForward(&i, staleDescriptorsBitMask))
	{
		numStaleDescriptors += descriptorTableCache[i].numDescriptors;
		unsetBit(staleDescriptorsBitMask, i);
	}

	return numStaleDescriptors;
}
