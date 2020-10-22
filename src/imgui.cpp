#include "pch.h"

#include <imgui/imgui_draw.cpp>
#include <imgui/imgui.cpp>
#include <imgui/imgui_widgets.cpp>
#include <imgui/imgui_demo.cpp>

#include <imgui/backends/imgui_impl_win32.cpp>
#include <imgui/backends/imgui_impl_dx12.cpp>

#include "imgui.h"
#include "window.h"
#include "dx_context.h"
#include "dx_render_primitives.h"
#include "dx_command_list.h"

#include <iostream>


static dx_cbv_srv_uav_descriptor_heap descriptorHeap;

ImGuiContext* initializeImGui(D3D12_RT_FORMAT_ARRAY screenRTFormats)
{
	IMGUI_CHECKVERSION();
	auto imguiContext = ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
	//io.ConfigViewportsNoAutoMerge = true;
	//io.ConfigViewportsNoTaskBarIcon = true;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.f;
		style.Colors[ImGuiCol_WindowBg].w = 1.f;
	}


	descriptorHeap = createDescriptorHeap(&dxContext, 1);

	ImGui_ImplWin32_Init(win32_window::mainWindow->windowHandle);
	ImGui_ImplDX12_Init(dxContext.device.Get(), NUM_BUFFERED_FRAMES,
		screenRTFormats.RTFormats[0], descriptorHeap.descriptorHeap.Get(),
		descriptorHeap.base.cpuHandle,
		descriptorHeap.base.gpuHandle);

	return imguiContext;
}

void newImGuiFrame(const user_input& input, float dt)
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void renderImGui(dx_command_list* cl)
{
	ImGui::Render();
	cl->setDescriptorHeap(descriptorHeap);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cl->commandList.Get());

	auto& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault(NULL, (void*)cl->commandList.Get());
	}
}

void handleImGuiInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
}




