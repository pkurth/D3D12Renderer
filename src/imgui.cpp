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
#include "dx_command_list.h"


static com<ID3D12DescriptorHeap> imguiDescriptorHeap;
static CD3DX12_CPU_DESCRIPTOR_HANDLE startCPUDescriptor;
static CD3DX12_GPU_DESCRIPTOR_HANDLE startGPUDescriptor;
static uint32 descriptorHandleIncrementSize;
static uint32 numImagesThisFrame;

#define MAX_NUM_IMGUI_IMAGES_PER_FRAME 16


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


	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = NUM_BUFFERED_FRAMES * MAX_NUM_IMGUI_IMAGES_PER_FRAME + 1;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	checkResult(dxContext.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&imguiDescriptorHeap)));

	startCPUDescriptor = imguiDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	startGPUDescriptor = imguiDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	descriptorHandleIncrementSize = dxContext.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	ImGui_ImplWin32_Init(win32_window::mainWindow->windowHandle);
	ImGui_ImplDX12_Init(dxContext.device.Get(), NUM_BUFFERED_FRAMES,
		screenFormat, imguiDescriptorHeap.Get(),
		startCPUDescriptor,
		startGPUDescriptor);

	return imguiContext;
}

void newImGuiFrame(float dt)
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void renderImGui(dx_command_list* cl)
{
	ImGui::Render();
	if (cl)
	{
		cl->setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, imguiDescriptorHeap);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cl->commandList.Get());
	}

	auto& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		if (cl)
		{
			ImGui::RenderPlatformWindowsDefault(0, cl->commandList.Get());
		}
	}

	numImagesThisFrame = 0;
}

LRESULT handleImGuiInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
}

namespace ImGui
{
	void Image(::dx_cpu_descriptor_handle& handle, ImVec2 size)
	{
		if (numImagesThisFrame < MAX_NUM_IMGUI_IMAGES_PER_FRAME)
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(startCPUDescriptor, 1 + dxContext.bufferedFrameID * MAX_NUM_IMGUI_IMAGES_PER_FRAME + numImagesThisFrame, descriptorHandleIncrementSize);
			CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(startGPUDescriptor, 1 + dxContext.bufferedFrameID * MAX_NUM_IMGUI_IMAGES_PER_FRAME + numImagesThisFrame, descriptorHandleIncrementSize);

			dxContext.device->CopyDescriptorsSimple(1, cpuHandle, handle.cpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			++numImagesThisFrame;

			ImGui::Image((ImTextureID)gpuHandle.ptr, size);
		}
	}

	void Image(::dx_cpu_descriptor_handle& handle, uint32 width, uint32 height)
	{
		ImGui::Image(handle, ImVec2((float)width, (float)height));
	}

	void Image(const ref<dx_texture>& texture, ImVec2 size)
	{
		ImGui::Image(texture->defaultSRV, size);
	}

	void Image(const ref<dx_texture>& texture, uint32 width, uint32 height)
	{
		ImGui::Image(texture->defaultSRV, width, height);
	}

	bool Dropdown(const char* label, const char** names, uint32 count, uint32& current)
	{
		bool changed = false;
		if (ImGui::BeginCombo(label, names[current]))
		{
			for (uint32 i = 0; i < count; ++i)
			{
				bool selected = i == current;
				if (ImGui::Selectable(names[i], selected))
				{
					current = i;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		return changed;
	}
}


