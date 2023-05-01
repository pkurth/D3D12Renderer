#pragma once

#include <concurrentqueue/concurrentqueue.h>

struct job_handle
{
    int32 globalIndex = -1;
    int32 queueIndex = -1;

    void submitNow();
    void submitAfter(job_handle before);
    void waitForCompletion();
};

template <typename data_t>
using job_function = void (*)(data_t&, job_handle);

struct job_queue
{
    void initialize(int32 queueIndex, uint32 numThreads, uint32 threadOffset, int32 threadPriority, const wchar* description);

    template <typename data_t>
    job_handle createJob(job_function<data_t> function, const data_t& data, job_handle parent = {})
    {
        static_assert(sizeof(data_t) <= job_queue_entry::DATA_SIZE);

        int32 globalIndex = nextFreeJob++;
        auto& job = allJobs[globalIndex & indexMask];
        job.numUnfinishedJobs = 1;
        job.parentGlobalIndex = parent.globalIndex;
        job.continuation.globalIndex = -1;

        if (parent.globalIndex != -1)
        {
            ASSERT(parent.queueIndex == queueIndex);
            ++allJobs[parent.globalIndex & indexMask].numUnfinishedJobs;
        }

        job.templatedFunction = function;
        job.function = [](void* templatedFunction, void* rawData, job_handle job)
        {
            data_t& data = *(data_t*)rawData;

            auto function = (job_function<data_t>)templatedFunction;
            function(data, job);

            data.~data_t();
        };

        new(job.data) data_t(data);

        return job_handle{ globalIndex, queueIndex };
    }


    void waitForCompletion();

private:

    struct job_queue_entry
    {
        void (*function)(void*, void*, job_handle);
        void* templatedFunction;

        std::atomic<int32> numUnfinishedJobs;
        int32 parentGlobalIndex; // Always in the same queue.
        job_handle continuation;


        static constexpr uint64 SIZE = sizeof(function) + sizeof(templatedFunction) + sizeof(numUnfinishedJobs) + sizeof(parentGlobalIndex) + sizeof(continuation);
        static constexpr uint64 DATA_SIZE = (2 * 64) - SIZE;

        uint8 data[DATA_SIZE];
    };

    static_assert(sizeof(job_queue_entry) % 64 == 0);



    friend struct job_handle;

    void addContinuation(int32 firstGlobalIndex, job_handle second);
    void submit(int32 globalIndex);
    void waitForCompletion(int32 globalIndex);


    void finishJob(int32 globalIndex);
    bool executeNextJob();
    void threadFunc(int32 threadIndex);


    moodycamel::ConcurrentQueue<int32> queue;
    std::atomic<uint32> runningJobs = 0;

    static constexpr int32 capacity = 4096;
    static constexpr int32 indexMask = capacity - 1;

    job_queue_entry allJobs[capacity];
    std::atomic<uint32> nextFreeJob = 0;

    int32 queueIndex;

    std::condition_variable wakeCondition;
    std::mutex wakeMutex;
};

extern job_queue highPriorityJobQueue;
extern job_queue lowPriorityJobQueue;
extern job_queue mainThreadJobQueue;


void initializeJobSystem();
void executeMainThreadJobs();

