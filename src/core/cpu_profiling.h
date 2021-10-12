#pragma once

#include "core/threading.h"
#include "profiling_internal.h"

extern bool cpuProfilerWindowOpen;


// Things the CPU profiler currently doesn't support (but probably should):
// - Filtering?


#if ENABLE_CPU_PROFILING

#define _CPU_PROFILE_BLOCK_(counter, name) cpu_profile_block_recorder COMPOSITE_VARNAME(__PROFILE_BLOCK, counter)(name)
#define CPU_PROFILE_BLOCK(name) _CPU_PROFILE_BLOCK_(__COUNTER__, name)

#define CPU_PROFILE_STAT(format, ...) cpuProfilingStat(format, __VA_ARGS__)



#define MAX_NUM_CPU_PROFILE_BLOCKS 16384
#define MAX_NUM_CPU_PROFILE_EVENTS (MAX_NUM_CPU_PROFILE_BLOCKS * 2) // One for start and end.
#define MAX_NUM_CPU_PROFILE_STATS 128
#define MAX_CPU_PROFILE_STAT_LENGTH 64


// 1 bit for array index, 1 free bit, 20 bits for events, 10 bits for stats.
#define _CPU_PROFILE_GET_ARRAY_INDEX(v) ((v) >> 31)
#define _CPU_PROFILE_GET_EVENT_INDEX(v) ((v) & 0xFFFFF)
#define _CPU_PROFILE_GET_STAT_INDEX(v)	(((v) >> 20) & 0x3FF)

#define recordProfileEvent(type_, name_) \
	extern profile_event cpuProfileEvents[2][MAX_NUM_CPU_PROFILE_EVENTS]; \
	extern std::atomic<uint32> cpuProfileIndex; \
	extern std::atomic<uint32> cpuProfileCompletelyWritten[2]; \
	uint32 arrayAndEventIndex = cpuProfileIndex++; \
	uint32 eventIndex = _CPU_PROFILE_GET_EVENT_INDEX(arrayAndEventIndex); \
	uint32 arrayIndex = _CPU_PROFILE_GET_ARRAY_INDEX(arrayAndEventIndex); \
	assert(eventIndex < MAX_NUM_CPU_PROFILE_EVENTS); \
	profile_event* e = cpuProfileEvents[arrayIndex] + eventIndex; \
	e->threadID = getThreadIDFast(); \
	e->name = name_; \
	e->type = type_; \
	QueryPerformanceCounter((LARGE_INTEGER*)&e->timestamp); \
	cpuProfileCompletelyWritten[arrayIndex].fetch_add(1, std::memory_order_release); // Mark this event as written. Release means that the compiler may not reorder the previous writes after this.


struct cpu_profile_block_recorder
{
	const char* name;

	cpu_profile_block_recorder(const char* name)
		: name(name)
	{
		recordProfileEvent(profile_event_begin_block, name);
	}

	~cpu_profile_block_recorder()
	{
		recordProfileEvent(profile_event_end_block, name);
	}
};

inline void cpuProfilingFrameEndMarker()
{
	recordProfileEvent(profile_event_frame_marker, 0);
}

inline void cpuProfilingStat(const char* format, ...)
{
	extern std::atomic<uint32> cpuProfileIndex;
	extern std::atomic<uint32> cpuProfileCompletelyWritten[2];
	extern char cpuProfileStats[2][MAX_NUM_CPU_PROFILE_STATS][MAX_CPU_PROFILE_STAT_LENGTH];

	uint32 arrayAndStatIndex = cpuProfileIndex.fetch_add(1 << 20);
	uint32 statIndex = _CPU_PROFILE_GET_STAT_INDEX(arrayAndStatIndex);
	uint32 arrayIndex = _CPU_PROFILE_GET_ARRAY_INDEX(arrayAndStatIndex);
	assert(statIndex < MAX_NUM_CPU_PROFILE_STATS);

	char* statBuffer = cpuProfileStats[arrayIndex][statIndex];
	va_list arg;
	va_start(arg, format);
	vsnprintf(statBuffer, MAX_CPU_PROFILE_STAT_LENGTH, format, arg);
	va_end(arg);

	cpuProfileCompletelyWritten[arrayIndex].fetch_add(1, std::memory_order_release); // Mark this stat as written. Release means that the compiler may not reorder the previous writes after this.
}

// Currently there must not be any profile events between calling cpuProfilingFrameEndMarker and cpuProfilingResolveTimeStamps.

void cpuProfilingResolveTimeStamps();

#undef recordProfileEvent

#else

#define CPU_PROFILE_BLOCK(...)
#define CPU_PROFILE_STAT(...)

#define cpuProfilingFrameEndMarker(...)
#define cpuProfilingResolveTimeStamps(...)

#endif

