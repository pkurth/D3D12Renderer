#include "pch.h"
#include "job_system.h"
#include "math.h"
#include "imgui.h"


job_queue highPriorityJobQueue;
job_queue lowPriorityJobQueue;
job_queue mainThreadJobQueue;

job_queue* queues[] =
{
    &highPriorityJobQueue,
    &lowPriorityJobQueue,
    &mainThreadJobQueue,
};

void job_queue::initialize(int32 queueIndex, uint32 numThreads, uint32 threadOffset, int32 threadPriority, const wchar* description)
{
    this->queueIndex = queueIndex;
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

void job_queue::addContinuation(int32 firstGlobalIndex, job_handle second)
{
    job_queue_entry& firstJob = allJobs[firstGlobalIndex & indexMask];
    ASSERT(firstJob.continuation.globalIndex == -1);

    int32 unfinished = firstJob.numUnfinishedJobs++;
    if (unfinished == 0)
    {
        // First job was finished before adding continuation -> just submit second.
        queues[second.queueIndex]->submit(second.globalIndex);
    }
    else
    {
        // First job hadn't finished before -> add second as continuation and then finish first (which decrements numUnfinished again).
        firstJob.continuation = second;
        finishJob(firstGlobalIndex);
    }
}

void job_queue::submit(int32 globalIndex)
{
    if (globalIndex != -1)
    {
        while (!queue.try_enqueue(globalIndex))
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

void job_queue::waitForCompletion(int32 globalIndex)
{
    if (globalIndex != -1)
    {
        job_queue_entry& job = allJobs[globalIndex & indexMask];

        while (job.numUnfinishedJobs > 0) 
        {
            executeNextJob();
        }
    }
}

void job_queue::finishJob(int32 globalIndex)
{
    job_queue_entry& job = allJobs[globalIndex & indexMask];
    int32 numUnfinishedJobs = --job.numUnfinishedJobs;
    ASSERT(numUnfinishedJobs >= 0);
    if (numUnfinishedJobs == 0)
    {
        --runningJobs;

        if (job.parentGlobalIndex != -1)
        {
            finishJob(job.parentGlobalIndex);
        }

        if (job.continuation.globalIndex != -1)
        {
            queues[job.continuation.queueIndex]->submit(job.continuation.globalIndex);
        }
    }
}

bool job_queue::executeNextJob()
{
    int32 globalIndex = -1;
    if (queue.try_dequeue(globalIndex))
    {
        job_queue_entry& job = allJobs[globalIndex & indexMask];
        job.function(job.templatedFunction, job.data, { globalIndex, queueIndex });

        finishJob(globalIndex);

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
    std::cout << globalIndex << '\n';
    queues[queueIndex]->submit(globalIndex);
}

void job_handle::submitAfter(job_handle before)
{
    queues[before.queueIndex]->addContinuation(before.globalIndex, *this);
}

void job_handle::waitForCompletion()
{
    queues[queueIndex]->waitForCompletion(globalIndex);
}




void initializeJobSystem()
{
    HANDLE handle = GetCurrentThread();
    SetThreadAffinityMask(handle, 1);
    SetThreadPriority(handle, THREAD_PRIORITY_HIGHEST);
    CloseHandle(handle);

    //uint32 numHardwareThreads = std::thread::hardware_concurrency();


    highPriorityJobQueue.initialize(0, 4, 1, THREAD_PRIORITY_NORMAL, L"High priority worker");
    lowPriorityJobQueue.initialize(1, 4, 5, THREAD_PRIORITY_BELOW_NORMAL, L"Low priority worker");
    mainThreadJobQueue.initialize(2, 0, 0, 0, 0);
}

void executeMainThreadJobs()
{
    mainThreadJobQueue.waitForCompletion();
}
