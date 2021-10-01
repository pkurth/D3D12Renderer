#pragma once

#define KB(n) (1024ull * (n))
#define MB(n) (1024ull * KB(n))
#define GB(n) (1024ull * MB(n))

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

struct memory_marker
{
	uint64 before;
};

struct memory_arena
{
	memory_arena() {}
	memory_arena(const memory_arena&) = delete;
	memory_arena(memory_arena&&) = default;
	~memory_arena() { reset(true); }

	void initialize(uint64 minimumBlockSize = 0, uint64 reserveSize = GB(8));


	void ensureFreeSize(uint64 size);

	void* allocate(uint64 size, uint64 alignment = 1, bool clearToZero = false);

	template <typename T>
	T* allocate(uint32 count = 1, bool clearToZero = false)
	{
		return (T*)allocate(sizeof(T) * count, alignof(T), clearToZero);
	}

	void* getCurrent(uint64 alignment = 1);

	template <typename T>
	T* getCurrent()
	{
		return (T*)getCurrent(alignof(T));
	}

	void setCurrentTo(void* ptr);


	void reset(bool freeMemory = false);

	memory_marker getMarker();
	void resetToMarker(memory_marker marker);

	uint8* base() { return memory; }

protected:
	uint8* memory;
	uint64 committedMemory;

	uint64 current;
	uint64 sizeLeftCurrent;

	uint64 sizeLeftTotal;

	uint64 pageSize;
	uint64 minimumBlockSize;

	uint64 reserveSize;
};

