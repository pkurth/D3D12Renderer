#include "pch.h"
#include "job_system.h"
#include "math.h"
#include "imgui.h"


void job_queue::initialize(uint32 numThreads, uint32 threadOffset, int threadPriority, const wchar* description)
{
    queue = moodycamel::ConcurrentQueue<int32>(capacity);

    for (uint32 i = 0; i < numThreads; ++i)
    {
        std::thread thread([this, i]() { threadFunc(i); });

        HANDLE handle = (HANDLE)thread.native_handle();
        SetThreadPriority(handle, threadPriority);

        uint64 affinityMask = 1ull << (i + threadOffset);
        SetThreadAffinityMask(handle, affinityMask);
        SetThreadDescription(handle, description);

        thread.detach();
    }
}

void job_queue::addContinuation(int32 first, job_handle second)
{
    job_queue_entry& firstJob = allJobs[first];
    ASSERT(firstJob.continuation.index == -1);

    int32 unfinished = firstJob.numUnfinishedJobs++;
    if (unfinished == 0)
    {
        // First job was finished before adding continuation -> just submit second.
        second.queue->submit(second.index);
    }
    else
    {
        // First job hadn't finished before -> add second as continuation and then finish first (which decrements numUnfinished again).
        firstJob.continuation = second;
        finishJob(first);
    }
}

void job_queue::submit(int32 handle)
{
    if (handle != -1)
    {
        while (!queue.try_enqueue(handle)) 
        {
            executeNextJob();
        }

        ++runningJobs;
        
        wakeCondition.notify_one();
    }
}

void job_queue::waitForCompletion()
{
    while (runningJobs)
    {
        executeNextJob();
    }
}

void job_queue::waitForCompletion(int32 handle)
{
    if (handle != -1)
    {
        job_queue_entry& job = allJobs[handle];

        while (job.numUnfinishedJobs > 0) 
        {
            executeNextJob();
        }
    }
}

int32 job_queue::allocateJob()
{
    return (int32)(nextFreeJob++ & indexMask);
}

void job_queue::finishJob(int32 handle)
{
    job_queue_entry& job = allJobs[handle];
    int32 numUnfinishedJobs = --job.numUnfinishedJobs;
    ASSERT(numUnfinishedJobs >= 0);
    if (numUnfinishedJobs == 0)
    {
        --runningJobs;

        if (job.parent != -1)
        {
            finishJob(job.parent);
        }

        if (job.continuation.index != -1)
        {
            job.continuation.queue->submit(job.continuation.index);
        }
    }
}

bool job_queue::executeNextJob()
{
    int32 handle = -1;
    if (queue.try_dequeue(handle))
    {
        job_queue_entry& job = allJobs[handle];
        job.function(job.templatedFunction, job.data, { handle, this });

        finishJob(handle);

        return true;
    }

    return false;
}

void job_queue::threadFunc(int32 threadIndex)
{
    while (true)
    {
        if (!executeNextJob())
        {
            std::unique_lock<std::mutex> lock(wakeMutex);
            wakeCondition.wait(lock);
        }
    }
}

void job_handle::submitNow()
{
    queue->submit(index);
}

void job_handle::submitAfter(job_handle before)
{
    before.queue->addContinuation(before.index, *this);
}

void job_handle::waitForCompletion()
{
    queue->waitForCompletion(index);
}




job_queue highPriorityJobQueue;
job_queue lowPriorityJobQueue;
job_queue mainThreadJobQueue;


void initializeJobSystem()
{
    HANDLE handle = GetCurrentThread();
    SetThreadAffinityMask(handle, 1);
    SetThreadPriority(handle, THREAD_PRIORITY_HIGHEST);
    CloseHandle(handle);

    //uint32 numHardwareThreads = std::thread::hardware_concurrency();


    highPriorityJobQueue.initialize(4, 1, THREAD_PRIORITY_NORMAL, L"High priority worker");
    lowPriorityJobQueue.initialize(4, 5, THREAD_PRIORITY_BELOW_NORMAL, L"Low priority worker");
    mainThreadJobQueue.initialize(0, 0, 0, 0);
}

void executeMainThreadJobs()
{
    mainThreadJobQueue.waitForCompletion();
}
