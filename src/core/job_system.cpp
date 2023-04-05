#include "pch.h"
#include "math.h"

struct job_handle
{
    int32 index = -1;
};

struct job_queue_entry
{
    void (*function)(void*, void*, job_handle);
    void* templatedFunction;

    std::atomic<int32> numUnfinishedJobs;
    job_handle parent;
    job_handle continuation;


    static constexpr uint64 SIZE = sizeof(function) + sizeof(templatedFunction) + sizeof(numUnfinishedJobs) + sizeof(parent) + sizeof(continuation);
    static constexpr uint64 DATA_SIZE = 128 - SIZE;

    uint8 data[DATA_SIZE];
};

static_assert(sizeof(job_queue_entry) % 64 == 0);


template <typename data_t> 
using job_function = void (*)(data_t*, job_handle);

struct job_queue
{
    void initialize()
    {
        for (uint32 i = 0; i < 4; ++i)
        {
            std::thread thread([this, i]() { threadFunc(i); });
            thread.detach();
        }
    }

    template <typename data_t,
        typename = std::enable_if_t<sizeof(data_t) <= job_queue_entry::DATA_SIZE>>
    job_handle createJob(job_function<data_t> function, const data_t& data, job_handle parent = {})
    {
        auto [handle, job] = allocateJob();
        job.numUnfinishedJobs = 1;
        job.parent = parent;

        if (parent.index != -1)
        {
            ++allJobs[parent.index].numUnfinishedJobs;
        }

        job.templatedFunction = function;
        job.function = [](void* templatedFunction, void* rawData, job_handle job)
        {
            data_t* data = (data_t*)rawData;

            auto function = (job_function<data_t>)templatedFunction;
            function(data, job);

            data->~data_t();
        };

        new(job.data) data_t(data);

        return handle;
    }

    void addContinuation(job_handle first, job_handle second)
    {
        job_queue_entry& firstJob = allJobs[first.index];
        ASSERT(firstJob.continuation.index == -1);
        firstJob.continuation = second;
    }

    void submit(job_handle handle)
    {
        if (handle.index != -1)
        {
            ++numSubmittedJobs;
            runningJobs[nextJobToWrite++ & indexMask] = handle;
            wakeCondition.notify_one();
        }
    }

    void waitForCompletion()
    {
        while (numSubmittedJobs != numCompletedJobs)
        {
            executeNextJob();
        }
    }

    void waitForCompletion(job_handle handle)
    {
        if (handle.index != -1)
        {
            job_queue_entry& job = allJobs[handle.index];

            while (job.numUnfinishedJobs > 0) 
            {
                executeNextJob();
            }
        }
    }

private:

    std::pair<job_handle, job_queue_entry&> allocateJob()
    {
        int32 index = (int32)(nextFreeJob++ & indexMask);
        return { {index}, allJobs[index] };
    }

    void finishJob(job_handle handle)
    {
        job_queue_entry& job = allJobs[handle.index];
        int32 numUnfinishedJobs = --job.numUnfinishedJobs;
        ASSERT(numUnfinishedJobs >= 0);
        if (numUnfinishedJobs == 0)
        {
            if (job.parent.index != -1)
            {
                finishJob(job.parent);
            }

            if (job.continuation.index != -1)
            {
                submit(job.continuation);
            }
        }
    }

    bool executeNextJob()
    {
        uint32 index = nextJobToRead.load();
        if (index != nextJobToWrite)
        {
            if (nextJobToRead.compare_exchange_weak(index, index + 1))
            {
                job_handle handle = runningJobs[index & indexMask];
                job_queue_entry& job = allJobs[handle.index];
                job.function(job.templatedFunction, job.data, handle);

                finishJob(handle);

                ++numCompletedJobs;
            }

            return true;
        }

        return false;
    }

    void threadFunc(int32 threadIndex)
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



    static constexpr uint32 capacity = 4096;
    static constexpr uint32 indexMask = capacity - 1;

    job_queue_entry allJobs[capacity];
    std::atomic<uint32> nextFreeJob = 0;

    job_handle runningJobs[capacity];
    std::atomic<uint32> nextJobToWrite = 0;
    std::atomic<uint32> nextJobToRead = 0;

    std::atomic<uint64> numSubmittedJobs = 0;
    std::atomic<uint64> numCompletedJobs = 0;

    std::condition_variable wakeCondition;
    std::mutex wakeMutex;
};




static job_queue queue;




















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

    job_handle loadJob = queue.createJob<load_texture_thread_data>([](load_texture_thread_data* data, job_handle job)
    {
        char buffer[128];
        sprintf(buffer, "JOB %d: Loading texture %d\n", job.index, data->index);
        std::cout << buffer;
        data->data->index = data->index;
        Sleep(1000);
    }, loadData, parent);


    queue.submit(loadJob);
    
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

    job_handle loadJob = queue.createJob<load_mesh_thread_data>([](load_mesh_thread_data* data, job_handle job)
    {
        char buffer[128];
        sprintf(buffer, "JOB %d: Loading mesh\n", job.index);
        std::cout << buffer;

        data->m->textures.resize(10);

        for (int32 i = 0; i < (int32)data->m->textures.size(); ++i)
        {
            data->m->textures[i] = loadTextureAsync(i, job);
        }
    }, loadData);




    struct blas_thread_data
    {
        ref<mesh> data;
    };


    blas_thread_data blasData = { result };

    job_handle blasJob = queue.createJob<blas_thread_data>([](blas_thread_data* data, job_handle job)
    {
        char buffer[128];
        sprintf(buffer, "JOB %d: Creating blas\n", job.index);
        std::cout << buffer;
    }, blasData);


    queue.addContinuation(loadJob, blasJob);
    queue.submit(loadJob);

    return { result, blasJob };
}

void testJobSystem()
{
    queue.initialize();

    auto [result, loadJob] = loadMeshAsync();

    std::cout << "Test\n";

    Sleep(1000);
    queue.waitForCompletion(loadJob);

    std::cout << "Mesh loaded\n";




}



