#pragma once

#include "dx_command_list.h"
#include "threading.h"

#if ENABLE_DX_PROFILING

#define COMPOSITE_VARNAME_(a, b) a##b
#define COMPOSITE_VARNAME(a, b) COMPOSITE_VARNAME_(a, b)

#define DX_PROFILE_BLOCK_(counter, cl, name) dx_profile_block_recorder COMPOSITE_VARNAME(__DX_PROFILE_BLOCK, counter)(cl, name)
#define DX_PROFILE_BLOCK(cl, name) DX_PROFILE_BLOCK_(__COUNTER__, cl, name)

#define MAX_NUM_DX_PROFILE_BLOCKS 2048
#define MAX_NUM_DX_PROFILE_EVENTS (MAX_NUM_DX_PROFILE_BLOCKS * 2) // One for start and end.
#define MAX_NUM_DX_PROFILE_FRAMES 1024

enum profile_event_type
{
	profile_event_frame_marker,
	profile_event_begin_block,
	profile_event_end_block,
};

enum profile_cl_type
{
	profile_cl_graphics,
	profile_cl_compute,

	profile_cl_count,
};

struct dx_profile_event
{
	profile_event_type type;
	profile_cl_type clType;
	const char* name;
	uint64 timestamp;
};

extern dx_profile_event profileEvents[NUM_BUFFERED_FRAMES][MAX_NUM_DX_PROFILE_EVENTS];
extern bool profilerWindowOpen;

struct dx_profile_block_recorder
{
	dx_command_list* cl;
	const char* name;

	dx_profile_block_recorder(dx_command_list* cl, const char* name)
		: cl(cl), name(name)
	{
		uint32 queryIndex = atomicIncrement(dxContext.timestampQueryIndex[dxContext.bufferedFrameID]);
		cl->queryTimestamp(queryIndex);

		profileEvents[dxContext.bufferedFrameID][queryIndex] = { profile_event_begin_block, cl->type == D3D12_COMMAND_LIST_TYPE_DIRECT ? profile_cl_graphics : profile_cl_compute, name };
	}

	~dx_profile_block_recorder()
	{
		uint32 queryIndex = atomicIncrement(dxContext.timestampQueryIndex[dxContext.bufferedFrameID]);
		cl->queryTimestamp(queryIndex);

		profileEvents[dxContext.bufferedFrameID][queryIndex] = { profile_event_end_block, cl->type == D3D12_COMMAND_LIST_TYPE_DIRECT ? profile_cl_graphics : profile_cl_compute, name };
	}
};

#else

#define DX_PROFILE_BLOCK(cl, name)

#endif
