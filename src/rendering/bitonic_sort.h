#pragma once

struct dx_command_list;
struct dx_buffer;

void initializeBitonicSort();

// These functions sort a buffer of values based on a key. The currently supported variants are:
// - bitonicSortUint: Key and value are packed into a single uint. The sorting algorithm will simply sort this buffer. The user must pack the key into
//	 the most significant and the value into the least significant bits (how many bits the two take is up to you). 
//   Since the values are sorted as uints, you have to make sure that your key is sortable when interpreted as a uint.
// - bitonicSortFloat: Here you provide two buffers, one with the values (32 bit indices) and one with the keys (32 bit floats). The buffers will be sorted
//   based on the keys.
// 
// The input buffers for keys and values are expected to be in the resource state D3D12_RESOURCE_STATE_UNORDERED_ACCESS. They also return in this state.
// It is expected that you have a GPU-visible buffer containing the number of elements to sort (counterBuffer). counterBufferOffset is a byte-offset into this
// buffer to the location where the count sits.
// The count does not need to be a power of two.

void bitonicSortUint(dx_command_list* cl, 
	const ref<dx_buffer>& sortKeysAndValues, uint32 sortOffset, uint32 maxNumElements, 
	const ref<dx_buffer>& counterBuffer, uint32 counterBufferOffset, bool sortAscending);
void bitonicSortFloat(dx_command_list* cl, 
	const ref<dx_buffer>& sortKeys, uint32 sortKeyOffset, 
	const ref<dx_buffer>& sortValues, uint32 sortValueOffset, uint32 maxNumElements, 
	const ref<dx_buffer>& counterBuffer, uint32 counterBufferOffset, bool sortAscending);

// Internal test functions.
void testBitonicSortUint(uint32 numElements, bool ascending);
void testBitonicSortFloat(uint32 numElements, bool ascending);
