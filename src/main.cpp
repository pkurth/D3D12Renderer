#include "pch.h"
#include "dx_context.h"
#include "dx_window.h"
#include "dx_command_list.h"
#include "dx_renderer.h"
#include "input.h"
#include "imgui.h"
#include "file_browser.h"
#include "game.h"

#include <fontawesome/list.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui_internal.h>

static float perfFreq;
static LARGE_INTEGER lastTime;

static bool showIconsWindow;
static bool showDemoWindow;

bool handleWindowsMessages();

static bool newFrame(float& dt)
{
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);
	dt = ((float)(currentTime.QuadPart - lastTime.QuadPart) / perfFreq);
	lastTime = currentTime;

	bool result = handleWindowsMessages();

	newImGuiFrame(dt);
	ImGui::DockSpaceOverViewport();

	return result;
}

static uint64 renderToWindow(dx_window& window, float* clearColor)
{
	dx_resource backbuffer = window.backBuffers[window.currentBackbufferIndex];
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(window.rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), window.currentBackbufferIndex, window.rtvDescriptorSize);


	dx_command_list* cl = dxContext.getFreeRenderCommandList();

	CD3DX12_RECT scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);
	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.f, 0.f, (float)window.clientWidth, (float)window.clientHeight);

	cl->setScissor(scissorRect);
	cl->setViewport(viewport);

	cl->transitionBarrier(backbuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cl->clearRTV(rtv, clearColor);
	cl->setScreenRenderTarget(&rtv, 1, (window.depthFormat == DXGI_FORMAT_UNKNOWN) ? nullptr : &window.depthBuffer.dsvHandle.cpuHandle);

	if (win32_window::mainWindow == &window)
	{
		renderImGui(cl);
	}

	cl->transitionBarrier(backbuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	uint64 result = dxContext.executeCommandList(cl);

	window.swapBuffers();

	return result;
}

static void drawHelperWindows()
{
	if (showIconsWindow)
	{
		ImGui::Begin("Icons", &showIconsWindow);

		static ImGuiTextFilter filter;
		filter.Draw();
		for (uint32 i = 0; i < arraysize(awesomeIcons); ++i)
		{
			ImGui::PushID(i);
			if (filter.PassFilter(awesomeIconNames[i]))
			{
				ImGui::Text("%s: %s", awesomeIconNames[i], awesomeIcons[i]);
				ImGui::SameLine();
				if (ImGui::Button("Copy to clipboard"))
				{
					ImGui::SetClipboardText(awesomeIconNames[i]);
				}
			}
			ImGui::PopID();
		}
		ImGui::End();
	}

	if (showDemoWindow)
	{
		ImGui::ShowDemoWindow(&showDemoWindow);
	}
}

static bool drawMainMenuBar()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu(ICON_FA_FILE "  File"))
		{
			if (ImGui::MenuItem(ICON_FA_SAVE "  Save scene"))
			{

			}

			if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  Load scene"))
			{

			}

			ImGui::Separator();
			if (ImGui::MenuItem(ICON_FA_TIMES "  Exit"))
			{
				return false;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(ICON_FA_TOOLS "  Developer"))
		{
			if (ImGui::MenuItem(ICON_FA_ICONS "  Show available icons"))
			{
				showIconsWindow = true;
			}

			if (ImGui::MenuItem(ICON_FA_PUZZLE_PIECE "  Show demo window"))
			{
				showDemoWindow = true;
			}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	return true;
}

LONG NTAPI handleVectoredException(PEXCEPTION_POINTERS exceptionInfo)
{
	PEXCEPTION_RECORD exceptionRecord = exceptionInfo->ExceptionRecord;

	switch (exceptionRecord->ExceptionCode)
	{
	case DBG_PRINTEXCEPTION_WIDE_C:
	case DBG_PRINTEXCEPTION_C:

		if (exceptionRecord->NumberParameters >= 2)
		{
			ULONG len = (ULONG)exceptionRecord->ExceptionInformation[0];

			union
			{
				ULONG_PTR up;
				PCWSTR pwz;
				PCSTR psz;
			};

			up = exceptionRecord->ExceptionInformation[1];

			HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);

			if (exceptionRecord->ExceptionCode == DBG_PRINTEXCEPTION_C)
			{
				// Localized text will be incorrect displayed, if used not CP_OEMCP encoding.
				// WriteConsoleA(hOut, psz, len, &len, 0);

				// assume CP_ACP encoding
				if (ULONG n = MultiByteToWideChar(CP_ACP, 0, psz, len, 0, 0))
				{
					PWSTR wz = (PWSTR)alloca(n * sizeof(WCHAR));

					if (len = MultiByteToWideChar(CP_ACP, 0, psz, len, wz, n))
					{
						pwz = wz;
					}
				}
			}

			if (len)
			{
				WriteConsoleW(hOut, pwz, len - 1, &len, 0);
			}

		}
		return EXCEPTION_CONTINUE_EXECUTION;
	}

	return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char** argv)
{
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	AddVectoredExceptionHandler(TRUE, handleVectoredException);

	dxContext.initialize();

	const color_depth colorDepth = color_depth_8;
	DXGI_FORMAT screenFormat = (colorDepth == color_depth_8) ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R10G10B10A2_UNORM;

	dx_window window;
	window.initialize(TEXT("Main Window"), 1280, 800, colorDepth, DXGI_FORMAT_UNKNOWN, false);
	setMainWindow(&window);

	dx_renderer::initialize(1024, 1024);

	initializeImGui(screenFormat);


	game_scene scene;
	scene.initialize();



	LARGE_INTEGER perfFreqResult;
	QueryPerformanceFrequency(&perfFreqResult);
	perfFreq = (float)perfFreqResult.QuadPart;

	QueryPerformanceCounter(&lastTime);


	user_input input = {};
	float dt;


	uint64 fenceValues[NUM_BUFFERED_FRAMES] = {};

	fenceValues[NUM_BUFFERED_FRAMES - 1] = dxContext.renderQueue.signal();

	uint64 frameID = 0;

	ImGuiWindowClass sceneViewWindowClass;
	sceneViewWindowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_AutoHideTabBar;

	while (newFrame(dt))
	{
		dxContext.renderQueue.waitForFence(fenceValues[window.currentBackbufferIndex]);
		dxContext.newFrame(frameID);


		ImGui::SetNextWindowClass(&sceneViewWindowClass);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("Scene");
		uint32 renderWidth = (uint32)ImGui::GetContentRegionAvail().x;
		uint32 renderHeight = (uint32)ImGui::GetContentRegionAvail().y;
		ImGui::Image(dx_renderer::frameResult.defaultSRV, renderWidth, renderHeight);

		ImGuiIO& io = ImGui::GetIO();
		if (ImGui::IsItemHovered())
		{
			ImVec2 relativeMouse = ImGui::GetMousePos() - ImGui::GetItemRectMin();
			vec2 mousePos = { relativeMouse.x, relativeMouse.y };
			input.mouse.dx = mousePos.x - input.mouse.x;
			input.mouse.dy = mousePos.y - input.mouse.y;
			input.mouse.reldx = input.mouse.dx / (renderWidth - 1);
			input.mouse.reldy = input.mouse.dy / (renderHeight - 1);
			input.mouse.x = mousePos.x;
			input.mouse.y = mousePos.y;
			input.mouse.relX = mousePos.x / (renderWidth - 1);
			input.mouse.relY = mousePos.y / (renderHeight - 1);
			input.mouse.left = { ImGui::IsMouseDown(ImGuiMouseButton_Left), ImGui::IsMouseClicked(ImGuiMouseButton_Left), ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) };
			input.mouse.right = { ImGui::IsMouseDown(ImGuiMouseButton_Right), ImGui::IsMouseClicked(ImGuiMouseButton_Right), ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right) };
			input.mouse.middle = { ImGui::IsMouseDown(ImGuiMouseButton_Middle), ImGui::IsMouseClicked(ImGuiMouseButton_Middle), ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Middle) };
		
			for (uint32 i = 0; i < arraysize(user_input::keyboard); ++i)
			{
				input.keyboard[i] = { ImGui::IsKeyDown(i), ImGui::IsKeyPressed(i) };
			}

			input.overWindow = true;
		}
		else
		{
			input.mouse.dx = 0.f;
			input.mouse.dy = 0.f;
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
				if (input.keyboard[i].down && !ImGui::IsKeyDown(i)) { input.keyboard[i].down = false; }
				input.keyboard[i].pressEvent = false;
			}

			input.overWindow = false;
		}
		ImGui::End();
		ImGui::PopStyleVar();


		if (input.keyboard['V'].pressEvent) { window.toggleVSync(); }
		if (ImGui::IsKeyPressed(key_esc)) { break; } // Also allowed if not focused on main window.
		if (ImGui::IsKeyPressed(key_enter) && ImGui::IsKeyDown(key_alt)) { window.toggleFullscreen(); } // Also allowed if not focused on main window.


		drawHelperWindows();

		dx_renderer::beginFrame(renderWidth, renderHeight);
		scene.update(input, dt);
		dx_renderer::render(dt);

		if (!drawMainMenuBar())
		{
			break;
		}

		drawFileBrowser();


		float clearColor1[] = { 0.f, 0.f, 0.f, 1.f };
		fenceValues[window.currentBackbufferIndex] = renderToWindow(window, clearColor1);
		
		//float clearColor2[] = { 1.f, 1.f, 0.f, 1.f };
		//renderToWindow(window2, clearColor2);

		++frameID;
	}

	dxContext.quit();
}
