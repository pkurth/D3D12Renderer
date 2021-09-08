#pragma once

#include <imgui/imgui.h>
#include <fontawesome/IconsFontAwesome5.h>

#include "dx/dx.h"
#include "core/math.h"

ImGuiContext* initializeImGui(struct dx_window& window);
void newImGuiFrame(float dt);
void renderImGui(struct dx_command_list* cl);

LRESULT handleImGuiInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct dx_texture;

enum imgui_icon
{
	imgui_icon_global,
	imgui_icon_local,
	imgui_icon_translate,
	imgui_icon_rotate,
	imgui_icon_scale,
	imgui_icon_cross,
};

static const char* imguiIconNames[] =
{
	"Transform in global coordinate system (G)",
	"Transform in local coordinate system (G)",
	"Translate (W)",
	"Rotate (E)",
	"Scale (R)",
	"No widget (Q)",
};

#define IMGUI_ICON_COLS 4
#define IMGUI_ICON_ROWS 4

namespace ImGui
{
	bool BeginWindowHiddenTabBar(const char* name, bool* open = 0, ImGuiWindowFlags flags = 0);

	void Image(struct dx_cpu_descriptor_handle& handle, ImVec2 size);
	void Image(struct dx_cpu_descriptor_handle& handle, uint32 width, uint32 height);
	void Image(const ref<dx_texture>& texture, ImVec2 size);
	void Image(const ref<dx_texture>& texture, uint32 width, uint32 height);

	bool ImageButton(struct dx_cpu_descriptor_handle& handle, ImVec2 size, ImVec2 uvTopLeft = ImVec2(0, 0), ImVec2 uvBottomRight = ImVec2(1, 1));
	bool ImageButton(struct dx_cpu_descriptor_handle& handle, uint32 width, uint32 height, ImVec2 uvTopLeft = ImVec2(0, 0), ImVec2 uvBottomRight = ImVec2(1, 1));
	bool ImageButton(const ref<dx_texture>& texture, ImVec2 size, ImVec2 uvTopLeft = ImVec2(0, 0), ImVec2 uvBottomRight = ImVec2(1, 1));
	bool ImageButton(const ref<dx_texture>& texture, uint32 width, uint32 height, ImVec2 uvTopLeft = ImVec2(0, 0), ImVec2 uvBottomRight = ImVec2(1, 1));

	void Icon(imgui_icon icon, uint32 size);
	bool IconButton(uint32 id, imgui_icon icon, uint32 size, bool enabled = true);
	bool IconRadioButton(imgui_icon icon, int* current, int value, uint32 size, bool enabled = true);

	bool Dropdown(const char* label, const char** names, uint32 count, uint32& current);
	bool Dropdown(const char* label, const char* (*name_func)(uint32, void*), uint32& current, void* data = 0);

	bool DisableableButton(const char* label, bool enabled);
	bool DisableableCheckbox(const char* label, bool& v, bool enabled);

	bool SelectableWrapped(const char* label, int width, bool selected = false, ImGuiSelectableFlags flags = 0);

	bool BeginTree(const char* label, bool defaultOpen = false);
	void EndTree();




	bool BeginProperties();
	void EndProperties();

	bool PropertyCheckbox(const char* label, bool& v);

	bool PropertySlider(const char* label, float& f, float minValue = 0.f, float maxValue = 1.f, const char* format = "%.3f");
	bool PropertySlider(const char* label, vec2& f, float minValue = 0.f, float maxValue = 1.f, const char* format = "%.3f");
	bool PropertySlider(const char* label, vec3& f, float minValue = 0.f, float maxValue = 1.f, const char* format = "%.3f");
	bool PropertySlider(const char* label, vec4& f, float minValue = 0.f, float maxValue = 1.f, const char* format = "%.3f");

	bool PropertySlider(const char* label, int32& v, int minValue, int maxValue, const char* format = "%d");
	bool PropertySlider(const char* label, uint32& v, uint32 minValue, uint32 maxValue, const char* format = "%d");

	bool PropertyInput(const char* label, float& f, const char* format = "%.3f");

	bool PropertyDropdown(const char* label, const char** names, uint32 count, uint32& current);
	bool PropertyDropdown(const char* label, const char* (*name_func)(uint32, void*), uint32& current, void* data = 0);

	bool PropertyColorEdit(const char* label, vec3& f);





	float SplineValue(float p, const float* x, const float* y, uint32 numPoints);
	bool Spline(const char* label, const ImVec2& size, uint32 maxNumPoints, float* x, float* y, uint32 drawResolution = 256);

#ifdef spline
	template <uint32 maxNumPoints> bool Spline(const char* label, const ImVec2& size, struct spline(float, maxNumPoints)& s, uint32 drawResolution = 256)
	{
		return Spline(label, size, maxNumPoints, s.ts, s.values, drawResolution);
	}
#endif

}
