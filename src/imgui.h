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
}
