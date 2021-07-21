#pragma once

#define KB(n) (1024 * (n))
#define MB(n) (1024 * KB(n))
#define GB(n) (1024 * MB(n))

#define BYTE_TO_KB(b) ((b) / 1024)
#define BYTE_TO_MB(b) ((b) / (1024 * 1024))
#define BYTE_TO_GB(b) ((b) / (1024 * 1024))

static uint32 alignTo(uint32 currentOffset, uint32 alignment)
{
	uint32 mask = alignment - 1;
	uint32 misalignment = currentOffset & mask;
	uint32 adjustment = (misalignment == 0) ? 0 : (alignment - misalignment);
	return currentOffset + adjustment;
}

static uint64 alignTo(uint64 currentOffset, uint64 alignment)
{
	uint64 mask = alignment - 1;
	uint64 misalignment = currentOffset & mask;
	uint64 adjustment = (misalignment == 0) ? 0 : (alignment - misalignment);
	return currentOffset + adjustment;
}

static void* alignTo(void* currentAddress, uint64 alignment)
{
	uint64 mask = alignment - 1;
	uint64 misalignment = (uint64)(currentAddress)&mask;
	uint64 adjustment = (misalignment == 0) ? 0 : (alignment - misalignment);
	return (uint8*)currentAddress + adjustment;
}

static bool rangesOverlap(uint64 fromA, uint64 toA, uint64 fromB, uint64 toB)
{
	if (toA <= fromB || fromA >= toA) return false;
	return true;
}

static bool rangesOverlap(void* fromA, void* toA, void* fromB, void* toB)
{
	if (toA <= fromB || fromA >= toB) return false;
	return true;
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

