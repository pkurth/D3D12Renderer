#include "pch.h"
#define PROFILING_INTERNAL
#include "dx_profiling.h"
#include "core/imgui.h"
#include "core/math.h"


bool dxProfilerWindowOpen = false;

#if ENABLE_DX_PROFILING

#include <algorithm>
#include <fontawesome/IconsFontAwesome5.h>

profile_event dxProfileEvents[NUM_BUFFERED_FRAMES][MAX_NUM_DX_PROFILE_EVENTS];

#define MAX_NUM_DX_PROFILE_FRAMES 1024

struct dx_profile_frame : profile_frame
{
	profile_block blocks[profile_cl_count][MAX_NUM_DX_PROFILE_BLOCKS];
	uint32 count[profile_cl_count];
};

static dx_profile_frame profileFrames[MAX_NUM_DX_PROFILE_FRAMES];
static uint32 profileFrameWriteIndex;
static bool pauseRecording;

void dxProfilingFrameEndMarker(dx_command_list* cl)
{
	assert(cl->type == D3D12_COMMAND_LIST_TYPE_DIRECT);

	uint32 queryIndex = atomicIncrement(dxContext.timestampQueryIndex[dxContext.bufferedFrameID]);
	cl->queryTimestamp(queryIndex);

	dxProfileEvents[dxContext.bufferedFrameID][queryIndex] = 
	{ 
		profile_event_frame_marker,
		profile_cl_graphics, 
		"Frame End" };
}

void dxProfilingResolveTimeStamps(uint64* timestamps)
{
	uint32 currentFrame = profileFrameWriteIndex;

	if (!pauseRecording)
	{
		uint32 numQueries = dxContext.timestampQueryIndex[dxContext.bufferedFrameID];
		if (numQueries == 0)
		{
			return;
		}

		profile_event* events = dxProfileEvents[dxContext.bufferedFrameID];
		for (uint32 i = 0; i < numQueries; ++i)
		{
			events[i].timestamp = timestamps[i];
		}

		// Stable sort is important here, since occasionally two time stamps will have the exact same value.
		std::stable_sort(events, events + numQueries, [](const profile_event& a, const profile_event& b)
		{
			return a.timestamp < b.timestamp;
		});


#if 0
		for (uint32 i = 0; i < numQueries; ++i)
		{
			profile_event* e = events + i;
			std::cout << i << ") " << (e->type == profile_event_begin_block ? "BEGIN: " : "END  : ") << e->name << " (" << e->timestamp << ")\n";
		}
#endif



		uint16 stack[profile_cl_count][1024];
		uint32 depth[profile_cl_count] = {};
		uint32 count[profile_cl_count] = {};

		for (uint32 cl = 0; cl < profile_cl_count; ++cl)
		{
			stack[cl][0] = INVALID_PROFILE_BLOCK;
		}


		dx_profile_frame& frame = profileFrames[profileFrameWriteIndex];

		for (uint32 i = 0; i < numQueries; ++i)
		{
			profile_event* e = events + i;
			profile_cl_type clType = (profile_cl_type)e->laneIndex;

			uint64 frameEndTimestamp;
			if (handleProfileEvent(events, i, numQueries, stack[clType], depth[clType], frame.blocks[clType], count[clType], frameEndTimestamp))
			{
				uint32 previousFrameIndex = (profileFrameWriteIndex == 0) ? (MAX_NUM_DX_PROFILE_FRAMES - 1) : (profileFrameWriteIndex - 1);
				dx_profile_frame& previousFrame = profileFrames[previousFrameIndex];

				frame.startClock = (previousFrame.endClock == 0) ? frameEndTimestamp : previousFrame.endClock;
				frame.endClock = frameEndTimestamp;
				frame.globalFrameID = dxContext.frameID;

				frame.duration = (float)(frame.endClock - frame.startClock) / dxContext.renderQueue.timeStampFrequency * 1000.f;

				for (uint32 cl = 0; cl < profile_cl_count; ++cl)
				{
					frame.count[cl] = count[cl];

					uint64 freq = (cl == profile_cl_graphics) ? dxContext.renderQueue.timeStampFrequency : dxContext.computeQueue.timeStampFrequency;

					for (uint32 i = 0; i < frame.count[cl]; ++i)
					{
						profile_block& block = frame.blocks[cl][i];
						block.relStart = (float)(block.startClock - frame.startClock) / freq * 1000.f;
						block.duration = (float)(block.endClock - block.startClock) / freq * 1000.f;
					}
				}


				++profileFrameWriteIndex;
				if (profileFrameWriteIndex >= MAX_NUM_DX_PROFILE_FRAMES)
				{
					profileFrameWriteIndex = 0;
				}
			}
		}



#if 0
		{
			uint16 currentIndex = 0;
			uint32 depth = 0;

			profile_block* blocks = frame.blocks[0];

			while (currentIndex != INVALID_PROFILE_BLOCK)
			{
				profile_block* current = blocks + currentIndex;

				for (uint32 i = 0; i < depth; ++i)
				{
					std::cout << " ";
				}
				std::cout << current->name << '\n';

				// Advance.
				uint16 nextIndex = current->firstChild;
				if (nextIndex == INVALID_PROFILE_BLOCK)
				{
					nextIndex = current->nextSibling;

					if (nextIndex == INVALID_PROFILE_BLOCK)
					{
						uint16 nextAncestor = current->parent;
						while (nextAncestor != INVALID_PROFILE_BLOCK)
						{
							--depth;
							if (blocks[nextAncestor].nextSibling != INVALID_PROFILE_BLOCK)
							{
								nextIndex = blocks[nextAncestor].nextSibling;
								break;
							}
							nextAncestor = blocks[nextAncestor].parent;
						}
					}
				}
				else
				{
					++depth;
				}
				currentIndex = nextIndex;
			}
		}

		exit(0);
#endif

	}


	static uint32 highlightFrameIndex = -1;


	if (dxProfilerWindowOpen)
	{
		if (ImGui::Begin("GPU Profiling", &dxProfilerWindowOpen))
		{
			profiler_timeline timeline;
			timeline.begin(MAX_NUM_DX_PROFILE_FRAMES);

			timeline.drawHeader(pauseRecording);

			for (uint32 frameIndex = 0; frameIndex < MAX_NUM_DX_PROFILE_FRAMES; ++frameIndex)
			{
				timeline.drawOverviewFrame(profileFrames[frameIndex], frameIndex, highlightFrameIndex, currentFrame);
			}
			timeline.endOverview();


			if (highlightFrameIndex != -1)
			{
				static float frameWidthMultiplier = 1.f;
				static float callstackLeftPadding = leftPadding;

				static float horizontalScrollAnchor = 0;
				static bool horizontalScrolling = false;

				uint32 colorIndex = 0;

				dx_profile_frame& frame = profileFrames[highlightFrameIndex];

				timeline.drawHighlightFrameInfo(frame);

				profile_block* blocks = frame.blocks[profile_cl_graphics];
				uint32 count = frame.count[profile_cl_graphics];


				if (count > 0)
				{
					timeline.drawCallStack(blocks, frameWidthMultiplier, callstackLeftPadding, 0);
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
