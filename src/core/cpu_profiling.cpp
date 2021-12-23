#include "pch.h"
#define PROFILING_INTERNAL
#include "cpu_profiling.h"
#include "dx/dx_context.h"
#include "core/imgui.h"


bool cpuProfilerWindowOpen = false;

#if ENABLE_CPU_PROFILING

std::atomic<uint32> cpuProfileIndex;
std::atomic<uint32> cpuProfileCompletelyWritten[2];
profile_event cpuProfileEvents[2][MAX_NUM_CPU_PROFILE_EVENTS];
profile_stat cpuProfileStats[2][MAX_NUM_CPU_PROFILE_STATS];


#define MAX_NUM_CPU_PROFILE_THREADS 128
#define MAX_NUM_CPU_PROFILE_FRAMES 1024


struct cpu_profile_frame : profile_frame
{
	uint16 firstTopLevelBlockPerThread[MAX_NUM_CPU_PROFILE_THREADS];

	profile_block profileBlockPool[MAX_NUM_CPU_PROFILE_BLOCKS];
	uint32 totalNumProfileBlocks;

	profile_stat stats[MAX_NUM_CPU_PROFILE_STATS];
	uint32 numStats;
};

static uint32 profileThreads[MAX_NUM_CPU_PROFILE_THREADS];
static char profileThreadNames[MAX_NUM_CPU_PROFILE_THREADS][32];
static uint32 numThreads;

static cpu_profile_frame profileFrames[MAX_NUM_CPU_PROFILE_FRAMES];
static uint32 profileFrameWriteIndex;

static cpu_profile_frame dummyFrames[2];
static uint32 dummyFrameWriteIndex;


static bool pauseRecording;


static uint16 stack[MAX_NUM_CPU_PROFILE_THREADS][1024];
static uint32 depth[MAX_NUM_CPU_PROFILE_THREADS];

static uint32 mapThreadIDToIndex(uint32 threadID)
{
	for (uint32 i = 0; i < numThreads; ++i)
	{
		if (profileThreads[i] == threadID)
		{
			return i;
		}
	}

	HANDLE handle = OpenThread(THREAD_ALL_ACCESS, false, threadID);
	assert(handle);
	WCHAR* description = nullptr;
	checkResult(GetThreadDescription(handle, &description));
	CloseHandle(handle);

	if (!description || !description[0])
	{
		description = L"Main thread";
	}

	assert(numThreads < MAX_NUM_CPU_PROFILE_THREADS);
	uint32 index = numThreads++;
	profileThreads[index] = threadID;
	snprintf(profileThreadNames[index], sizeof(profileThreadNames[index]), "Thread %u (%ws)", threadID, description);
	return index;
}

static void initializeNewFrame(cpu_profile_frame& oldFrame, cpu_profile_frame& newFrame)
{
	for (uint32 thread = 0; thread < MAX_NUM_CPU_PROFILE_THREADS; ++thread)
	{
		if (depth[thread] > 0)
		{
			// Some blocks are still running on this thread.
			copyProfileBlocks(oldFrame.profileBlockPool, stack[thread], depth[thread], newFrame.profileBlockPool, newFrame.totalNumProfileBlocks);
			newFrame.firstTopLevelBlockPerThread[thread] = stack[thread][0];
		}
		else
		{
			newFrame.firstTopLevelBlockPerThread[thread] = INVALID_PROFILE_BLOCK;
			stack[thread][0] = INVALID_PROFILE_BLOCK;
		}
	}
}

void cpuProfilingResolveTimeStamps()
{
	uint32 currentFrame = profileFrameWriteIndex;

	uint32 arrayIndex = _CPU_PROFILE_GET_ARRAY_INDEX(cpuProfileIndex); // We are only interested in the most significant bit here, so don't worry about thread safety.
	uint32 currentIndex = cpuProfileIndex.exchange((1 - arrayIndex) << 31); // Swap array and get current event count.

	profile_event* events = cpuProfileEvents[arrayIndex];
	uint32 numEvents = _CPU_PROFILE_GET_EVENT_INDEX(currentIndex);
	auto stats = cpuProfileStats[arrayIndex];
	uint32 numStats = _CPU_PROFILE_GET_STAT_INDEX(currentIndex);
	uint32 numWrites = numEvents + numStats;

	while (numWrites > cpuProfileCompletelyWritten[arrayIndex]) {} // Wait until all events and stats have been written completely.
	cpuProfileCompletelyWritten[arrayIndex] = 0;


	static bool initializedStack = false;

	// Initialize on the very first frame.
	if (!initializedStack)
	{
		for (uint32 thread = 0; thread < MAX_NUM_CPU_PROFILE_THREADS; ++thread)
		{
			stack[thread][0] = INVALID_PROFILE_BLOCK;
			profileFrames[0].firstTopLevelBlockPerThread[thread] = INVALID_PROFILE_BLOCK;
			depth[thread] = 0;
		}

		initializedStack = true;
	}



	
	CPU_PROFILE_BLOCK("CPU Profiling"); // Important: Must be after array swap!

	{
		CPU_PROFILE_BLOCK("Collate profile events from last frame");

		std::stable_sort(events, events + numEvents, [](const profile_event& a, const profile_event& b)
		{
			return a.timestamp < b.timestamp;
		});


		cpu_profile_frame* frame = !pauseRecording ? (profileFrames + profileFrameWriteIndex) : (dummyFrames + dummyFrameWriteIndex);

		for (uint32 i = 0; i < numEvents; ++i)
		{
			profile_event* e = events + i;
			uint32 threadID = e->threadID;
			uint32 threadIndex = mapThreadIDToIndex(threadID);

			uint32 blocksBefore = frame->totalNumProfileBlocks;

			uint64 frameEndTimestamp;
			if (handleProfileEvent(events, i, numEvents, stack[threadIndex], depth[threadIndex], frame->profileBlockPool, frame->totalNumProfileBlocks, frameEndTimestamp, false))
			{
				static uint64 clockFrequency;
				static bool performanceFrequencyQueried = QueryPerformanceFrequency((LARGE_INTEGER*)&clockFrequency);

				cpu_profile_frame* previousFrame;
				if (!pauseRecording)
				{
					uint32 previousFrameIndex = (profileFrameWriteIndex == 0) ? (MAX_NUM_CPU_PROFILE_FRAMES - 1) : (profileFrameWriteIndex - 1);
					previousFrame = profileFrames + previousFrameIndex;
				}
				else
				{
					uint32 previousFrameIndex = 1 - dummyFrameWriteIndex;
					previousFrame = dummyFrames + previousFrameIndex;
				}


				frame->startClock = (previousFrame->endClock == 0) ? frameEndTimestamp : previousFrame->endClock;
				frame->endClock = frameEndTimestamp;
				frame->globalFrameID = dxContext.frameID;

				frame->duration = (float)(frame->endClock - frame->startClock) / clockFrequency * 1000.f;

				for (uint32 i = 0; i < frame->totalNumProfileBlocks; ++i)
				{
					profile_block* block = frame->profileBlockPool + i;

					uint64 endClock = (block->endClock == 0) ? frame->endClock : block->endClock;
					assert(endClock <= frame->endClock);

					if (block->startClock >= frame->startClock)
					{
						block->relStart = (float)(block->startClock - frame->startClock) / clockFrequency * 1000.f;
					}
					else
					{
						// For blocks which started in a previous frame.
						block->relStart = -(float)(frame->startClock - block->startClock) / clockFrequency * 1000.f;
					}
					block->duration = (float)(endClock - block->startClock) / clockFrequency * 1000.f;
				}

				frame->numStats = numStats;
				memcpy(frame->stats, stats, numStats * sizeof(profile_stat));


				cpu_profile_frame* oldFrame = frame;

				if (!pauseRecording)
				{
					profileFrameWriteIndex = (profileFrameWriteIndex + 1) % MAX_NUM_CPU_PROFILE_FRAMES;
					frame = profileFrames + profileFrameWriteIndex;
				}
				else
				{
					dummyFrameWriteIndex = 1 - dummyFrameWriteIndex;
					frame = dummyFrames + dummyFrameWriteIndex;
				}

				frame->totalNumProfileBlocks = 0;

				initializeNewFrame(*oldFrame, *frame);
			}
			else
			{
				// Set first top-level block if another block was added and this thread has no first top-level block yet.
				uint32 blocksAfter = frame->totalNumProfileBlocks;

				if (blocksBefore != blocksAfter && frame->firstTopLevelBlockPerThread[threadIndex] == INVALID_PROFILE_BLOCK)
				{
					frame->firstTopLevelBlockPerThread[threadIndex] = frame->totalNumProfileBlocks - 1;
				}
			}

		}
	}


	if (cpuProfilerWindowOpen)
	{
		CPU_PROFILE_BLOCK("Display profiling");

		if (ImGui::Begin(ICON_FA_CHART_LINE "  CPU Profiling", &cpuProfilerWindowOpen))
		{
			static profiler_persistent persistent;
			profiler_timeline timeline(persistent, MAX_NUM_CPU_PROFILE_FRAMES);

			if (timeline.drawHeader(pauseRecording))
			{
				// Recording has been stopped/resumed. Swap array into which is recorded.
				if (pauseRecording)
				{
					cpu_profile_frame& oldFrame = profileFrames[profileFrameWriteIndex];
					cpu_profile_frame& newFrame = dummyFrames[dummyFrameWriteIndex];
					initializeNewFrame(oldFrame, newFrame);
				}
				else
				{
					cpu_profile_frame& oldFrame = dummyFrames[dummyFrameWriteIndex];
					cpu_profile_frame& newFrame = profileFrames[profileFrameWriteIndex];
					initializeNewFrame(oldFrame, newFrame);
				}
			}

			for (uint32 frameIndex = 0; frameIndex < MAX_NUM_CPU_PROFILE_FRAMES; ++frameIndex)
			{
				timeline.drawOverviewFrame(profileFrames[frameIndex], frameIndex, currentFrame);
			}
			timeline.endOverview();



			if (persistent.highlightFrameIndex != -1)
			{
				cpu_profile_frame& frame = profileFrames[persistent.highlightFrameIndex];
				profile_block* blocks = profileFrames[persistent.highlightFrameIndex].profileBlockPool;

				if (frame.totalNumProfileBlocks)
				{
					uint32 threadIndices[MAX_NUM_CPU_PROFILE_THREADS];
					const char* threadNames[MAX_NUM_CPU_PROFILE_THREADS];
					uint32 numActiveThreadsThisFrame = 0;

					for (uint32 i = 0; i < numThreads; ++i)
					{
						if (frame.firstTopLevelBlockPerThread[i] != INVALID_PROFILE_BLOCK)
						{
							threadIndices[numActiveThreadsThisFrame] = i;
							threadNames[numActiveThreadsThisFrame] = profileThreadNames[i];
							++numActiveThreadsThisFrame;
						}
					}


					timeline.drawHighlightFrameInfo(frame);

					static uint32 threadIndex = 0;
					ImGui::SameLine();
					ImGui::Dropdown("Thread", threadNames, numActiveThreadsThisFrame, threadIndex);

					if (persistent.highlightFrameIndex != profileFrameWriteIndex)
					{
						timeline.drawCallStack(blocks, frame.firstTopLevelBlockPerThread[threadIndices[threadIndex]]);
					}

					timeline.drawMillisecondSpacings(frame);
					timeline.handleUserInteractions();

					if (frame.numStats != 0)
					{
						ImGui::Dummy(ImVec2(0, 30.f));
						if (ImGui::BeginChild("Stats"))
						{
							ImGui::Text("Frame stats");
							ImGui::Separator();
							for (uint32 i = 0; i < frame.numStats; ++i)
							{
								const profile_stat& stat = frame.stats[i];
								switch (stat.type)
								{
									case profile_stat_type_bool: ImGui::Value(stat.label, stat.boolValue); break;
									case profile_stat_type_int32: ImGui::Value(stat.label, stat.int32Value); break;
									case profile_stat_type_uint32: ImGui::Value(stat.label, stat.uint32Value); break;
									case profile_stat_type_int64: ImGui::Value(stat.label, stat.int64Value); break;
									case profile_stat_type_uint64: ImGui::Value(stat.label, stat.uint64Value); break;
									case profile_stat_type_float: ImGui::Value(stat.label, stat.floatValue); break;
									case profile_stat_type_string: ImGui::Value(stat.label, stat.stringValue); break;
								}
							}
						}
						ImGui::EndChild();
					}
				}
			}
		}
		ImGui::End();
	}
}

#endif
