#include "cs.hlsli"
#include "bitonic_sort_rs.hlsli"
#include "indirect.hlsli"

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


ConstantBuffer<bitonic_sort_cb> cb						: register(b0);
StructuredBuffer<uint> counterBuffer					: register(t0);
RWStructuredBuffer<D3D12_DISPATCH_ARGUMENTS> dispatch	: register(u0);


uint nextPowerOfTwo(uint i)
{
	uint mask = (1 << firstbithigh(i)) - 1;
	return (i + mask) & ~mask;
}

[numthreads(22, 1, 1)]
[RootSignature(BITONIC_SORT_RS)]
void main(cs_input IN)
{
	if (IN.groupIndex >= cb.maxNumIterations)
	{
		return;
	}

	uint listCount = counterBuffer[cb.counterOffset];
	uint k = 2048 << IN.groupIndex;

	// We need one more iteration every time the number of thread groups doubles.
	if (k > nextPowerOfTwo((listCount + 2047) & ~2047))
	{
		listCount = 0;
	}

	uint offset = IN.groupIndex * (IN.groupIndex + 1) / 2;

	// Generate outer sort dispatch arguments.
	for (uint j = k / 2; j > 1024; j /= 2)
	{
		// All of the groups of size 2j that are full.
		uint completeGroups = (listCount & ~(2 * j - 1)) / 2048;

		// Remaining items must only be sorted if there are more than j of them.
		uint partialGroups = ((uint)max(int(listCount - completeGroups * 2048 - j), 0) + 1023) / 1024;

		D3D12_DISPATCH_ARGUMENTS arguments = { completeGroups + partialGroups, 1, 1 };
		dispatch[offset] = arguments;

		++offset;
	}

	// The inner sort always sorts all groups (rounded up to multiples of 2048).
	D3D12_DISPATCH_ARGUMENTS arguments = { (listCount + 2047) / 2048, 1, 1 };
	dispatch[offset] = arguments;
}
