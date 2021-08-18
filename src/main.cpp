#include "pch.h"
#include "dx/dx_context.h"
#include "window/dx_window.h"
#include "dx/dx_command_list.h"
#include "dx/dx_profiling.h"
#include "core/input.h"
#include "core/imgui.h"
#include "editor/file_browser.h"
#include "application.h"
#include "rendering/render_utils.h"
#include "rendering/main_renderer.h"
#include "editor/asset_editor_panel.h"


#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui_internal.h>


bool handleWindowsMessages();

static bool newFrame(float& dt)
{
	static bool first = true;
	static float perfFreq;
	static LARGE_INTEGER lastTime;

	if (first)
	{
		LARGE_INTEGER perfFreqResult;
		QueryPerformanceFrequency(&perfFreqResult);
		perfFreq = (float)perfFreqResult.QuadPart;

		QueryPerformanceCounter(&lastTime);

		first = false;
	}

	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);
	dt = ((float)(currentTime.QuadPart - lastTime.QuadPart) / perfFreq);
	lastTime = currentTime;

	bool result = handleWindowsMessages();

	newImGuiFrame(dt);
	ImGui::DockSpaceOverViewport();

	return result;
}

static uint64 renderToMainWindow(dx_window& window)
{
	dx_resource backbuffer = window.backBuffers[window.currentBackbufferIndex];
	dx_rtv_descriptor_handle rtv = window.backBufferRTVs[window.currentBackbufferIndex];

	dx_command_list* cl = dxContext.getFreeRenderCommandList();

	{
		DX_PROFILE_BLOCK(cl, "Blit to backbuffer");

		CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.f, 0.f, (float)window.clientWidth, (float)window.clientHeight);
		cl->setViewport(viewport);

		cl->transitionBarrier(backbuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);

		cl->clearRTV(rtv, 0.f, 0.f, 0.f);
		cl->setRenderTarget(&rtv, 1, 0);

		renderImGui(cl);

		cl->transitionBarrier(backbuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	}
	dxContext.endFrame(cl);

	uint64 result = dxContext.executeCommandList(cl);

	window.swapBuffers();

	return result;
}

int main(int argc, char** argv)
{
	if (!dxContext.initialize())
	{
		return EXIT_FAILURE;
	}

	initializeJobSystem();

	const color_depth colorDepth = color_depth_8;

	dx_window window;
	window.initialize(TEXT("Main Window"), 1280, 800, colorDepth);
	setMainWindow(&window);
	//window.toggleVSync();

	application app = {};
	app.loadCustomShaders();

	window.setFileDropCallback([&app](const std::string& s) { app.handleFileDrop(s); });

	initializeTransformationGizmos();
	initializeRenderUtils();

	initializeImGui(window);

	renderer_spec spec = { true, true, true, true };

	main_renderer renderer;
	renderer.initialize(colorDepth, 1280, 800, spec);

	app.initialize(&renderer);

	file_browser fileBrowser;
	mesh_editor_panel meshEditor;

	user_input input = {};

	uint64 fenceValues[NUM_BUFFERED_FRAMES] = {};
	fenceValues[NUM_BUFFERED_FRAMES - 1] = dxContext.renderQueue.signal();

	uint64 frameID = 0;

	bool appFocusedLastFrame = true;

	dxContext.flushApplication();


	float dt;
	while (newFrame(dt))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::BeginWindowHiddenTabBar("Scene Viewport");
		uint32 renderWidth = (uint32)ImGui::GetContentRegionAvail().x;
		uint32 renderHeight = (uint32)ImGui::GetContentRegionAvail().y;
		ImGui::Image(renderer.frameResult, renderWidth, renderHeight);

		ImGuiIO& io = ImGui::GetIO();
		if (ImGui::IsItemHovered())
		{
			ImVec2 relativeMouse = ImGui::GetMousePos() - ImGui::GetItemRectMin();
			vec2 mousePos = { relativeMouse.x, relativeMouse.y };
			if (appFocusedLastFrame)
			{
				input.mouse.dx = (int32)(mousePos.x - input.mouse.x);
				input.mouse.dy = (int32)(mousePos.y - input.mouse.y);
				input.mouse.reldx = (float)input.mouse.dx / (renderWidth - 1);
				input.mouse.reldy = (float)input.mouse.dy / (renderHeight - 1);
			}
			else
			{
				input.mouse.dx = 0;
				input.mouse.dy = 0;
				input.mouse.reldx = 0.f;
				input.mouse.reldy = 0.f;
			}
			input.mouse.x = (int32)mousePos.x;
			input.mouse.y = (int32)mousePos.y;
			input.mouse.relX = mousePos.x / (renderWidth - 1);
			input.mouse.relY = mousePos.y / (renderHeight - 1);
			input.mouse.left = { ImGui::IsMouseDown(ImGuiMouseButton_Left), ImGui::IsMouseClicked(ImGuiMouseButton_Left), ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) };
			input.mouse.right = { ImGui::IsMouseDown(ImGuiMouseButton_Right), ImGui::IsMouseClicked(ImGuiMouseButton_Right), ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right) };
			input.mouse.middle = { ImGui::IsMouseDown(ImGuiMouseButton_Middle), ImGui::IsMouseClicked(ImGuiMouseButton_Middle), ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Middle) };

			for (uint32 i = 0; i < arraysize(user_input::keyboard); ++i)
			{
				input.keyboard[i] = { ImGui::IsKeyDown(i), ImGui::IsKeyPressed(i, false) };
			}

			input.overWindow = true;
		}
		else
		{
			input.mouse.dx = 0;
			input.mouse.dy = 0;
			input.mouse.reldx = 0.f;
			input.mouse.reldy = 0.f;

			if (input.mouse.left.down && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) { input.mouse.left.down = false; }
			if (input.mouse.right.down && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) { input.mouse.right.down = false; }
			if (input.mouse.middle.down && !ImGui::IsMouseDown(ImGuiMouseButton_Middle)) { input.mouse.middle.down = false; }

			input.mouse.left.clickEvent = input.mouse.left.doubleClickEvent = false;
			input.mouse.right.clickEvent = input.mouse.right.doubleClickEvent = false;
			input.mouse.middle.clickEvent = input.mouse.middle.doubleClickEvent = false;

			for (uint32 i = 0; i < arraysize(user_input::keyboard); ++i)
			{
				if (!ImGui::IsKeyDown(i)) { input.keyboard[i].down = false; }
				input.keyboard[i].pressEvent = false;
			}

			input.overWindow = false;
		}


		// The drag&drop outline is rendered around the drop target. Since the image fills the frame, the outline is outside the window 
		// and thus invisible. So instead this Dummy acts as the drop target.
		// Important: This is under the input processing, so that we don't override the current element id.
		ImGui::SetCursorPos(ImVec2(4.5f, 4.5f));
		ImGui::Dummy(ImVec2(renderWidth - 9.f, renderHeight - 9.f));

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("content_browser_file"))
			{
				std::string str = (const char*)payload->Data;
				app.handleFileDrop(str);
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::End();
		ImGui::PopStyleVar();

		appFocusedLastFrame = ImGui::IsMousePosValid();

		if (input.keyboard['V'].pressEvent && !(input.keyboard[key_ctrl].down || input.keyboard[key_shift].down || input.keyboard[key_alt].down)) { window.toggleVSync(); }
		if (ImGui::IsKeyPressed(key_esc)) { break; } // Also allowed if not focused on main window.
		if (ImGui::IsKeyPressed(key_enter) && ImGui::IsKeyDown(key_alt)) { window.toggleFullscreen(); } // Also allowed if not focused on main window.



		dxContext.renderQueue.waitForFence(fenceValues[window.currentBackbufferIndex]);
		dxContext.newFrame(frameID);

		renderer.beginFrame(renderWidth, renderHeight);
		
		app.update(input, dt);

		endFrameCommon();
		renderer.endFrame(input);

		fileBrowser.draw(meshEditor);
		meshEditor.draw();

		fenceValues[window.currentBackbufferIndex] = renderToMainWindow(window);


		++frameID;
	}

	dxContext.flushApplication();

	dxContext.quit();

	return EXIT_SUCCESS;
}
