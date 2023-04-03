#include "pch.h"
#include "threading.h"
#include "math.h"
#include <intrin.h>




template <typename T, uint32 capacity>
struct work_queue
{
	void initialize(uint32 numThreads, uint32 threadOffset, int threadPriority, const wchar* description)
	{
		semaphoreHandle = CreateSemaphoreEx(0, 0, numThreads, 0, 0, SEMAPHORE_ALL_ACCESS);

		for (uint32 i = 0; i < numThreads; ++i)
		{
			std::thread thread([this]() { workerThreadProc(); });

			HANDLE handle = (HANDLE)thread.native_handle();
			SetThreadPriority(handle, threadPriority);

			uint64 affinityMask = 1ull << (i + threadOffset);
			SetThreadAffinityMask(handle, affinityMask);
			SetThreadDescription(handle, description);

			thread.detach();
		}
	}

	void workerThreadProc()
	{
		while (true)
		{
			if (!performWork())
			{
				WaitForSingleObjectEx(semaphoreHandle, INFINITE, FALSE);
			}
		}
	}

	bool performWork()
	{
		T entry;
		if (pop(entry))
		{
			entry.process();
			return true;
		}

		return false;
	}

	void push(const T& t)
	{
		while (!pushInternal(t))
		{
			performWork();
		}

		ReleaseSemaphore(semaphoreHandle, 1, 0);
	}

private:

	bool pushInternal(const T& t)
	{
		bool result = false;
		mutex.lock();
		uint32 next = (nextItemToWrite + 1) % capacity;
		if (next != nextItemToRead)
		{
			data[nextItemToWrite] = t;
			nextItemToWrite = next;
			result = true;
		}
		mutex.unlock();
		return result;
	}

	bool pop(T& t)
	{
		bool result = false;
		mutex.lock();
		if (nextItemToRead != nextItemToWrite)
		{
			t = data[nextItemToRead];
			nextItemToRead = (nextItemToRead + 1) % capacity;
			result = true;
		}
		mutex.unlock();
		return result;
	}



	uint32 nextItemToRead = 0;
	uint32 nextItemToWrite = 0;
	T data[capacity];

	std::mutex mutex;
	HANDLE semaphoreHandle;
};


struct frame_queue_entry
{
	std::function<void()> callback;
	thread_job_context* context;

	void process()
	{
		callback();
		atomicDecrement(context->numJobs);
	}
};

struct load_queue_entry
{
	std::function<void()> callback;

	void process()
	{
		callback();
	}
};

static work_queue<frame_queue_entry, 256> frameQueue;
static work_queue<load_queue_entry, 256> loadQueue;




void initializeJobSystem()
{
	HANDLE handle = GetCurrentThread();
	SetThreadAffinityMask(handle, 1);
	SetThreadPriority(handle, THREAD_PRIORITY_HIGHEST);
	CloseHandle(handle);

	uint32 numHardwareThreads = std::thread::hardware_concurrency();

	uint32 numFrameThreads = clamp(numHardwareThreads, 1u, 4u);
	frameQueue.initialize(numFrameThreads, 1, THREAD_PRIORITY_NORMAL, L"Worker thread"); // 1 is the main thread.

	uint32 numLoadThreads = 8;
	loadQueue.initialize(numLoadThreads, numFrameThreads + 1, THREAD_PRIORITY_BELOW_NORMAL, L"Loader thread"); // 1 is the main thread.
}












void thread_job_context::addWork(const std::function<void()>& cb)
{
	frame_queue_entry entry;
	entry.callback = cb;
	entry.context = this;
	atomicIncrement(numJobs);

	frameQueue.push(entry);
}

void thread_job_context::waitForWorkCompletion()
{
	while (numJobs)
	{
		if (!frameQueue.performWork())
		{
			while (numJobs) {}
			break;
		}
	}
}

void addAsyncLoadWork(const std::function<void()>& cb)
{
	load_queue_entry entry;
	entry.callback = cb;

	loadQueue.push(entry);
}
