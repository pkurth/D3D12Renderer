#pragma once

static uint32 atomicAdd(volatile uint32& a, uint32 b)
{
	return InterlockedAdd((volatile LONG*)&a, b) - b;
}

static uint32 atomicIncrement(volatile uint32& a)
{
	return InterlockedIncrement((volatile LONG*)&a) - 1;
}

static uint64 atomicIncrement(volatile uint64& a)
{
	return InterlockedIncrement64((volatile LONG64*)&a) - 1;
}

static uint32 atomicDecrement(volatile uint32& a)
{
	return InterlockedDecrement((volatile LONG*)&a) + 1;
}

static uint64 atomicDecrement(volatile uint64& a)
{
	return InterlockedDecrement64((volatile LONG64*)&a) + 1;
}

struct thread_mutex
{
	HANDLE handle;

	bool lock()
	{
		return WaitForSingleObject(handle, INFINITE) == WAIT_OBJECT_0;
	}

	bool unlock()
	{
		return ReleaseMutex(handle);
	}
};

static thread_mutex createMutex()
{
	thread_mutex result = { CreateMutex(0, FALSE, 0) };
	return result;
}

