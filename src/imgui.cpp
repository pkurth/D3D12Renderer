#include "pch.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui_draw.cpp>
#include <imgui/imgui.cpp>
#include <imgui/imgui_widgets.cpp>
#include <imgui/imgui_demo.cpp>
#include <imgui/imgui_tables.cpp>
#include <imgui/imgui_internal.h>

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


static void setStyle()
{
	auto& colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_WindowBg] = ImVec4{ 0.1f, 0.105f, 0.11f, 1.0f };

	// Headers
	colors[ImGuiCol_Header] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
	colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
	colors[ImGuiCol_HeaderActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

	// Buttons
	colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
	colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
	colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

	// Frame BG
	colors[ImGuiCol_FrameBg] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
	colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
	colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

	// Tabs
	colors[ImGuiCol_Tab] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
	colors[ImGuiCol_TabHovered] = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
	colors[ImGuiCol_TabActive] = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
	colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };

	// Title
	colors[ImGuiCol_TitleBg] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
	colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
}


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

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.f;
		style.Colors[ImGuiCol_WindowBg].w = 1.f;
	}

	setStyle();

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

	bool Dropdown(const char* label, const char* (*name_func)(uint32, void*), uint32& current, void* data)
	{
		bool changed = false;
		if (ImGui::BeginCombo(label, name_func(current, data)))
		{
			for (uint32 i = 0; ; ++i)
			{
				bool selected = i == current;
				const char* name = name_func(i, data);
				if (!name)
				{
					break;
				}
				if (ImGui::Selectable(name, selected))
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



// Curve editing from https://gist.githubusercontent.com/r-lyeh-archived/40d4fd0ea157ab3a58a4/raw/b80af5cc39438aa40c0170eb2b0111faf4971fc6/curve.hpp.

namespace tween {
	enum TYPE
	{
		LINEAR,

		QUADIN,          // t^2
		QUADOUT,
		QUADINOUT,
		CUBICIN,         // t^3
		CUBICOUT,
		CUBICINOUT,
		QUARTIN,         // t^4
		QUARTOUT,
		QUARTINOUT,
		QUINTIN,         // t^5
		QUINTOUT,
		QUINTINOUT,
		SINEIN,          // sin(t)
		SINEOUT,
		SINEINOUT,
		EXPOIN,          // 2^t
		EXPOOUT,
		EXPOINOUT,
		CIRCIN,          // sqrt(1-t^2)
		CIRCOUT,
		CIRCINOUT,
		ELASTICIN,       // exponentially decaying sine wave
		ELASTICOUT,
		ELASTICINOUT,
		BACKIN,          // overshooting cubic easing: (s+1)*t^3 - s*t^2
		BACKOUT,
		BACKINOUT,
		BOUNCEIN,        // exponentially decaying parabolic bounce
		BOUNCEOUT,
		BOUNCEINOUT,

		SINESQUARE,      // gapjumper's
		EXPONENTIAL,     // gapjumper's
		SCHUBRING1,      // terry schubring's formula 1
		SCHUBRING2,      // terry schubring's formula 2
		SCHUBRING3,      // terry schubring's formula 3

		SINPI2,          // tomas cepeda's
		SWING,           // tomas cepeda's & lquery's
	};

	static float ease(int easetype, float t)
	{
		using namespace std;

		const float d = 1.f;
		const float pi = M_PI;
		const float pi2 = M_PI / 2;

		float p = t / d;

		switch (easetype)
		{
			// Modeled after the line y = x
			default:
			case TYPE::LINEAR: {
				return p;
			}

							 // Modeled after the parabola y = x^2
			case TYPE::QUADIN: {
				return p * p;
			}

							 // Modeled after the parabola y = -x^2 + 2x
			case TYPE::QUADOUT: {
				return -(p * (p - 2));
			}

							  // Modeled after the piecewise quadratic
							  // y = (1/2)((2x)^2)             ; [0, 0.5)
							  // y = -(1/2)((2x-1)*(2x-3) - 1) ; [0.5, 1]
			case TYPE::QUADINOUT: {
				if (p < 0.5f) {
					return 2 * p * p;
				}
				else {
					return (-2 * p * p) + (4 * p) - 1;
				}
			}

								// Modeled after the cubic y = x^3
			case TYPE::CUBICIN: {
				return p * p * p;
			}

							  // Modeled after the cubic y = (x - 1)^3 + 1
			case TYPE::CUBICOUT: {
				float f = (p - 1);
				return f * f * f + 1;
			}

							   // Modeled after the piecewise cubic
							   // y = (1/2)((2x)^3)       ; [0, 0.5)
							   // y = (1/2)((2x-2)^3 + 2) ; [0.5, 1]
			case TYPE::CUBICINOUT: {
				if (p < 0.5f) {
					return 4 * p * p * p;
				}
				else {
					float f = ((2 * p) - 2);
					return 0.5f * f * f * f + 1;
				}
			}

								 // Modeled after the quartic x^4
			case TYPE::QUARTIN: {
				return p * p * p * p;
			}

							  // Modeled after the quartic y = 1 - (x - 1)^4
			case TYPE::QUARTOUT: {
				float f = (p - 1);
				return f * f * f * (1 - p) + 1;
			}

							   // Modeled after the piecewise quartic
							   // y = (1/2)((2x)^4)        ; [0, 0.5)
							   // y = -(1/2)((2x-2)^4 - 2) ; [0.5, 1]
			case TYPE::QUARTINOUT: {
				if (p < 0.5) {
					return 8 * p * p * p * p;
				}
				else {
					float f = (p - 1);
					return -8 * f * f * f * f + 1;
				}
			}

								 // Modeled after the quintic y = x^5
			case TYPE::QUINTIN: {
				return p * p * p * p * p;
			}

							  // Modeled after the quintic y = (x - 1)^5 + 1
			case TYPE::QUINTOUT: {
				float f = (p - 1);
				return f * f * f * f * f + 1;
			}

							   // Modeled after the piecewise quintic
							   // y = (1/2)((2x)^5)       ; [0, 0.5)
							   // y = (1/2)((2x-2)^5 + 2) ; [0.5, 1]
			case TYPE::QUINTINOUT: {
				if (p < 0.5) {
					return 16 * p * p * p * p * p;
				}
				else {
					float f = ((2 * p) - 2);
					return  0.5f * f * f * f * f * f + 1;
				}
			}

								 // Modeled after quarter-cycle of sine wave
			case TYPE::SINEIN: {
				return sin((p - 1) * pi2) + 1;
			}

							 // Modeled after quarter-cycle of sine wave (different phase)
			case TYPE::SINEOUT: {
				return sin(p * pi2);
			}

							  // Modeled after half sine wave
			case TYPE::SINEINOUT: {
				return 0.5f * (1 - cos(p * pi));
			}

								// Modeled after shifted quadrant IV of unit circle
			case TYPE::CIRCIN: {
				return 1 - sqrt(1 - (p * p));
			}

							 // Modeled after shifted quadrant II of unit circle
			case TYPE::CIRCOUT: {
				return sqrt((2 - p) * p);
			}

							  // Modeled after the piecewise circular function
							  // y = (1/2)(1 - sqrt(1 - 4x^2))           ; [0, 0.5)
							  // y = (1/2)(sqrt(-(2x - 3)*(2x - 1)) + 1) ; [0.5, 1]
			case TYPE::CIRCINOUT: {
				if (p < 0.5f) {
					return 0.5f * (1 - sqrt(1 - 4 * (p * p)));
				}
				else {
					return 0.5f * (sqrt(-((2 * p) - 3) * ((2 * p) - 1)) + 1);
				}
			}

								// Modeled after the exponential function y = 2^(10(x - 1))
			case TYPE::EXPOIN: {
				return (p == 0.f) ? p : powf(2, 10 * (p - 1));
			}

							 // Modeled after the exponential function y = -2^(-10x) + 1
			case TYPE::EXPOOUT: {
				return (p == 1.f) ? p : 1 - powf(2, -10 * p);
			}

							  // Modeled after the piecewise exponential
							  // y = (1/2)2^(10(2x - 1))         ; [0,0.5)
							  // y = -(1/2)*2^(-10(2x - 1))) + 1 ; [0.5,1]
			case TYPE::EXPOINOUT: {
				if (p == 0.f || p == 1.f) return p;

				if (p < 0.5f) {
					return 0.5f * powf(2, (20 * p) - 10);
				}
				else {
					return -0.5f * powf(2, (-20 * p) + 10) + 1;
				}
			}

								// Modeled after the damped sine wave y = sin(13pi/2*x)*powf(2, 10 * (x - 1))
			case TYPE::ELASTICIN: {
				return sin(13 * pi2 * p) * powf(2, 10 * (p - 1));
			}

								// Modeled after the damped sine wave y = sin(-13pi/2*(x + 1))*powf(2, -10x) + 1
			case TYPE::ELASTICOUT: {
				return sin(-13 * pi2 * (p + 1)) * powf(2, -10 * p) + 1;
			}

								 // Modeled after the piecewise exponentially-damped sine wave:
								 // y = (1/2)*sin(13pi/2*(2*x))*powf(2, 10 * ((2*x) - 1))      ; [0,0.5)
								 // y = (1/2)*(sin(-13pi/2*((2x-1)+1))*powf(2,-10(2*x-1)) + 2) ; [0.5, 1]
			case TYPE::ELASTICINOUT: {
				if (p < 0.5) {
					return 0.5f * sin(13 * pi2 * (2 * p)) * powf(2, 10 * ((2 * p) - 1));
				}
				else {
					return 0.5f * (sin(-13 * pi2 * ((2 * p - 1) + 1)) * powf(2, -10 * (2 * p - 1)) + 2);
				}
			}

								   // Modeled (originally) after the overshooting cubic y = x^3-x*sin(x*pi)
			case TYPE::BACKIN: { /*
				return p * p * p - p * sin(p * pi); */
				float s = 1.70158f;
				return p * p * ((s + 1) * p - s);
			}

							 // Modeled (originally) after overshooting cubic y = 1-((1-x)^3-(1-x)*sin((1-x)*pi))
			case TYPE::BACKOUT: { /*
				float f = (1 - p);
				return 1 - (f * f * f - f * sin(f * pi)); */
				float s = 1.70158f;
				return --p, 1.f * (p * p * ((s + 1) * p + s) + 1);
			}

							  // Modeled (originally) after the piecewise overshooting cubic function:
							  // y = (1/2)*((2x)^3-(2x)*sin(2*x*pi))           ; [0, 0.5)
							  // y = (1/2)*(1-((1-x)^3-(1-x)*sin((1-x)*pi))+1) ; [0.5, 1]
			case TYPE::BACKINOUT: {
				float s = 1.70158f * 1.525f;
				if (p < 0.5f) {
					return p *= 2, 0.5f * p * p * (p * s + p - s);
				}
				else {
					return p = p * 2 - 2, 0.5f * (2 + p * p * (p * s + p + s));
				}
			}

#           define tween$bounceout(p) ( \
                (p) < 4/11.f ? (121 * (p) * (p))/16.f : \
                (p) < 8/11.f ? (363/40.f * (p) * (p)) - (99/10.f * (p)) + 17/5.f : \
                (p) < 9/10.f ? (4356/361.f * (p) * (p)) - (35442/1805.f * (p)) + 16061/1805.f \
                           : (54/5.f * (p) * (p)) - (513/25.f * (p)) + 268/25.f )

			case TYPE::BOUNCEIN: {
				return 1 - tween$bounceout(1 - p);
			}

			case TYPE::BOUNCEOUT: {
				return tween$bounceout(p);
			}

			case TYPE::BOUNCEINOUT: {
				if (p < 0.5) {
					return 0.5f * (1 - tween$bounceout(1 - p * 2));
				}
				else {
					return 0.5f * tween$bounceout((p * 2 - 1)) + 0.5f;
				}
			}

#           undef tween$bounceout

			case TYPE::SINESQUARE: {
				float A = sin((p)*pi2);
				return A * A;
			}

			case TYPE::EXPONENTIAL: {
				return 1 / (1 + exp(6 - 12 * (p)));
			}

			case TYPE::SCHUBRING1: {
				return 2 * (p + (0.5f - p) * abs(0.5f - p)) - 0.5f;
			}

			case TYPE::SCHUBRING2: {
				float p1pass = 2 * (p + (0.5f - p) * abs(0.5f - p)) - 0.5f;
				float p2pass = 2 * (p1pass + (0.5f - p1pass) * abs(0.5f - p1pass)) - 0.5f;
				float pAvg = (p1pass + p2pass) / 2;
				return pAvg;
			}

			case TYPE::SCHUBRING3: {
				float p1pass = 2 * (p + (0.5f - p) * abs(0.5f - p)) - 0.5f;
				float p2pass = 2 * (p1pass + (0.5f - p1pass) * abs(0.5f - p1pass)) - 0.5f;
				return p2pass;
			}

			case TYPE::SWING: {
				return ((-cos(pi * p) * 0.5f) + 0.5f);
			}

			case TYPE::SINPI2: {
				return sin(p * pi2);
			}
		}
	}
}

namespace ImGui
{
	float SplineValue(float p, uint32 maxpoints, const ImVec2* points)
	{
		if (maxpoints < 2 || !points)
		{
			return 0;
		}

		if (p < 0.f)
		{
			return points[0].y;
		}

		float* input = (float*)alloca(sizeof(float) * maxpoints * 2);
		float* ts = input;
		float* values = input + maxpoints;
		for (uint32 i = 0; i < maxpoints; ++i)
		{
			ts[i] = points[i].x;
			values[i] = points[i].y;
		}

		float output = spline(ts, values, maxpoints, p);

		return output;
	}

	bool Spline(const char* label, const ImVec2& size, uint32 maxpoints, ImVec2* points, uint32 drawResolution)
	{
		bool modified = false;
		if (maxpoints < 2 || points == 0)
		{
			return false;
		}

		if (points[0].x < 0)
		{
			points[0].x = 0;
			points[0].y = 0;
			points[1].x = 1;
			points[1].y = 1;
			points[2].x = -1;
		}

		ImGuiWindow* window = GetCurrentWindow();
		const ImGuiStyle& style = GetStyle();
		const ImGuiID id = window->GetID(label);
		if (window->SkipItems)
		{
			return false;
		}

		ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
		ItemSize(bb);
		if (!ItemAdd(bb, NULL))
		{
			return false;
		}

		const bool hovered = IsItemHovered();

		int max = 0;
		while (max < (int)maxpoints && points[max].x >= 0)
		{
			++max;
		}

		RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg, 1), true, style.FrameRounding);

		float ht = bb.Max.y - bb.Min.y;
		float wd = bb.Max.x - bb.Min.x;

		auto& io = GetIO();

		int kill = 0;

		if (hovered)
		{
			if (io.MouseDown[0])
			{
				modified = true;
				ImVec2 pos = (io.MousePos - bb.Min) / (bb.Max - bb.Min);
				pos.y = 1 - pos.y;

				int left = 0;
				while (left < max && points[left].x < pos.x)
				{
					++left;
				}
				if (left)
				{
					--left;
				}

				ImVec2 p = points[left] - pos;
				float p1d = sqrt(p.x * p.x + p.y * p.y);
				p = points[left + 1] - pos;
				float p2d = sqrt(p.x * p.x + p.y * p.y);
				int sel = -1;
				if (p1d < (1 / 10.f)) { sel = left; }
				if (p2d < (1 / 10.f)) { sel = left + 1; }

				if (sel != -1)
				{
					if (io.MouseClicked[0] && io.KeyCtrl)
					{
						kill = sel;
					}
					else
					{
						points[sel] = pos;
					}
				}
				else if (io.MouseClicked[0] && !io.KeyCtrl)
				{
					if (max < (int)maxpoints)
					{
						++max;
						for (int i = max; i > left; i--)
						{
							points[i] = points[i - 1];
						}
						points[left + 1] = pos;
					}
					if (max < (int)maxpoints)
					{
						points[max].x = -1;
					}
				}

				// Snap first/last to min/max.
				if (points[0].x < points[max - 1].x) 
				{
					points[0].x = 0;
					points[max - 1].x = 1;
				}
				else 
				{
					points[0].x = 1;
					points[max - 1].x = 0;
				}
			}
		}

		do
		{
			if (kill)
			{
				modified = true;
				for (int i = kill + 1; i < max; ++i)
				{
					points[i - 1] = points[i];
				}
				--max;
				points[max].x = -1;
				kill = 0;
			}

			for (int i = 1; i < max - 1; ++i)
			{
				if (points[i].x < points[i - 1].x)
				{
					kill = i;
				}
			}
		} while (kill);


		// Draw background grid.
		window->DrawList->AddLine(
			ImVec2(bb.Min.x, bb.Min.y + ht / 2),
			ImVec2(bb.Max.x, bb.Min.y + ht / 2),
			GetColorU32(ImGuiCol_TextDisabled), 3);

		window->DrawList->AddLine(
			ImVec2(bb.Min.x, bb.Min.y + ht / 4),
			ImVec2(bb.Max.x, bb.Min.y + ht / 4),
			GetColorU32(ImGuiCol_TextDisabled));

		window->DrawList->AddLine(
			ImVec2(bb.Min.x, bb.Min.y + ht / 4 * 3),
			ImVec2(bb.Max.x, bb.Min.y + ht / 4 * 3),
			GetColorU32(ImGuiCol_TextDisabled));

		for (int i = 0; i < 9; ++i)
		{
			window->DrawList->AddLine(
				ImVec2(bb.Min.x + (wd / 10) * (i + 1), bb.Min.y),
				ImVec2(bb.Min.x + (wd / 10) * (i + 1), bb.Max.y),
				GetColorU32(ImGuiCol_TextDisabled));
		}

		// Draw smooth curve.
		drawResolution = min(drawResolution, (uint32)size.x);
		for (uint32 i = 0; i < drawResolution; ++i)
		{
			float px = (i + 0) / float(drawResolution);
			float qx = (i + 1) / float(drawResolution);
			float py = 1 - SplineValue(px, max, points);
			float qy = 1 - SplineValue(qx, max, points);
			ImVec2 p(px * (bb.Max.x - bb.Min.x) + bb.Min.x, py * (bb.Max.y - bb.Min.y) + bb.Min.y);
			ImVec2 q(qx * (bb.Max.x - bb.Min.x) + bb.Min.x, qy * (bb.Max.y - bb.Min.y) + bb.Min.y);
			window->DrawList->AddLine(p, q, GetColorU32(ImGuiCol_PlotLines));
		}

		// Draw lines.
#if 0
		for (int i = 1; i < max; i++)
		{
			ImVec2 a = points[i - 1];
			ImVec2 b = points[i];
			a.y = 1 - a.y;
			b.y = 1 - b.y;
			a = a * (bb.Max - bb.Min) + bb.Min;
			b = b * (bb.Max - bb.Min) + bb.Min;
			window->DrawList->AddLine(a, b, GetColorU32(ImGuiCol_PlotLinesHovered));
		}
#endif

		if (hovered)
		{
			// Draw control points.
			for (int i = 0; i < max; i++)
			{
				ImVec2 p = points[i];
				p.y = 1 - p.y;
				p = p * (bb.Max - bb.Min) + bb.Min;
				ImVec2 a = p - ImVec2(2, 2);
				ImVec2 b = p + ImVec2(2, 2);
				window->DrawList->AddRect(a, b, GetColorU32(ImGuiCol_PlotLinesHovered));
			}
		}

		if (ImGui::Button("Flip")) 
		{
			for (int i = 0; i < max; ++i)
			{
				points[i].y = 1 - points[i].y;
			}
		}
		ImGui::SameLine();

		if (ImGui::Button("Mirror"))
		{
			for (int i = 0; i < max / 2; ++i)
			{
				int j = max - 1 - i;
				points[i].x = 1 - points[i].x;
				points[j].x = 1 - points[j].x;
				std::swap(points[i], points[j]);
			}
			if (max % 2 == 1)
			{
				points[max / 2 + 1].x = 1 - points[max / 2 + 1].x;
			}
		}
		ImGui::SameLine();

		// Preset selector.
		const char* items[] = 
		{
			"Choose preset",

			"Linear",
			"Quad in",
			"Quad out",
			"Quad in  out",
			"Cubic in",
			"Cubic out",
			"Cubic in  out",
			"Quart in",
			"Quart out",
			"Quart in  out",
			"Quint in",
			"Quint out",
			"Quint in  out",
			"Sine in",
			"Sine out",
			"Sine in  out",
			"Expo in",
			"Expo out",
			"Expo in  out",
			"Circ in",
			"Circ out",
			"Circ in  out",
			"Elastic in",
			"Elastic out",
			"Elastic in  out",
			"Back in",
			"Back out",
			"Back in  out",
			"Bounce in",
			"Bounce out",
			"Bounce in out",

			"Sine square",
			"Exponential",

			"Schubring1",
			"Schubring2",
			"Schubring3",

			"SinPi2",
			"Swing"
		};
		int item = 0;
		if (modified) 
		{
			item = 0;
		}
		if (ImGui::Combo("##preset", &item, items, IM_ARRAYSIZE(items))) 
		{
			max = maxpoints;
			if (item > 0) 
			{
				for (int i = 0; i < max; ++i)
				{
					points[i].x = i / float(max - 1);
					points[i].y = float(tween::ease(item - 1, points[i].x));
				}
			}
		}

		char buf[128];
		const char* str = label;

		if (hovered) 
		{
			ImVec2 pos = (io.MousePos - bb.Min) / (bb.Max - bb.Min);
			pos.y = 1 - pos.y;

			sprintf(buf, "%s (%f,%f)", label, pos.x, pos.y);
			str = buf;
		}

		RenderTextClipped(ImVec2(bb.Min.x, bb.Min.y + style.FramePadding.y), bb.Max, str, NULL, NULL);

		return modified;
	}

}


