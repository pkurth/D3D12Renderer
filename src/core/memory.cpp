#include "pch.h"
#include "memory.h"


static memory_block* allocateMemoryBlock(uint64 size)
{
	uint64 blockSize = alignTo(sizeof(memory_block), 64);
	void* data = _aligned_malloc(blockSize + size, 64);

	memory_block* result = (memory_block*)data;
	result->start = (uint8*)data + blockSize;
	result->size = size;
	return result;
}

memory_block* memory_arena::getFreeBlock(uint64 size)
{
	if (size < minimumBlockSize)
	{
		size = minimumBlockSize;
	}

	memory_block* result = 0;

	for (memory_block* block = freeBlocks, *prev = 0; block; prev = block, block = block->next)
	{
		if (block->size >= size)
		{
			if (prev)
			{
				prev->next = block->next;
			}
			else
			{
				freeBlocks = block->next;
			}

			result = block;
			break;
		}
	}

	if (!result)
	{
		result = allocateMemoryBlock(size);
	}

	result->next = 0;
	result->current = result->start;

	return result;
}

static uint64 getRemainingSize(memory_block* block)
{
	return block->size - (uint64)(block->current - block->size);
}

void* memory_arena::allocate(uint64 size, bool clearToZero)
{
	if (!currentBlock ||
		getRemainingSize(currentBlock) < size)
	{
		memory_block* block = getFreeBlock(size);
		block->next = currentBlock;

		if (!lastActiveBlock)
		{
			lastActiveBlock = block;
		}

		currentBlock = block;
	}

	void* result = currentBlock->current;
	currentBlock->current += size;

	if (clearToZero)
	{
		memset(result, 0, size);
	}

	return result;
}

void memory_arena::reset()
{
	if (lastActiveBlock)
	{
		lastActiveBlock->next = freeBlocks;
	}
	freeBlocks = currentBlock;
	currentBlock = 0;
	lastActiveBlock = 0;
}

void memory_arena::free()
{
	reset();
	for (memory_block* block = freeBlocks; block; )
	{
		memory_block* next = block->next;
		_aligned_free(block);
		block = next;
	}
	*this = {};
}
