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

ConstantBuffer<bitonic_sort_cb> cb		: register(b0);
ConstantBuffer<bitonic_sort_kj_cb> kj	: register(b1);
StructuredBuffer<uint> counterBuffer	: register(t0);
RWStructuredBuffer<uint> sortBuffer     : register(u0);

#if defined(BITONIC_SORT_FLOAT)
RWStructuredBuffer<float> comparisonBuffer  : register(u1);
#endif

[numthreads(1024, 1, 1)]
[RootSignature(BITONIC_SORT_RS)]
void main(cs_input IN)
{
    uint listCount = counterBuffer[cb.counterOffset];

    // Form unique index pair from dispatch thread ID.
    uint k = kj.k;
    uint j = kj.j;
    uint index2 = insertOneBit(IN.dispatchThreadID.x, j);
    uint index1 = index2 ^ (k == 2 * j ? k - 1 : j);

    if (index2 >= listCount)
    {
        return;
    }

#if defined(BITONIC_SORT_UINT)
    key_value a = sortBuffer[index1];
    key_value b = sortBuffer[index2];

    if (shouldSwapUint(a, b, cb.nullItem))
    {
        sortBuffer[index1] = b;
        sortBuffer[index2] = a;
    }
#elif defined(BITONIC_SORT_FLOAT)

    bool ascending = cb.nullItem > 0.f;

    float a = comparisonBuffer[index1];
    float b = comparisonBuffer[index2];

    if (shouldSwapFloat(a, b, ascending))
    {
        uint aVal = sortBuffer[index1];
        uint bVal = sortBuffer[index2];

        sortBuffer[index1] = bVal;
        sortBuffer[index2] = aVal;

        comparisonBuffer[index1] = b;
        comparisonBuffer[index2] = a;
    }
#endif
}
