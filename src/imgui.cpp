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
#include "dx_renderer.h"

#include <iostream>


ImGuiContext* initializeImGui(DXGI_FORMAT screenFormat)
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

	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.f;
		style.Colors[ImGuiCol_WindowBg].w = 1.f;
	}

	io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/fonts/opensans/OpenSans-Regular.ttf", 18.f);

	// Merge in icons.
	static const ImWchar iconsRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	ImFontConfig iconsConfig;
	iconsConfig.MergeMode = true; 
	iconsConfig.PixelSnapH = true;
	io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/fonts/icons/" FONT_ICON_FILE_NAME_FAS, 16.f, &iconsConfig, iconsRanges);


	auto& descriptorHeap = dx_renderer::globalDescriptorHeap;
	auto handle = descriptorHeap.pushNullTextureSRV();

	ImGui_ImplWin32_Init(win32_window::mainWindow->windowHandle);
	ImGui_ImplDX12_Init(dxContext.device.Get(), NUM_BUFFERED_FRAMES,
		screenFormat, descriptorHeap.descriptorHeap.Get(),
		handle.cpuHandle,
		handle.gpuHandle);

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
	cl->setDescriptorHeap(dx_renderer::globalDescriptorHeap);
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

namespace ImGui
{
	void Image(::dx_descriptor_handle& handle, ImVec2 size)
	{
		ImGui::Image((ImTextureID)handle.gpuHandle.ptr, size);
	}

	void Image(::dx_descriptor_handle& handle, uint32 width, uint32 height)
	{
		ImGui::Image((ImTextureID)handle.gpuHandle.ptr, ImVec2((float)width, (float)height));
	}
}


