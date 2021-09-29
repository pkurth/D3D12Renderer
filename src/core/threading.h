#pragma once

#include <functional>

// All functions return the original value.

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

static uint32 atomicCompareExchange(volatile uint32& destination, uint32 exchange, uint32 compare)
{
	return InterlockedCompareExchange((volatile LONG*)&destination, exchange, compare);
}

static uint64 atomicCompareExchange(volatile uint64& destination, uint64 exchange, uint64 compare)
{
	return InterlockedCompareExchange64((volatile LONG64*)&destination, exchange, compare);
}

static uint32 atomicExchange(volatile uint32& destination, uint32 exchange)
{
	return InterlockedExchange((volatile LONG*)&destination, exchange);
}

static uint64 atomicExchange(volatile uint64& destination, uint64 exchange)
{
	return InterlockedExchange64((volatile LONG64*)&destination, exchange);
}

static uint32 getThreadIDFast()
{
	// This is what standard library functions do internally, but this function can trivially be inlined.
	uint8* threadLocalStorage = (uint8*)__readgsqword(0x30);
	uint32 threadID = *(uint32*)(threadLocalStorage + 0x48);
	return threadID;
}


struct thread_job_context
{
	volatile uint32 numJobs = 0;

	void addWork(const std::function<void()>& cb);
	void waitForWorkCompletion();
};

void initializeJobSystem();

