#pragma once

struct dx_command_list;
struct dx_buffer;

void initializeBitonicSort();

// With the variant 'bitonicSortUint' you can sort a uint-buffer. This way you can for example pack your key (the thing you want to sort by) in the upper
// bits and the value (e.g. the index to your item) in the lower bits. Note that this requires that the keys are ints, since the sorting is done on the whole
// uint. You cannot e.g. pack a half into the upper bits.
// With 'bitonicSortFloat' you provide a sortBuffer and a comparisonBuffer. The comparisonBuffer contains the values to sort, while the sortBuffer contains 
// the corresponding values. 

// sortBuffer is the buffer to sort. It is expected that this buffer is in state D3D12_RESOURCE_STATE_UNORDERED_ACCESS.
void bitonicSortUint(dx_command_list* cl, const ref<dx_buffer>& sortBuffer, const ref<dx_buffer>& counterBuffer, uint32 counterBufferOffset, bool sortAscending);
void bitonicSortFloat(dx_command_list* cl, const ref<dx_buffer>& sortBuffer, const ref<dx_buffer>& comparisonBuffer, const ref<dx_buffer>& counterBuffer, uint32 counterBufferOffset, bool sortAscending);

// Internal test functions.
void testBitonicSortUint(uint32 numElements, bool ascending);
void testBitonicSortFloat(uint32 numElements, bool ascending);
