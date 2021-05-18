#pragma once

#include <imgui/imgui.h>
#include <fontawesome/IconsFontAwesome5.h>

#include "dx.h"

ImGuiContext* initializeImGui(DXGI_FORMAT screenFormat);
void newImGuiFrame(float dt);
void renderImGui(struct dx_command_list* cl);

LRESULT handleImGuiInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct dx_texture;

namespace ImGui
{
	void Image(struct dx_cpu_descriptor_handle& handle, ImVec2 size);
	void Image(struct dx_cpu_descriptor_handle& handle, uint32 width, uint32 height);
	void Image(const ref<dx_texture>& texture, ImVec2 size);
	void Image(const ref<dx_texture>& texture, uint32 width, uint32 height);

	bool Dropdown(const char* label, const char** names, uint32 count, uint32& current);
	bool Dropdown(const char* label, const char* (*name_func)(uint32, void*), uint32& current, void* data = 0);

	bool DisableableButton(const char* label, bool enabled);
	bool DisableableCheckbox(const char* label, bool* v, bool enabled);


	float SplineValue(float p, const float* x, const float* y, uint32 numPoints);
	bool Spline(const char* label, const ImVec2& size, uint32 maxNumPoints, float* x, float* y, uint32 drawResolution = 256);

#ifdef spline
	template <uint32 maxNumPoints> bool Spline(const char* label, const ImVec2& size, struct spline(float, maxNumPoints)& s, uint32 drawResolution = 256)
	{
		return Spline(label, size, maxNumPoints, s.ts, s.values, drawResolution);
	}
#endif

}
