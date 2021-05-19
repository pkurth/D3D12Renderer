#include "pch.h"
#include "dx_profiling.h"
#include "imgui.h"
#include "math.h"


bool profilerWindowOpen = false;

#if ENABLE_DX_PROFILING

#include <algorithm>
#include <fontawesome/IconsFontAwesome5.h>

dx_profile_event profileEvents[NUM_BUFFERED_FRAMES][MAX_NUM_DX_PROFILE_EVENTS];

struct dx_profile_block
{
	dx_profile_block* firstChild;
	dx_profile_block* lastChild;
	dx_profile_block* nextSibling;
	dx_profile_block* parent;

	uint64 startClock;
	uint64 endClock;

	float relStart;
	float duration;

	const char* name;
};

struct dx_profile_frame
{
	dx_profile_block blocks[profile_cl_count][MAX_NUM_DX_PROFILE_BLOCKS];

	uint64 startClock;
	uint64 endClock;
	uint64 globalFrameID;

	float duration;

	uint32 count[profile_cl_count];
};

static dx_profile_frame profileFrames[MAX_NUM_DX_PROFILE_FRAMES];
static uint32 profileFrameWriteIndex;
static bool pauseRecording;

void profileFrameMarker(dx_command_list* cl)
{
	assert(cl->type == D3D12_COMMAND_LIST_TYPE_DIRECT);

	uint32 queryIndex = atomicIncrement(dxContext.timestampQueryIndex[dxContext.bufferedFrameID]);
	cl->queryTimestamp(queryIndex);

	profileEvents[dxContext.bufferedFrameID][queryIndex] = { profile_event_frame_marker, profile_cl_graphics, "Frame End" };
}

void resolveTimeStampQueries(uint64* timestamps)
{
	uint32 currentFrame = profileFrameWriteIndex;

	if (!pauseRecording)
	{
		uint32 numQueries = dxContext.timestampQueryIndex[dxContext.bufferedFrameID];
		if (numQueries == 0)
		{
			return;
		}

		dx_profile_event* events = profileEvents[dxContext.bufferedFrameID];
		for (uint32 i = 0; i < numQueries; ++i)
		{
			events[i].timestamp = timestamps[i];
		}

		// Stable sort is important here, since occasionally two time stamps will have the exact same value.
		std::stable_sort(events, events + numQueries, [](const dx_profile_event& a, const dx_profile_event& b)
		{
			return a.timestamp < b.timestamp;
		});




		dx_profile_block* stack[profile_cl_count][1024];
		uint32 depth[profile_cl_count] = { };
		uint32 count[profile_cl_count] = { };

		for (uint32 cl = 0; cl < profile_cl_count; ++cl)
		{
			stack[cl][0] = 0;
		}


		dx_profile_frame& frame = profileFrames[profileFrameWriteIndex];

		uint64 frameEndTimestamp = 0;

		for (uint32 i = 0; i < numQueries; ++i)
		{
			dx_profile_event* e = events + i;
			profile_cl_type clType = e->clType;
			uint32& d = depth[clType];

			switch (e->type)
			{
				case profile_event_begin_block:
				{
					uint32 index = count[clType]++;
					dx_profile_block& block = frame.blocks[clType][index];

					block.startClock = e->timestamp;
					block.parent = (d == 0) ? 0 : stack[clType][d - 1];
					block.name = e->name;
					block.firstChild = 0;
					block.lastChild = 0;
					block.nextSibling = 0;

					if (block.parent)
					{
						if (!block.parent->firstChild)
						{
							block.parent->firstChild = &block;
						}
						if (block.parent->lastChild)
						{
							block.parent->lastChild->nextSibling = &block;
						}
						block.parent->lastChild = &block;
					}
					else if (stack[clType][d])
					{
						stack[clType][d]->nextSibling = &block;
					}

					stack[clType][d] = &block;
					++d;
				} break;

				case profile_event_end_block:
				{
					--d;

					dx_profile_block* block = stack[clType][d];
					assert(block->name == e->name);

					block->endClock = e->timestamp;
				} break;

				case profile_event_frame_marker:
				{
					frameEndTimestamp = e->timestamp;
				} break;
			}
		}

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
				dx_profile_block& block = frame.blocks[cl][i];
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


	static uint32 highlightFrameIndex = -1;


	if (profilerWindowOpen)
	{
		if (ImGui::Begin("Profiling"), &profilerWindowOpen)
		{
			// Timeline.

			if (ImGui::Button(pauseRecording ? (ICON_FA_PLAY "  Resume recording") : (ICON_FA_PAUSE "  Pause recording")))
			{
				pauseRecording = !pauseRecording;
			}
			ImGui::SameLine();
			ImGui::Text("The last %u frames are recorded. Click on one frame to get a detailed hierarchical view of all blocks. Zoom into detail view with mouse wheel and click and drag to shift the display.", MAX_NUM_DX_PROFILE_FRAMES);

			const float timelineBottom = 400.f;
			const float leftPadding = 5.f;
			const float rightPadding = 50.f;
			const float totalWidth = ImGui::GetContentRegionAvail().x - leftPadding - rightPadding;

			static const ImColor white(1.f, 1.f, 1.f, 1.f);
			static const ImColor yellow(1.f, 1.f, 0.f, 1.f);
			static const ImColor green(0.f, 1.f, 0.f, 1.f);
			static const ImColor red(1.f, 0.f, 0.f, 1.f);
			static const ImColor blue(0.f, 0.f, 1.f, 1.f);
			static const ImColor highlightFrameColor = green;

			{
				const float barHeight16ms = 100.f;
				const float barHeight33ms = barHeight16ms * 2.f;

				const float rightEdge = leftPadding + totalWidth;
				const float barStride = totalWidth / MAX_NUM_DX_PROFILE_FRAMES;
				const float barWidth = barStride/* * 0.9f*/;


				for (uint32 frameIndex = 0; frameIndex < MAX_NUM_DX_PROFILE_FRAMES; ++frameIndex)
				{
					dx_profile_frame& frame = profileFrames[frameIndex];

					if (frame.duration > 0.f)
					{
						float left = leftPadding + frameIndex * barStride;
						float height = frame.duration / (1000.f / 60.f) * barHeight16ms;
						float top = timelineBottom - height;

						ImGui::PushID(frameIndex);

						ImGui::SetCursorPos(ImVec2(left, top));

						ImColor color = (frameIndex == highlightFrameIndex) ? highlightFrameColor : red;
						color = (frameIndex == currentFrame) ? yellow : color;

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

				ImGui::SetCursorPos(ImVec2(leftPadding, timelineBottom - barHeight16ms - 1));
				ImGui::ColorButton("##60FPS", white, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(totalWidth, 1));

				ImGui::SetCursorPos(ImVec2(leftPadding, timelineBottom - barHeight33ms - 1));
				ImGui::ColorButton("##30FPS", white, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(totalWidth, 1));

				ImGui::SetCursorPos(ImVec2(rightEdge + 3.f, timelineBottom - barHeight16ms - 1));
				ImGui::Text("16.7ms");

				ImGui::SetCursorPos(ImVec2(rightEdge + 3.f, timelineBottom - barHeight33ms - 1));
				ImGui::Text("33.3ms");
			}

			if (highlightFrameIndex != -1)
			{
				const float highlightTop = timelineBottom + 150.f;
				const float barStride = 40.f;
				const float barHeight = barStride * 0.8f;
				static float frameWidthMultiplier = 1.f;
				const float frameWidth16ms = totalWidth * frameWidthMultiplier;
				const float frameWidth33ms = frameWidth16ms * 2.f;

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

				uint32 colorIndex = 0;

				dx_profile_frame& frame = profileFrames[highlightFrameIndex];

				ImGui::SetCursorPos(ImVec2(leftPadding, highlightTop - 80.f));
				ImGui::Text("Frame %llu (%fms)", frame.globalFrameID, frame.duration);


				static float callstackLeftPadding = leftPadding;

				dx_profile_block* blocks = frame.blocks[profile_cl_graphics];
				uint32 count = frame.count[profile_cl_graphics];


				if (count > 0)
				{
					// Call stack.
					dx_profile_block* current = blocks;
					uint32 depth = 0;

					uint32 maxDepth = 0;

					ImVec2 mousePos = ImGui::GetMousePos();
					ImVec2 windowPos = ImGui::GetWindowPos();

					while (current)
					{
						// Draw.
						float top = highlightTop + depth * barStride;
						float left = callstackLeftPadding + current->relStart / (1000.f / 60.f) * frameWidth16ms;
						float width = current->duration / (1000.f / 60.f) * frameWidth16ms;

						ImGui::SetCursorPos(ImVec2(left, top));
						ImGui::ColorButton(current->name, colorTable[colorIndex++], ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(width, barHeight));

						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip("%s: %.3fms", current->name, current->duration);
						}

						ImGui::PushClipRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), false);
						ImGui::SetCursorPos(ImVec2(left + ImGui::GetStyle().FramePadding.x, top + ImGui::GetStyle().FramePadding.y));
						ImGui::Text("%s: %.3fms", current->name, current->duration);
						ImGui::PopClipRect();


						if (colorIndex >= arraysize(colorTable))
						{
							colorIndex = 0;
						}


						// Advance.
						dx_profile_block* next = current->firstChild;
						if (!next)
						{
							next = current->nextSibling;

							if (!next)
							{
								dx_profile_block* nextAncestor = current->parent;
								while (nextAncestor)
								{
									--depth;
									if (nextAncestor->nextSibling)
									{
										next = nextAncestor->nextSibling;
										break;
									}
									nextAncestor = nextAncestor->parent;
								}
							}
						}
						else
						{
							++depth;
							maxDepth = max(depth, maxDepth);
						}
						current = next;
					}

					assert(depth == 0);


					// Millisecond spacings.

					const float textSpacing = 30.f;
					const float lineHeight = (maxDepth + 1) * barStride + textSpacing;
					const float lineTop = highlightTop - textSpacing;

					// 0ms.
					ImGui::SetCursorPos(ImVec2(callstackLeftPadding, lineTop));
					ImGui::ColorButton("##0ms", white, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(1, lineHeight));

					ImGui::SetCursorPos(ImVec2(callstackLeftPadding + 2, lineTop - 5));
					ImGui::Text("0ms");

					// 16ms.
					ImGui::SetCursorPos(ImVec2(callstackLeftPadding + frameWidth16ms, lineTop));
					ImGui::ColorButton("##16ms", white, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(1, lineHeight));

					ImGui::SetCursorPos(ImVec2(callstackLeftPadding + frameWidth16ms + 2, lineTop - 5));
					ImGui::Text("16.7ms");

					// 33ms.
					ImGui::SetCursorPos(ImVec2(callstackLeftPadding + frameWidth33ms, lineTop));
					ImGui::ColorButton("##33ms", white, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(1, lineHeight));

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
					ImGui::ColorButton("##Frame end", blue, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(1, lineHeight));

					ImGui::SetCursorPos(ImVec2(callstackLeftPadding + frame.duration * millisecondSpacing + 2, lineTop - 5));
					ImGui::Text("Frame end");



					// Invisible widget to block window dragging in this area.
					ImGui::SetCursorPos(ImVec2(leftPadding, highlightTop));
					ImGui::InvisibleButton("Blocker", ImVec2(totalWidth + rightPadding, (maxDepth + 1) * barStride));


					float relMouseX = mousePos.x - windowPos.x;

					bool overStack = false;
					if (ImGui::IsMouseHoveringRect(ImVec2(leftPadding + windowPos.x, highlightTop + windowPos.y), ImVec2(leftPadding + totalWidth + rightPadding + windowPos.x, highlightTop + (maxDepth + 1) * barStride + windowPos.y), false))
					{
						overStack = true;

						// Hover time.
						float hoveredX = ImGui::GetMousePos().x - ImGui::GetWindowPos().x;
						float hoveredTime = (hoveredX - callstackLeftPadding) / frameWidth33ms * (1000.f / 30.f);
						ImGui::SetCursorPos(ImVec2(hoveredX, lineTop));
						ImGui::ColorButton("", yellow, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(1, lineHeight));

						ImGui::SetCursorPos(ImVec2(hoveredX + 2, lineTop - 5));
						ImGui::Text("%.3fms", hoveredTime);
					}


					// Horizontal scrolling.

					static float horizontalScrollAnchor = 0;
					static bool horizontalScrolling = false;

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
			}

		}
		ImGui::End();
	}
}

#endif
