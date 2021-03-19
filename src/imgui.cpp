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

		QUADIN,
		QUADOUT,
		QUADINOUT,
		CUBICIN,
		CUBICOUT,
		CUBICINOUT,
		QUARTIN,
		QUARTOUT,
		QUARTINOUT,
		QUINTIN,
		QUINTOUT,
		QUINTINOUT,
		SINEIN,
		SINEOUT,
		SINEINOUT,
		EXPOIN,
		EXPOOUT,
		EXPOINOUT,
		CIRCIN,
		CIRCOUT,
		CIRCINOUT,
		ELASTICIN,
		ELASTICOUT,
		ELASTICINOUT,
		BACKIN,
		BACKOUT,
		BACKINOUT,
		BOUNCEIN,
		BOUNCEOUT,
		BOUNCEINOUT,
	};

	static float ease(int easetype, float t)
	{
		switch (easetype)
		{
			default:
			case LINEAR: return t;
			case QUADIN: return easeInQuadratic(t);
			case QUADOUT: return easeOutQuadratic(t);
			case QUADINOUT: return easeInOutQuadratic(t);
			case CUBICIN: return easeInCubic(t);
			case CUBICOUT: return easeOutCubic(t);
			case CUBICINOUT: return easeInOutCubic(t);
			case QUARTIN: return easeInQuartic(t);
			case QUARTOUT: return easeOutQuartic(t);
			case QUARTINOUT: return easeInOutQuartic(t);
			case QUINTIN: return easeInQuintic(t);
			case QUINTOUT: return easeOutQuintic(t);
			case QUINTINOUT: return easeInOutQuintic(t);
			case SINEIN: return easeInSine(t);
			case SINEOUT: return easeOutSine(t);
			case SINEINOUT: return easeInOutSine(t);
			case CIRCIN: return easeInCircular(t);
			case CIRCOUT: return easeOutCircular(t);
			case CIRCINOUT: return easeInOutCircular(t);
			case EXPOIN: return easeInExponential(t);
			case EXPOOUT: return easeOutExponential(t);
			case EXPOINOUT: return easeInOutExponential(t);
			case ELASTICIN: return inElastic(t);
			case ELASTICOUT: return outElastic(t);
			case ELASTICINOUT: return inOutElastic(t);
			case BACKIN: return inBack(t);
			case BACKOUT: return outBack(t);
			case BACKINOUT: return inOutBack(t);
			case BOUNCEIN: return inBounce(t);
			case BOUNCEOUT: return outBounce(t);
			case BOUNCEINOUT: return inOutBounce(t);
		}
	}
}

namespace ImGui
{
	float SplineValue(float p, const float* x, const float* y, uint32 numPoints)
	{
		if (numPoints < 2 || !x || !y)
		{
			return 0;
		}

		if (p < 0.f)
		{
			return y[0];
		}

		float output = evaluateSpline(x, y, numPoints, p);

		return output;
	}

	bool Spline(const char* label, const ImVec2& size, uint32 maxpoints, float* x, float* y, uint32 drawResolution)
	{
		bool modified = false;
		if (maxpoints < 2 || !x || !y)
		{
			return false;
		}

		if (x[0] < 0)
		{
			x[0] = 0;
			y[0] = 0;
			x[1] = 1;
			y[1] = 1;
			x[2] = -1;
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

		PushID(label);

		const bool hovered = IsItemHovered();

		int max = 0;
		while (max < (int)maxpoints && x[max] >= 0)
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
				while (left < max && x[left] < pos.x)
				{
					++left;
				}
				if (left)
				{
					--left;
				}

				ImVec2 p = ImVec2(x[left], y[left]) - pos;
				float p1d = sqrt(p.x * p.x + p.y * p.y);
				p = ImVec2(x[left + 1], y[left + 1]) - pos;
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
						x[sel] = pos.x;
						y[sel] = pos.y;
					}
				}
				else if (io.MouseClicked[0] && !io.KeyCtrl)
				{
					if (max < (int)maxpoints)
					{
						++max;
						for (int i = max - 1; i > left; i--)
						{
							x[i] = x[i - 1];
							y[i] = y[i - 1];
						}
						x[left + 1] = pos.x;
						y[left + 1] = pos.y;
					}
					if (max < (int)maxpoints)
					{
						x[max] = -1;
					}
				}

				// Snap first/last to min/max.
				if (x[0] < x[max - 1]) 
				{
					x[0] = 0;
					x[max - 1] = 1;
				}
				else 
				{
					x[0] = 1;
					x[max - 1] = 0;
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
					x[i - 1] = x[i];
					y[i - 1] = y[i];
				}
				--max;
				x[max] = -1;
				kill = 0;
			}

			for (int i = 1; i < max - 1; ++i)
			{
				if (x[i] < x[i - 1])
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
			float py = 1 - SplineValue(px, x, y, max);
			float qy = 1 - SplineValue(qx, x, y, max);
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
				ImVec2 p(x[i], y[i]);
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
				y[i] = 1 - y[i];
			}
		}
		ImGui::SameLine();

		if (ImGui::Button("Mirror"))
		{
			for (int i = 0; i < max / 2; ++i)
			{
				int j = max - 1 - i;
				x[i] = 1 - x[i];
				x[j] = 1 - x[j];
				std::swap(x[i], x[j]);
				std::swap(y[i], y[j]);
			}
			if (max % 2 == 1)
			{
				x[max / 2] = 1 - x[max / 2];
			}
		}
		ImGui::SameLine();

		// Preset selector.
		const char* items[] = 
		{
			"Choose preset",

			"Linear",
			"Quadratic in",
			"Quadratic out",
			"Quadratic in out",
			"Cubic in",
			"Cubic out",
			"Cubic in out",
			"Quartic in",
			"Quartic out",
			"Quartic in out",
			"Quintic in",
			"Quintic out",
			"Quintic in out",
			"Sine in",
			"Sine out",
			"Sine in out",
			"Exponetial in",
			"Exponetial out",
			"Exponetial in out",
			"Circular in",
			"Circular out",
			"Circular in out",
			"Elastic in",
			"Elastic out",
			"Elastic in out",
			"Back in",
			"Back out",
			"Back in out",
			"Bounce in",
			"Bounce out",
			"Bounce in out",
		};
		int item = 0;
		if (ImGui::Combo("##preset", &item, items, arraysize(items))) 
		{
			max = maxpoints;
			if (item > 0) 
			{
				for (int i = 0; i < max; ++i)
				{
					x[i] = i / float(max - 1);
					y[i] = float(tween::ease(item - 1, x[i]));
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

		RenderTextClipped(ImVec2(bb.Min.x, bb.Min.y + style.FramePadding.y), bb.Max, str, 0, 0);

		PopID();

		return modified;
	}

}


