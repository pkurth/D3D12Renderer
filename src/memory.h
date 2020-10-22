#pragma once

#define KB(n) (1024 * (n))
#define MB(n) (1024 * KB(n))
#define GB(n) (1024 * MB(n))

static uint32 alignTo(uint32 currentOffset, uint32 alignment)
{
	uint32 mask = alignment - 1;
	uint32 misalignment = currentOffset & mask;
	if (misalignment == 0)
	{
		return currentOffset;
	}
	uint32 adjustment = alignment - misalignment;
	return currentOffset + adjustment;
}

static uint64 alignTo(uint64 currentOffset, uint64 alignment)
{
	uint64 mask = alignment - 1;
	uint64 misalignment = currentOffset & mask;
	if (misalignment == 0)
	{
		return currentOffset;
	}
	uint64 adjustment = alignment - misalignment;
	return currentOffset + adjustment;
}

static void* alignTo(void* currentAddress, uint64 alignment)
{
	uint64 mask = alignment - 1;
	uint64 misalignment = (uint64)(currentAddress)&mask;
	if (misalignment == 0)
	{
		return currentAddress;
	}
	uint64 adjustment = alignment - misalignment;
	return (uint8*)currentAddress + adjustment;
}

struct memory_block
{
	uint8* start;
	uint8* current;
	uint64 size;
	memory_block* next;
};

struct memory_arena
{
	memory_block* currentBlock;
	memory_block* lastActiveBlock;
	memory_block* freeBlocks;
	uint64 minimumBlockSize;

	void* allocate(uint64 size, bool clearToZero = false);
	void reset();
	void free();

	memory_block* getFreeBlock(uint64 size = 0);
};

