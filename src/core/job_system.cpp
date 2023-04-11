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

void job_queue::addContinuation(int32 first, int32 second)
{
    job_queue_entry& firstJob = allJobs[first];
    ASSERT(firstJob.continuation == -1);

    int32 unfinished = firstJob.numUnfinishedJobs++;
    if (unfinished == 0)
    {
        // First job was finished before adding continuation -> just submit second.
        submit(second);
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

        if (job.continuation != -1)
        {
            submit(job.continuation);
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
    ASSERT(queue == before.queue);
    queue->addContinuation(before.index, index);
}

void job_handle::waitForCompletion()
{
    queue->waitForCompletion(index);
}




job_queue highPriorityJobQueue;
job_queue lowPriorityJobQueue;




















struct texture
{
    int32 index;

    ~texture()
    {
        std::cout << "Texture destructor\n";
    }
};

struct mesh
{
    std::vector<ref<texture>> textures;
};

static ref<texture> loadTextureAsync(int32 index, job_handle parent = {})
{
    ref<texture> result = make_ref<texture>();


    struct load_texture_thread_data
    {
        ref<texture> data;
        int32 index;
    };

    load_texture_thread_data loadData = { result, index };

    job_handle loadJob = lowPriorityJobQueue.createJob<load_texture_thread_data>([](load_texture_thread_data& data, job_handle job)
    {
        char buffer[128];
        sprintf(buffer, "JOB %d: Loading texture %d\n", job.index, data.index);
        std::cout << buffer;
        data.data->index = data.index;
        Sleep(1000);
    }, loadData, parent);


    loadJob.submitNow();
    
    return result;
}

static std::pair<ref<mesh>, job_handle> loadMeshAsync(job_handle parent = {})
{
    ref<mesh> result = make_ref<mesh>();

    struct load_mesh_thread_data
    {
        ref<mesh> m;
    };

    load_mesh_thread_data loadData = { result };

    job_handle loadJob = lowPriorityJobQueue.createJob<load_mesh_thread_data>([](load_mesh_thread_data& data, job_handle job)
    {
        char buffer[128];
        sprintf(buffer, "JOB %d: Loading mesh\n", job.index);
        std::cout << buffer;

        data.m->textures.resize(10);

        for (int32 i = 0; i < (int32)data.m->textures.size(); ++i)
        {
            data.m->textures[i] = loadTextureAsync(i, job);
        }
    }, loadData);




    struct blas_thread_data
    {
        ref<mesh> data;
    };


    blas_thread_data blasData = { result };

    job_handle blasJob = lowPriorityJobQueue.createJob<blas_thread_data>([](blas_thread_data& data, job_handle job)
    {
        char buffer[128];
        sprintf(buffer, "JOB %d: Creating blas\n", job.index);
        std::cout << buffer;
    }, blasData);


    loadJob.submitNow();
    blasJob.submitAfter(loadJob);

    return { result, blasJob };
}

void initializeJobSystem()
{
    HANDLE handle = GetCurrentThread();
    SetThreadAffinityMask(handle, 1);
    SetThreadPriority(handle, THREAD_PRIORITY_HIGHEST);
    CloseHandle(handle);

    //uint32 numHardwareThreads = std::thread::hardware_concurrency();


    highPriorityJobQueue.initialize(4, 1, THREAD_PRIORITY_NORMAL, L"High priority worker");
    lowPriorityJobQueue.initialize(4, 5, THREAD_PRIORITY_BELOW_NORMAL, L"Low priority worker");


#if 0
    auto [result, loadJob] = loadMeshAsync();

    std::cout << "Test\n";

    Sleep(1000);
    loadJob.waitForCompletion();

    std::cout << "Mesh loaded\n";
#endif
}
