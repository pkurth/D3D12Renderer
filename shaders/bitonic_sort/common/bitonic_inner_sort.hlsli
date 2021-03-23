#include "cs.hlsli"
#include "bitonic_sort_rs.hlsli"

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

ConstantBuffer<bitonic_sort_cb> cb		    : register(b0);
StructuredBuffer<uint> counterBuffer	    : register(t0);
RWStructuredBuffer<uint> sortBuffer         : register(u0);

#if defined(BITONIC_SORT_FLOAT)
RWStructuredBuffer<float> comparisonBuffer  : register(u1);
#endif


groupshared key_value gs_sortKeys[2048];

void loadKeyIndexPair(uint element, uint listCount)
{
#if defined(BITONIC_SORT_UINT)
    gs_sortKeys[element & 2047] = (element < listCount ? sortBuffer[element] : cb.nullItem);
#elif defined(BITONIC_SORT_FLOAT)
    key_value kv = {
        element < listCount ? comparisonBuffer[element] : cb.nullItem,
        element < listCount ? sortBuffer[element] : 0
    };
    gs_sortKeys[element & 2047] = kv;
#endif
}

void storeKeyIndexPair(uint element, uint listCount)
{
    if (element < listCount)
    {
#if defined(BITONIC_SORT_UINT)
        sortBuffer[element] = gs_sortKeys[element & 2047];
#elif defined(BITONIC_SORT_FLOAT)
        comparisonBuffer[element] = gs_sortKeys[element & 2047].key;
        sortBuffer[element] = gs_sortKeys[element & 2047].value;
#endif
    }
}

[numthreads(1024, 1, 1)]
[RootSignature(BITONIC_SORT_RS)]
void main(cs_input IN)
{
    uint listCount = counterBuffer[cb.counterOffset];

    // Item index of the start of this group.
    const uint groupStart = IN.groupID.x * 2048;

    // Load from memory into LDS to prepare sort.
    loadKeyIndexPair(groupStart + IN.groupIndex, listCount);
    loadKeyIndexPair(groupStart + IN.groupIndex + 1024, listCount);

    GroupMemoryBarrierWithGroupSync();

#if defined(BITONIC_SORT_FLOAT)
    bool ascending = cb.nullItem > 0.f;
#endif

    // This is better unrolled because it reduces ALU and because some
    // architectures can load/store two LDS items in a single instruction
    // as long as their separation is a compile-time constant.
    [unroll]
    for (uint j = 1024; j > 0; j /= 2)
    {
        uint index2 = insertOneBit(IN.groupIndex, j);
        uint index1 = index2 ^ j;

        key_value a = gs_sortKeys[index1];
        key_value b = gs_sortKeys[index2];

        if (
#if defined(BITONIC_SORT_UINT)
            shouldSwapUint(a, b, cb.nullItem)
#elif defined(BITONIC_SORT_FLOAT)
            shouldSwapFloat(a.key, b.key, ascending)
#endif                
            )
        {
            // Swap the keys.
            gs_sortKeys[index1] = b;
            gs_sortKeys[index2] = a;
        }

        GroupMemoryBarrierWithGroupSync();
    }

    storeKeyIndexPair(groupStart + IN.groupIndex, listCount);
    storeKeyIndexPair(groupStart + IN.groupIndex + 1024, listCount);
}
