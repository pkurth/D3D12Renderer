#include "pch.h"
#include "memory.h"
#include "math.h"

void memory_arena::initialize(uint64 minimumBlockSize, uint64 reserveSize)
{
	reset(true);

	memory = (uint8*)VirtualAlloc(0, reserveSize, MEM_RESERVE, PAGE_READWRITE);

	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);

	pageSize = systemInfo.dwPageSize;
	sizeLeftTotal = reserveSize;
	this->minimumBlockSize = minimumBlockSize;
	this->reserveSize = reserveSize;
}

void* memory_arena::allocate(uint64 size, uint64 alignment, bool clearToZero)
{
	uint64 mask = alignment - 1;
	uint64 misalignment = current & mask;
	uint64 adjustment = (misalignment == 0) ? 0 : (alignment - misalignment);
	current += adjustment;

	sizeLeftCurrent -= adjustment;
	sizeLeftTotal -= adjustment;

	assert(sizeLeftTotal >= size);

	if (sizeLeftCurrent < size)
	{
		uint64 allocationSize = max(size, minimumBlockSize);
		allocationSize = pageSize * bucketize(allocationSize, pageSize); // Round up to next page boundary.
		VirtualAlloc(memory + committedMemory, allocationSize, MEM_COMMIT, PAGE_READWRITE);

		sizeLeftTotal += allocationSize;
		sizeLeftCurrent += allocationSize;
		committedMemory += allocationSize;
	}

	uint8* result = memory + current;
	if (clearToZero)
	{
		memset(result, 0, size);
	}
	current += size;
	sizeLeftCurrent -= size;
	sizeLeftTotal -= size;

	return result;
}

void memory_arena::reset(bool freeMemory)
{
	if (memory && freeMemory)
	{
		VirtualFree(memory, 0, MEM_RELEASE);
		memory = 0;
		committedMemory = 0;
	}

	current = 0;
	sizeLeftCurrent = committedMemory;
	sizeLeftTotal = reserveSize;
}
