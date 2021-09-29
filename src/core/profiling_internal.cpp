#include "pch.h"
#define PROFILING_INTERNAL
#include "profiling_internal.h"
#include "core/imgui.h"


bool handleProfileEvent(profile_event* events, uint32 eventIndex, uint32 numEvents, uint16* stack, uint32& d, profile_block* blocks, uint32& numBlocksUsed, uint64& frameEndTimestamp)
{
	profile_event* e = events + eventIndex;

	bool result = false;

	switch (e->type)
	{
		case profile_event_begin_block:
		{
		rehandleEvent:
			uint64 timestamp = e->timestamp;


			// The end event of one block often has the exact same timestamp as the begin event of the next block(s).
			// When rendering multithreaded, the order of these events in the CPU-side array may not be correct.
			// We thus search for possible end blocks with the same timestamp here and - if we already saw a matching 
			// begin block earlier - process them first.

			if (d > 0)
			{
				for (uint32 j = eventIndex + 1; j < numEvents && events[j].timestamp == timestamp; ++j)
				{
					if (events[j].type == profile_event_end_block
						&& events[j].laneIndex == e->laneIndex 
						&& events[j].name == blocks[stack[d - 1]].name)
					{
						--d;
						profile_block* block = blocks + stack[d];
						block->endClock = timestamp;

						events[j].type = profile_event_none; // Mark as handled.
						goto rehandleEvent;
					}
				}
			}

			uint32 index = numBlocksUsed++;
			profile_block& block = blocks[index];

			block.startClock = timestamp;
			block.parent = (d == 0) ? INVALID_PROFILE_BLOCK : stack[d - 1];
			block.name = e->name;
			block.firstChild = INVALID_PROFILE_BLOCK;
			block.lastChild = INVALID_PROFILE_BLOCK;
			block.nextSibling = INVALID_PROFILE_BLOCK;

			if (block.parent != INVALID_PROFILE_BLOCK) // d > 0.
			{
				profile_block* parent = blocks + block.parent;

				if (parent->firstChild == INVALID_PROFILE_BLOCK)
				{
					parent->firstChild = index;
				}
				if (parent->lastChild != INVALID_PROFILE_BLOCK)
				{
					profile_block* lastChild = blocks + parent->lastChild;
					lastChild->nextSibling = index;
				}
				parent->lastChild = index;
			}
			else if (stack[d] != INVALID_PROFILE_BLOCK) // d is guaranteed to be 0 here. Therefore we can check for the validity of the stack element. This is always initialized.
			{
				profile_block* current = blocks + stack[d];
				current->nextSibling = index;
			}

			stack[d] = index;
			++d;
		} break;

		case profile_event_end_block:
		{
			--d;

			profile_block* block = blocks + stack[d];
			assert(block->name == e->name);

			block->endClock = e->timestamp;
		} break;

		case profile_event_frame_marker:
		{
			frameEndTimestamp = e->timestamp;
			result = true;
		} break;
	}

	return result;
}

static const ImColor highlightFrameColor = ImGui::green;


static const float timelineBottom = 400.f;
static const float rightPadding = 50.f;
static const float highlightTop = timelineBottom + 150.f;

static const ImVec4 colorTable[] =
{
	ImVec4(1.f, 0, 0, 1.f),
	ImVec4(0, 1.f, 0, 1.f),
	ImVec4(0, 0, 1.f, 1.f),

	ImVec4(1.f, 0, 1.f, 1.f),
	ImVec4(1.f, 1.f, 0, 1.f),
	ImVec4(0, 1.f, 1.f, 1.f),

	ImVec4(1.f, 0.5f, 0, 1.f),
	ImVec4(0, 1.f, 0.5f, 1.f),
	ImVec4(0.5f, 0, 1.f, 1.f),

	ImVec4(0.5f, 1.f, 0, 1.f),
	ImVec4(1.f, 0, 0.5f, 1.f),
	ImVec4(0, 0.5f, 1.f, 1.f),
};

void profiler_timeline::begin(uint32 numFrames)
{
	this->numFrames = numFrames;
	totalWidth = ImGui::GetContentRegionAvail().x - leftPadding - rightPadding;

	barHeight16ms = 100.f;
	barHeight33ms = barHeight16ms * 2.f;

	rightEdge = leftPadding + totalWidth;
	horizontalBarStride = totalWidth / numFrames;
	barWidth = horizontalBarStride/* * 0.9f*/;
}

void profiler_timeline::drawHeader(bool& pauseRecording)
{
	if (ImGui::Button(pauseRecording ? (ICON_FA_PLAY "  Resume recording") : (ICON_FA_PAUSE "  Pause recording")))
	{
		pauseRecording = !pauseRecording;
	}
	ImGui::SameLine();
	ImGui::Text("The last %u frames are recorded. Click on one frame to get a detailed hierarchical view of all blocks. Zoom into detail view with mouse wheel and click and drag to shift the display.", numFrames);
}

void profiler_timeline::drawOverviewFrame(profile_frame& frame, uint32 frameIndex, uint32& highlightFrameIndex, uint32 currentFrame)
{
	if (frame.duration > 0.f)
	{
		float left = leftPadding + frameIndex * horizontalBarStride;
		float height = frame.duration / (1000.f / 60.f) * barHeight16ms;
		float top = timelineBottom - height;

		ImGui::PushID(frameIndex);

		ImGui::SetCursorPos(ImVec2(left, top));

		ImColor color = (frameIndex == highlightFrameIndex) ? highlightFrameColor : ImGui::red;
		color = (frameIndex == currentFrame) ? ImGui::yellow : color;

		bool result = ImGui::ColorButton("", color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(barWidth, height));
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Frame %llu (%fms)", frame.globalFrameID, frame.duration);
		}

		if (result)
		{
			highlightFrameIndex = frameIndex;
		}

		ImGui::PopID();
	}
}

void profiler_timeline::endOverview()
{
	ImGui::SetCursorPos(ImVec2(leftPadding, timelineBottom - barHeight16ms - 1));
	ImGui::ColorButton("##60FPS", ImGui::white, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(totalWidth, 1));

	ImGui::SetCursorPos(ImVec2(leftPadding, timelineBottom - barHeight33ms - 1));
	ImGui::ColorButton("##30FPS", ImGui::white, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(totalWidth, 1));

	ImGui::SetCursorPos(ImVec2(rightEdge + 3.f, timelineBottom - barHeight16ms - 1));
	ImGui::Text("16.7ms");

	ImGui::SetCursorPos(ImVec2(rightEdge + 3.f, timelineBottom - barHeight33ms - 1));
	ImGui::Text("33.3ms");
}

void profiler_timeline::drawHighlightFrameInfo(profile_frame& frame)
{
	ImGui::SetCursorPos(ImVec2(leftPadding, highlightTop - 80.f));
	ImGui::Text("Frame %llu (%fms)", frame.globalFrameID, frame.duration);
}

void profiler_timeline::drawCallStack(profile_block* blocks, float frameWidthMultiplier, float callstackLeftPadding, uint16 startIndex)
{
	static const float barStride = 40.f;
	static const float barHeight = barStride * 0.8f;

	const float frameWidth16ms = totalWidth * frameWidthMultiplier;
	const float frameWidth33ms = frameWidth16ms * 2.f;

	// Call stack.
	uint16 currentIndex = startIndex;
	uint32 depth = 0;

	while (currentIndex != INVALID_PROFILE_BLOCK)
	{
		profile_block* current = blocks + currentIndex;

		// Draw.
		float top = highlightTop + depth * barStride;
		float left = callstackLeftPadding + current->relStart / (1000.f / 60.f) * frameWidth16ms;
		float width = current->duration / (1000.f / 60.f) * frameWidth16ms;

		ImGui::SetCursorPos(ImVec2(left, top));
		ImGui::ColorButton(current->name, colorTable[colorIndex++],
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder | ImGuiColorEditFlags_NoDragDrop,
			ImVec2(width, barHeight));

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s: %.3fms", current->name, current->duration);
		}

		ImGui::PushClipRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), true);
		ImGui::SetCursorPos(ImVec2(left + ImGui::GetStyle().FramePadding.x, top + ImGui::GetStyle().FramePadding.y));
		ImGui::Text("%s: %.3fms", current->name, current->duration);
		ImGui::PopClipRect();


		if (colorIndex >= arraysize(colorTable))
		{
			colorIndex = 0;
		}




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
			maxDepth = max(depth, maxDepth);
		}
		currentIndex = nextIndex;
	}

	assert(depth == 0);
}

void profiler_timeline::drawMillisecondSpacings(profile_frame& frame, float frameWidthMultiplier, float callstackLeftPadding)
{
	const float frameWidth16ms = totalWidth * frameWidthMultiplier;
	const float frameWidth33ms = frameWidth16ms * 2.f;
	const float barStride = 40.f;

	const float textSpacing = 30.f;
	const float lineHeight = (maxDepth + 1) * barStride + textSpacing;
	const float lineTop = highlightTop - textSpacing;

	// 0ms.
	ImGui::SetCursorPos(ImVec2(callstackLeftPadding, lineTop));
	ImGui::ColorButton("##0ms", ImGui::white, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(1, lineHeight));

	ImGui::SetCursorPos(ImVec2(callstackLeftPadding + 2, lineTop - 5));
	ImGui::Text("0ms");

	// 16ms.
	ImGui::SetCursorPos(ImVec2(callstackLeftPadding + frameWidth16ms, lineTop));
	ImGui::ColorButton("##16ms", ImGui::white, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(1, lineHeight));

	ImGui::SetCursorPos(ImVec2(callstackLeftPadding + frameWidth16ms + 2, lineTop - 5));
	ImGui::Text("16.7ms");

	// 33ms.
	ImGui::SetCursorPos(ImVec2(callstackLeftPadding + frameWidth33ms, lineTop));
	ImGui::ColorButton("##33ms", ImGui::white, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(1, lineHeight));

	ImGui::SetCursorPos(ImVec2(callstackLeftPadding + frameWidth33ms + 2, lineTop - 5));
	ImGui::Text("33.3ms");


	// 1ms spacings.
	const ImVec4 normalColor(0.2f, 0.2f, 0.2f, 1.f);
	const ImVec4 specialColor(0.5f, 0.5f, 0.5f, 1.f);

	const float millisecondSpacing = frameWidth33ms / (1000.f / 30.f);
	for (uint32 i = 1; i <= 33; ++i)
	{
		ImVec4 color = (i % 5 == 0) ? specialColor : normalColor;

		ImGui::SetCursorPos(ImVec2(callstackLeftPadding + i * millisecondSpacing, lineTop + 8));
		ImGui::ColorButton("", color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(1, lineHeight));
	}

	// Frame end.
	ImGui::SetCursorPos(ImVec2(callstackLeftPadding + frame.duration * millisecondSpacing, lineTop));
	ImGui::ColorButton("##Frame end", ImGui::blue, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(1, lineHeight));

	ImGui::SetCursorPos(ImVec2(callstackLeftPadding + frame.duration * millisecondSpacing + 2, lineTop - 5));
	ImGui::Text("Frame end");
}

void profiler_timeline::handleUserInteractions(float& frameWidthMultiplier, float& callstackLeftPadding, float& horizontalScrollAnchor, bool& horizontalScrolling)
{
	const float frameWidth16ms = totalWidth * frameWidthMultiplier;
	const float frameWidth33ms = frameWidth16ms * 2.f;

	const float barStride = 40.f;
	const float textSpacing = 30.f;
	const float lineHeight = (maxDepth + 1) * barStride + textSpacing;
	const float lineTop = highlightTop - textSpacing;



	// Invisible widget to block window dragging in this area.
	ImGui::SetCursorPos(ImVec2(leftPadding, highlightTop));
	ImGui::InvisibleButton("Blocker", ImVec2(totalWidth + rightPadding, (maxDepth + 1) * barStride));


	ImVec2 mousePos = ImGui::GetMousePos();
	ImVec2 windowPos = ImGui::GetWindowPos();

	float relMouseX = mousePos.x - windowPos.x;

	bool overStack = false;
	if (ImGui::IsMouseHoveringRect(ImVec2(leftPadding + windowPos.x, highlightTop + windowPos.y), ImVec2(leftPadding + totalWidth + rightPadding + windowPos.x, highlightTop + (maxDepth + 1) * barStride + windowPos.y), false))
	{
		overStack = true;

		// Hover time.
		float hoveredX = ImGui::GetMousePos().x - ImGui::GetWindowPos().x;
		float hoveredTime = (hoveredX - callstackLeftPadding) / frameWidth33ms * (1000.f / 30.f);
		ImGui::SetCursorPos(ImVec2(hoveredX, lineTop));
		ImGui::ColorButton("", ImGui::yellow, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(1, lineHeight));

		ImGui::SetCursorPos(ImVec2(hoveredX + 2, lineTop - 5));
		ImGui::Text("%.3fms", hoveredTime);
	}


	// Horizontal scrolling.

	if (!ImGui::IsMouseDown(ImGuiPopupFlags_MouseButtonLeft))
	{
		horizontalScrolling = false;
	}
	if (overStack && ImGui::IsMouseClicked(ImGuiPopupFlags_MouseButtonLeft))
	{
		horizontalScrollAnchor = relMouseX;
		horizontalScrolling = true;
	}
	if (horizontalScrolling)
	{
		callstackLeftPadding += relMouseX - horizontalScrollAnchor;
		horizontalScrollAnchor = relMouseX;
	}


	// Zooming.

	if (overStack)
	{
		float zoom = ImGui::GetIO().MouseWheel;

		if (zoom != 0.f)
		{
			float t = inverseLerp(callstackLeftPadding, callstackLeftPadding + frameWidth16ms, relMouseX);

			frameWidthMultiplier += zoom * 0.1f;
			frameWidthMultiplier = max(frameWidthMultiplier, 0.2f);

			float newFrameWidth16ms = totalWidth * frameWidthMultiplier;
			callstackLeftPadding = relMouseX - t * newFrameWidth16ms;
		}
	}
}
