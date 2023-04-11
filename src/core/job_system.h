#pragma once

#include <concurrentqueue/concurrentqueue.h>

struct job_handle
{
    int32 index = -1;
    struct job_queue* queue;

    void submitNow();
    void submitAfter(job_handle before);
    void waitForCompletion();
};

template <typename data_t>
using job_function = void (*)(data_t&, job_handle);

struct job_queue
{
    struct job_queue_entry
    {
        void (*function)(void*, void*, job_handle);
        void* templatedFunction;

        std::atomic<int32> numUnfinishedJobs;
        int32 parent;
        job_handle continuation;


        static constexpr uint64 SIZE = sizeof(function) + sizeof(templatedFunction) + sizeof(numUnfinishedJobs) + sizeof(parent) + sizeof(continuation);
        static constexpr uint64 DATA_SIZE = (3 * 64) - SIZE;

        uint8 data[DATA_SIZE];
    };

    static_assert(sizeof(job_queue_entry) % 64 == 0);



    void initialize(uint32 numThreads, uint32 threadOffset, int threadPriority, const wchar* description);

    template <typename data_t,
        typename = std::enable_if_t<sizeof(data_t) <= job_queue_entry::DATA_SIZE>>
        job_handle createJob(job_function<data_t> function, const data_t& data, job_handle parent = {})
    {
        int32 handle = allocateJob();
        auto& job = allJobs[handle];
        job.numUnfinishedJobs = 1;
        job.parent = parent.index;
        job.continuation.index = -1;

        if (parent.index != -1)
        {
            ++allJobs[parent.index].numUnfinishedJobs;
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

        return job_handle{ handle, this };
    }


    void waitForCompletion();

private:

    friend struct job_handle;

    void addContinuation(int32 first, job_handle second);
    void submit(int32 handle);
    void waitForCompletion(int32 handle);


    int32 allocateJob();
    void finishJob(int32 handle);
    bool executeNextJob();
    void threadFunc(int32 threadIndex);


    moodycamel::ConcurrentQueue<int32> queue;
    std::atomic<uint32> runningJobs = 0;

    static constexpr uint32 capacity = 4096;
    static constexpr uint32 indexMask = capacity - 1;

    job_queue_entry allJobs[capacity];
    std::atomic<uint32> nextFreeJob = 0;


    std::condition_variable wakeCondition;
    std::mutex wakeMutex;
};

extern job_queue highPriorityJobQueue;
extern job_queue lowPriorityJobQueue;
extern job_queue mainThreadJobQueue;


void initializeJobSystem();
void executeMainThreadJobs();

