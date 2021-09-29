#pragma once

#define COMPOSITE_VARNAME_(a, b) a##b
#define COMPOSITE_VARNAME(a, b) COMPOSITE_VARNAME_(a, b)


enum profile_event_type : uint16
{
	profile_event_frame_marker,
	profile_event_begin_block,
	profile_event_end_block,

	profile_event_none,
};

struct profile_event
{
	profile_event_type type;
	uint16 clType; // For gpu profiler.
	uint32 threadID;
	const char* name;
	uint64 timestamp;
};


#ifdef PROFILING_INTERNAL

#define INVALID_PROFILE_BLOCK 0xFFFF

struct profile_block
{
	uint16 firstChild;
	uint16 lastChild;
	uint16 nextSibling;
	uint16 parent;

	uint64 startClock;
	uint64 endClock;

	float relStart;
	float duration;

	uint32 threadID;

	const char* name;
};

struct profile_frame
{
	uint64 startClock;
	uint64 endClock;
	uint64 globalFrameID;

	float duration;
};

static const float leftPadding = 5.f;

struct profiler_timeline
{
	uint32 numFrames;
	float totalWidth;

	float barHeight16ms;
	float barHeight33ms;

	float rightEdge;
	float horizontalBarStride;
	float barWidth;


	uint32 colorIndex = 0;
	uint32 maxDepth = 0;



	void begin(uint32 numFrames);
	void drawHeader(bool& pauseRecording);
	void drawOverviewFrame(profile_frame& frame, uint32 frameIndex, uint32& highlightFrameIndex, uint32 currentFrame);
	void endOverview();

	void drawHighlightFrameInfo(profile_frame& frame);
	void drawCallStack(profile_block* blocks, float frameWidthMultiplier, float callstackLeftPadding, uint16 startIndex);
	void drawMillisecondSpacings(profile_frame& frame, float frameWidthMultiplier, float callstackLeftPadding);
	void handleUserInteractions(float& frameWidthMultiplier, float& callstackLeftPadding, float& horizontalScrollAnchor, bool& horizontalScrolling);
};

// Returns true if frame-end marker is found.
bool handleProfileEvent(profile_event* events, uint32 eventIndex, uint32 numEvents, uint16* stack, uint32& d, profile_block* blocks, uint32& numBlocksUsed, uint64& frameEndTimestamp);


#endif
