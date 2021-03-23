#include "pch.h"
#include "dx_bitonic_sort.h"
#include "dx_pipeline.h"
#include "dx_command_list.h"
#include "dx_barrier_batcher.h"
#include "random.h"

#include "bitonic_sort_rs.hlsli"

// Adapted from MiniEngine.

//
// Copyright(c) 2013 - 2015 Microsoft
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//


static dx_pipeline startSortPipeline;

// Uint.
static dx_pipeline preSortUintPipeline;
static dx_pipeline innerSortUintPipeline;
static dx_pipeline outerSortUintPipeline;

// Float.
static dx_pipeline preSortFloatPipeline;
static dx_pipeline innerSortFloatPipeline;
static dx_pipeline outerSortFloatPipeline;

static ref<dx_buffer> dispatchBuffer;

void initializeBitonicSort()
{
	startSortPipeline = createReloadablePipeline("bitonic_start_sort_cs");

	// Uint.
	preSortUintPipeline = createReloadablePipeline("bitonic_pre_sort_uint_cs");
	innerSortUintPipeline = createReloadablePipeline("bitonic_inner_sort_uint_cs");
	outerSortUintPipeline = createReloadablePipeline("bitonic_outer_sort_uint_cs");

	// Float.
	preSortFloatPipeline = createReloadablePipeline("bitonic_pre_sort_float_cs");
	innerSortFloatPipeline = createReloadablePipeline("bitonic_inner_sort_float_cs");
	outerSortFloatPipeline = createReloadablePipeline("bitonic_outer_sort_float_cs");

	dispatchBuffer = createBuffer(sizeof(D3D12_DISPATCH_ARGUMENTS), 22 * 23 / 2, 0, true, false, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

static std::pair<uint32, uint32> getAlignedMaxNumElementsAndNumIterations(uint32 maxNumElements)
{
	uint32 alignedMaxNumElements = alignToPowerOfTwo(maxNumElements);
	uint32 maxNumIterations = log2(max(2048u, alignedMaxNumElements)) - 10;

	return { alignedMaxNumElements, maxNumIterations };
}

static void bitonicSortInternal(dx_command_list* cl, const ref<dx_buffer>& sortBuffer, const ref<dx_buffer>& comparisonBuffer, 
	const ref<dx_buffer>& counterBuffer, bitonic_sort_cb cb, uint32 alignedMaxNumElements,
	const dx_pipeline& preSortPipeline, const dx_pipeline& outerSortPipeline, const dx_pipeline& innerSortPipeline)
{
	// ----------------------------------------
	// START
	// ----------------------------------------

	cl->setPipelineState(*startSortPipeline.pipeline);
	cl->setComputeRootSignature(*startSortPipeline.rootSignature);

	cl->setCompute32BitConstants(BITONIC_SORT_RS_CB, cb);
	cl->setRootComputeUAV(BITONIC_SORT_RS_DISPATCH, dispatchBuffer);
	cl->setRootComputeSRV(BITONIC_SORT_RS_COUNTER_BUFFER, counterBuffer);

	cl->dispatch(1);
	barrier_batcher(cl)
		//.uav(dispatchBuffer)
		.transition(dispatchBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);



	// ----------------------------------------
	// PRE-SORT
	// ----------------------------------------

	// Pre-sort the buffer up to k = 2048. This also pads the list with invalid indices
	// that will drift to the end of the sorted list.

	// Since the root signatures between all stages are identical, we don't need to switch between them and don't need to rebind the root arguments.
	// The only thing we need to bind here is the sort buffer, since this is not needed in the start stage.

	cl->setPipelineState(*preSortPipeline.pipeline);

	cl->setRootComputeUAV(BITONIC_SORT_RS_SORT_BUFFER, sortBuffer);
	if (comparisonBuffer)
	{
		cl->setRootComputeUAV(BITONIC_SORT_RS_COMPARISON_BUFFER, comparisonBuffer);
	}

	cl->dispatchIndirect(1, dispatchBuffer, 0);
	cl->uavBarrier(sortBuffer);




	// ----------------------------------------
	// SORT
	// ----------------------------------------

	uint32 indirectArgsOffset = 12;

	for (uint32 k = 4096; k <= alignedMaxNumElements; k *= 2)
	{
		cl->setPipelineState(*outerSortPipeline.pipeline);

		for (uint32 j = k / 2; j >= 2048; j /= 2)
		{
			cl->setCompute32BitConstants(BITONIC_SORT_RS_KJ, bitonic_sort_kj_cb{ k, j });
			cl->dispatchIndirect(1, dispatchBuffer, indirectArgsOffset);
			cl->uavBarrier(sortBuffer);
			indirectArgsOffset += 12;
		}

		cl->setPipelineState(*innerSortPipeline.pipeline);

		cl->dispatchIndirect(1, dispatchBuffer, indirectArgsOffset);
		cl->uavBarrier(sortBuffer);
		indirectArgsOffset += 12;
	}

	cl->transitionBarrier(dispatchBuffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void bitonicSortUint(dx_command_list* cl, const ref<dx_buffer>& sortBuffer, const ref<dx_buffer>& counterBuffer, uint32 counterBufferOffset, bool sortAscending)
{
	assert(sortBuffer->elementSize == 4);

	uint32 maxNumElements = sortBuffer->elementCount;

	auto [alignedMaxNumElements, maxNumIterations] = getAlignedMaxNumElementsAndNumIterations(maxNumElements);
	bitonic_sort_cb cb;
	cb.maxNumIterations = maxNumIterations;
	cb.counterOffset = counterBufferOffset;
	cb.nullUint = sortAscending ? 0xffffffff : 0;

	bitonicSortInternal(cl, sortBuffer, 0, counterBuffer, cb, alignedMaxNumElements, preSortUintPipeline, outerSortUintPipeline, innerSortUintPipeline);
}

void bitonicSortFloat(dx_command_list* cl, const ref<dx_buffer>& sortBuffer, const ref<dx_buffer>& comparisonBuffer, const ref<dx_buffer>& counterBuffer, uint32 counterBufferOffset, bool sortAscending)
{
	assert(sortBuffer->elementSize == 4);
	assert(comparisonBuffer->elementSize == 4);

	uint32 maxNumElements = max(sortBuffer->elementCount, comparisonBuffer->elementCount);

	auto [alignedMaxNumElements, maxNumIterations] = getAlignedMaxNumElementsAndNumIterations(maxNumElements);
	bitonic_sort_cb cb;
	cb.maxNumIterations = maxNumIterations;
	cb.counterOffset = counterBufferOffset;
	cb.nullFloat = sortAscending ? FLT_MAX : -FLT_MAX;

	bitonicSortInternal(cl, sortBuffer, comparisonBuffer, counterBuffer, cb, alignedMaxNumElements, preSortFloatPipeline, outerSortFloatPipeline, innerSortFloatPipeline);
}

void testBitonicSortUint(uint32 numElements, bool ascending)
{
	uint32 mask = alignToPowerOfTwo(numElements) - 1;

	random_number_generator rng = { 125912842 };

	uint32* values = new uint32[numElements];
	for (uint32 i = 0; i < numElements; ++i)
	{
		values[i] = (rng.randomUint() & ~mask) | i;
	}

	ref<dx_buffer> list = createBuffer(sizeof(uint32), numElements, values, true);
	ref<dx_buffer> counter = createBuffer(sizeof(uint32), 1, &numElements);

	delete[] values;

	ref<dx_buffer> dispatchReadback = createReadbackBuffer(sizeof(D3D12_DISPATCH_ARGUMENTS), 22 * 23 / 2);
	ref<dx_buffer> listReadback = createReadbackBuffer(sizeof(uint32), numElements);

	dx_command_list* cl = dxContext.getFreeRenderCommandList();
	
	bitonicSortUint(cl, list, counter, 0, ascending);
	cl->transitionBarrier(list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	cl->transitionBarrier(dispatchBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	cl->copyResource(dispatchBuffer->resource, dispatchReadback->resource);
	cl->copyResource(list->resource, listReadback->resource);

	dxContext.executeCommandList(cl);
	dxContext.flushApplication();

	uint32* sortedValues = (uint32*)mapBuffer(listReadback, true);
	D3D12_DISPATCH_ARGUMENTS* args = (D3D12_DISPATCH_ARGUMENTS*)mapBuffer(dispatchReadback, true);

	for (uint32 i = 0; i < numElements - 1; ++i)
	{
		if ((sortedValues[i] & mask) >= numElements)
		{
			std::cerr << "Corrupted list index detected.\n";
		}

		if (ascending)
		{
			if (sortedValues[i] > sortedValues[i + 1])
			{
				std::cerr << "Invalid sort order:  non-ascending.\n";
			}
		}
		else
		{
			if (sortedValues[i] < sortedValues[i + 1])
			{
				std::cerr << "Invalid sort order:  non-descending.\n";
			}
		}
	}

	if ((sortedValues[numElements - 1] & mask) >= numElements)
	{
		std::cerr << "Corrupted list index detected.\n";
	}

	unmapBuffer(dispatchReadback, false);
	unmapBuffer(listReadback, false);
}

void testBitonicSortFloat(uint32 numElements, bool ascending)
{
	random_number_generator rng = { 125912842 };

	uint32* values = new uint32[numElements];
	float* keys = new float[numElements];
	for (uint32 i = 0; i < numElements; ++i)
	{
		values[i] = i;
		keys[i] = rng.randomFloatBetween(-10000.f, 10000.f);
	}

	ref<dx_buffer> valuesList = createBuffer(sizeof(uint32), numElements, values, true);
	ref<dx_buffer> keysList = createBuffer(sizeof(float), numElements, keys, true);
	ref<dx_buffer> counter = createBuffer(sizeof(uint32), 1, &numElements);


	ref<dx_buffer> dispatchReadback = createReadbackBuffer(sizeof(D3D12_DISPATCH_ARGUMENTS), 22 * 23 / 2);
	ref<dx_buffer> valuesListReadback = createReadbackBuffer(sizeof(uint32), numElements);
	ref<dx_buffer> keysListReadback = createReadbackBuffer(sizeof(float), numElements);

	dx_command_list* cl = dxContext.getFreeRenderCommandList();

	bitonicSortFloat(cl, valuesList, keysList, counter, 0, ascending);
	cl->transitionBarrier(valuesList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	cl->transitionBarrier(keysList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	cl->transitionBarrier(dispatchBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	cl->copyResource(dispatchBuffer->resource, dispatchReadback->resource);
	cl->copyResource(valuesList->resource, valuesListReadback->resource);
	cl->copyResource(keysList->resource, keysListReadback->resource);

	dxContext.executeCommandList(cl);
	dxContext.flushApplication();

	uint32* sortedValues = (uint32*)mapBuffer(valuesListReadback, true);
	float* sortedKeys = (float*)mapBuffer(keysListReadback, true);
	D3D12_DISPATCH_ARGUMENTS* args = (D3D12_DISPATCH_ARGUMENTS*)mapBuffer(dispatchReadback, true);

	for (uint32 i = 0; i < numElements - 1; ++i)
	{
		if (ascending)
		{
			if (sortedKeys[i] > sortedKeys[i + 1])
			{
				std::cerr << "Invalid sort order:  non-ascending.\n";
			}
		}
		else
		{
			if (sortedKeys[i] < sortedKeys[i + 1])
			{
				std::cerr << "Invalid sort order:  non-descending.\n";
			}
		}

		uint32 value = sortedValues[i];
		float key = sortedKeys[i];
		float compKey = keys[value];

		if (key != compKey)
		{
			std::cerr << "Invalid pairing.\n";
		}
	}

	unmapBuffer(dispatchReadback, false);
	unmapBuffer(keysListReadback, false);
	unmapBuffer(valuesListReadback, false);

	delete[] values;
	delete[] keys;
}
