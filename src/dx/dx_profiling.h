#pragma once

#include "dx_command_list.h"
#include "core/profiling_internal.h"
#include "core/cpu_profiling.h"

extern bool dxProfilerWindowOpen;


#if ENABLE_DX_PROFILING

#define DX_PROFILE_BLOCK_(counter, cl, name) dx_profile_block_recorder COMPOSITE_VARNAME(__DX_PROFILE_BLOCK, counter)(cl, name)
#define DX_PROFILE_BLOCK(cl, name) DX_PROFILE_BLOCK_(__COUNTER__, cl, name)


#define MAX_NUM_DX_PROFILE_BLOCKS 2048
#define MAX_NUM_DX_PROFILE_EVENTS (MAX_NUM_DX_PROFILE_BLOCKS * 2) // One for start and end.


enum profile_cl_type
{
	profile_cl_graphics,
	profile_cl_compute,

	profile_cl_count,
};

extern profile_event dxProfileEvents[NUM_BUFFERED_FRAMES][MAX_NUM_DX_PROFILE_EVENTS];

struct dx_profile_block_recorder
{
	dx_command_list* cl;
	const char* name;

	dx_profile_block_recorder(dx_command_list* cl, const char* name)
		: cl(cl), name(name)
	{
		uint32 queryIndex = atomicIncrement(dxContext.timestampQueryIndex[dxContext.bufferedFrameID]);
		cl->queryTimestamp(queryIndex);

		dxProfileEvents[dxContext.bufferedFrameID][queryIndex] = 
		{ 
			profile_event_begin_block,
			(uint16)((cl->type == D3D12_COMMAND_LIST_TYPE_DIRECT) ? profile_cl_graphics : profile_cl_compute),
			getThreadIDFast(),
			name 
		};
	}

	~dx_profile_block_recorder()
	{
		uint32 queryIndex = atomicIncrement(dxContext.timestampQueryIndex[dxContext.bufferedFrameID]);
		cl->queryTimestamp(queryIndex);

		dxProfileEvents[dxContext.bufferedFrameID][queryIndex] = 
		{ 
			profile_event_end_block,
			(uint16)((cl->type == D3D12_COMMAND_LIST_TYPE_DIRECT) ? profile_cl_graphics : profile_cl_compute),
			getThreadIDFast(),
			name 
		};
	}
};

void dxProfilingFrameEndMarker(dx_command_list* cl);
void dxProfilingResolveTimeStamps(uint64* timestamps);

#else

#define DX_PROFILE_BLOCK(...)

#define dxProfilingFrameEndMarker(...)
#define dxProfilingResolveTimeStamps(...)

#endif


#define PROFILE_ALL(cl, name) DX_PROFILE_BLOCK(cl, name); CPU_PROFILE_BLOCK(name)

