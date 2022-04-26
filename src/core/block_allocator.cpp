#include "pch.h"
#include "block_allocator.h"


void block_allocator::initialize(uint64 capacity)
{
	availableSize = capacity;
	addNewBlock(0, capacity);
}

uint64 block_allocator::allocate(uint64 requestedSize)
{
	auto smallestBlockItIt = blocksBySize.lower_bound(requestedSize);
	if (smallestBlockItIt == blocksBySize.end())
	{
		return UINT64_MAX;
	}

	size_value sizeValue = smallestBlockItIt->second;

	uint64 offset = sizeValue.getOffset();
	uint64 size = smallestBlockItIt->first;

	uint64 newOffset = offset + requestedSize;
	uint64 newSize = size - requestedSize;

	blocksBySize.erase(smallestBlockItIt);
	blocksByOffset.erase(sizeValue.offsetIterator);

	if (newSize > 0)
	{
		addNewBlock(newOffset, newSize);
	}

	availableSize -= requestedSize;
	return offset;
}

void block_allocator::free(uint64 offset, uint64 size)
{
	auto nextBlockIt = blocksByOffset.upper_bound(offset);
	auto prevBlockIt = nextBlockIt;
	if (prevBlockIt != blocksByOffset.begin())
	{
		--prevBlockIt;
	}
	else
	{
		prevBlockIt = blocksByOffset.end();
	}

	uint64 newOffset, newSize;
	if (prevBlockIt != blocksByOffset.end() && offset == prevBlockIt->first + prevBlockIt->second.getSize())
	{
		// PrevBlock.Offset           Offset
		// |                          |
		// |<-----PrevBlock.Size----->|<------Size-------->|
		//
		newSize = prevBlockIt->second.getSize() + size;
		newOffset = prevBlockIt->first;

		if (nextBlockIt != blocksByOffset.end() && offset + size == nextBlockIt->first)
		{
			// PrevBlock.Offset           Offset               NextBlock.Offset
			// |                          |                    |
			// |<-----PrevBlock.Size----->|<------Size-------->|<-----NextBlock.Size----->|
			//
			newSize += nextBlockIt->second.getSize();
			blocksBySize.erase(prevBlockIt->second.sizeIterator);
			blocksBySize.erase(nextBlockIt->second.sizeIterator);
			// Delete the range of two blocks
			++nextBlockIt;
			blocksByOffset.erase(prevBlockIt, nextBlockIt);
		}
		else
		{
			// PrevBlock.Offset           Offset                       NextBlock.Offset
			// |                          |                            |
			// |<-----PrevBlock.Size----->|<------Size-------->| ~ ~ ~ |<-----NextBlock.Size----->|
			//
			blocksBySize.erase(prevBlockIt->second.sizeIterator);
			blocksByOffset.erase(prevBlockIt);
		}
	}
	else if (nextBlockIt != blocksByOffset.end() && offset + size == nextBlockIt->first)
	{
		// PrevBlock.Offset                   Offset               NextBlock.Offset
		// |                                  |                    |
		// |<-----PrevBlock.Size----->| ~ ~ ~ |<------Size-------->|<-----NextBlock.Size----->|
		//
		newSize = size + nextBlockIt->second.getSize();
		newOffset = offset;
		blocksBySize.erase(nextBlockIt->second.sizeIterator);
		blocksByOffset.erase(nextBlockIt);
	}
	else
	{
		// PrevBlock.Offset                   Offset                       NextBlock.Offset
		// |                                  |                            |
		// |<-----PrevBlock.Size----->| ~ ~ ~ |<------Size-------->| ~ ~ ~ |<-----NextBlock.Size----->|
		//
		newSize = size;
		newOffset = offset;
	}

	addNewBlock(newOffset, newSize);

	availableSize += size;
}

