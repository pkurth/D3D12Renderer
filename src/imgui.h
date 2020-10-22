#pragma once

#include <imgui/imgui.h>

#include "dx.h"
#include "input.h"

ImGuiContext* initializeImGui(D3D12_RT_FORMAT_ARRAY screenRTFormats);
void newImGuiFrame(const user_input& input, float dt);
void renderImGui(struct dx_command_list* cl);

void handleImGuiInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
