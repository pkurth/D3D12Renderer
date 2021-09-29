#include "pch.h"
#define PROFILING_INTERNAL
#include "cpu_profiling.h"
#include "dx/dx_context.h"
#include "core/imgui.h"


bool cpuProfilerWindowOpen = false;

#if ENABLE_CPU_PROFILING

volatile uint64 cpuProfileArrayAndEventIndex = 0;
profile_event cpuProfileEvents[2][MAX_NUM_CPU_PROFILE_EVENTS];


#define MAX_NUM_CPU_PROFILE_THREADS 128
#define MAX_NUM_CPU_PROFILE_FRAMES 1024


struct cpu_profile_frame : profile_frame
{
	uint16 firstTopLevelBlockPerThread[MAX_NUM_CPU_PROFILE_THREADS];
	uint32 totalNumProfileBlocks;
};

struct cpu_profile_thread
{
	uint32 id;
};

static cpu_profile_thread profileThreads[MAX_NUM_CPU_PROFILE_THREADS];
static uint32 numThreads;

static cpu_profile_frame profileFrames[MAX_NUM_CPU_PROFILE_FRAMES];
static uint32 profileFrameWriteIndex;

static bool pauseRecording;

static profile_block profileBlockPool[MAX_NUM_CPU_PROFILE_FRAMES][MAX_NUM_CPU_PROFILE_BLOCKS];


static uint32 mapThreadIDToIndex(uint32 threadID)
{
	for (uint32 i = 0; i < numThreads; ++i)
	{
		if (profileThreads[i].id == threadID)
		{
			return i;
		}
	}

	assert(numThreads < MAX_NUM_CPU_PROFILE_THREADS);
	uint32 index = numThreads++;
	profileThreads[index] = { threadID };
	return index;
}

void cpuProfilingResolveTimeStamps()
{
	uint32 currentFrame = profileFrameWriteIndex;

	uint32 arrayIndex = (uint32)(cpuProfileArrayAndEventIndex >> 32); // We are only interested in upper 32 bits here, so don't worry about thread safety.
	uint64 currentArrayAndEventIndex = atomicExchange(cpuProfileArrayAndEventIndex, (uint64)(1 - arrayIndex) << 32); // Swap array and get current event count.
	
	profile_event* events = cpuProfileEvents[arrayIndex];
	uint32 numEvents = (uint32)currentArrayAndEventIndex;

	if (!pauseRecording)
	{
		std::stable_sort(events, events + numEvents, [](const profile_event& a, const profile_event& b)
		{
			return a.timestamp < b.timestamp;
		});


		uint16 stack[MAX_NUM_CPU_PROFILE_THREADS][1024];
		uint32 depth[MAX_NUM_CPU_PROFILE_THREADS] = {};

		for (uint32 thread = 0; thread < MAX_NUM_CPU_PROFILE_THREADS; ++thread)
		{
			stack[thread][0] = INVALID_PROFILE_BLOCK;
		}


		cpu_profile_frame* frame = profileFrames + profileFrameWriteIndex;

		// Only valid on the very first frame.
		if (frame->endClock == 0)
		{
			for (uint32 thread = 0; thread < MAX_NUM_CPU_PROFILE_THREADS; ++thread)
			{
				frame->firstTopLevelBlockPerThread[thread] = INVALID_PROFILE_BLOCK;
			}
		}


		for (uint32 i = 0; i < numEvents; ++i)
		{
			profile_event* e = events + i;
			uint32 threadID = e->threadID;
			uint32 threadIndex = mapThreadIDToIndex(threadID);

			uint32 blocksBefore = frame->totalNumProfileBlocks;

			uint64 frameEndTimestamp;
			if (handleProfileEvent(events, i, numEvents, stack[threadIndex], depth[threadIndex], profileBlockPool[profileFrameWriteIndex], frame->totalNumProfileBlocks, frameEndTimestamp))
			{
				static uint64 clockFrequency;
				static bool performanceFrequencyQueried = QueryPerformanceFrequency((LARGE_INTEGER*)&clockFrequency);

				uint32 previousFrameIndex = (profileFrameWriteIndex == 0) ? (MAX_NUM_CPU_PROFILE_FRAMES - 1) : (profileFrameWriteIndex - 1);
				cpu_profile_frame& previousFrame = profileFrames[previousFrameIndex];

				frame->startClock = (previousFrame.endClock == 0) ? frameEndTimestamp : previousFrame.endClock;
				frame->endClock = frameEndTimestamp;
				frame->globalFrameID = dxContext.frameID;

				frame->duration = (float)(frame->endClock - frame->startClock) / clockFrequency * 1000.f;

				for (uint32 i = 0; i < frame->totalNumProfileBlocks; ++i)
				{
					profile_block* block = profileBlockPool[profileFrameWriteIndex] + i;

					assert(block->endClock <= frame->endClock);

					block->relStart = (float)(block->startClock - frame->startClock) / clockFrequency * 1000.f;
					block->duration = (float)(block->endClock - block->startClock) / clockFrequency * 1000.f;
				}



				++profileFrameWriteIndex;
				if (profileFrameWriteIndex >= MAX_NUM_CPU_PROFILE_FRAMES)
				{
					profileFrameWriteIndex = 0;
				}


				frame = profileFrames + profileFrameWriteIndex;
				frame->totalNumProfileBlocks = 0;

				for (uint32 thread = 0; thread < MAX_NUM_CPU_PROFILE_THREADS; ++thread)
				{
					frame->firstTopLevelBlockPerThread[thread] = INVALID_PROFILE_BLOCK;
				}
			}

			uint32 blocksAfter = frame->totalNumProfileBlocks;

			if (blocksBefore != blocksAfter && frame->firstTopLevelBlockPerThread[threadIndex] == INVALID_PROFILE_BLOCK)
			{
				frame->firstTopLevelBlockPerThread[threadIndex] = frame->totalNumProfileBlocks - 1;
			}
		}
	}


	static uint32 highlightFrameIndex = -1;


	if (cpuProfilerWindowOpen)
	{
		if (ImGui::Begin("CPU Profiling", &cpuProfilerWindowOpen))
		{
			profiler_timeline timeline;
			timeline.begin(MAX_NUM_CPU_PROFILE_FRAMES);

			timeline.drawHeader(pauseRecording);

			for (uint32 frameIndex = 0; frameIndex < MAX_NUM_CPU_PROFILE_FRAMES; ++frameIndex)
			{
				timeline.drawOverviewFrame(profileFrames[frameIndex], frameIndex, highlightFrameIndex, currentFrame);
			}
			timeline.endOverview();



			if (highlightFrameIndex != -1)
			{
				cpu_profile_frame& frame = profileFrames[highlightFrameIndex];
				profile_block* blocks = profileBlockPool[highlightFrameIndex];

				if (frame.totalNumProfileBlocks)
				{
					static float frameWidthMultiplier = 1.f;
					static float callstackLeftPadding = leftPadding;

					static float horizontalScrollAnchor = 0;
					static bool horizontalScrolling = false;

					for (uint32 thread = 0; thread < numThreads; ++thread)
					{
						timeline.drawCallStack(blocks, frameWidthMultiplier, callstackLeftPadding, frame.firstTopLevelBlockPerThread[thread]);
					}

					timeline.drawMillisecondSpacings(frame, frameWidthMultiplier, callstackLeftPadding);
					timeline.handleUserInteractions(frameWidthMultiplier, callstackLeftPadding,
						horizontalScrollAnchor, horizontalScrolling);

				}
			}
		}
		ImGui::End();
	}

}

#endif
