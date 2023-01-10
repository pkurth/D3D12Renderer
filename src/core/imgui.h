#pragma once

#include <imgui/imgui.h>
#include <fontawesome/IconsFontAwesome5.h>

#include "dx/dx.h"
#include "math.h"

ImGuiContext* initializeImGui(struct dx_window& window);
void newImGuiFrame(float dt);
void renderImGui(struct dx_command_list* cl);

LRESULT handleImGuiInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct dx_texture;
struct asset_handle;

enum imgui_icon
{
	imgui_icon_global,
	imgui_icon_local,
	imgui_icon_translate,
	imgui_icon_rotate,
	imgui_icon_scale,
	imgui_icon_cross,
	imgui_icon_play,
	imgui_icon_stop,
	imgui_icon_pause,
};

static const char* imguiIconNames[] =
{
	"Transform in global coordinate system (G)",
	"Transform in local coordinate system (G)",
	"Translate (W)",
	"Rotate (E)",
	"Scale (R)",
	"No gizmo (Q)",
	"Play",
	"Stop",
	"Pause",
};

#define IMGUI_ICON_COLS 4
#define IMGUI_ICON_ROWS 4

#define IMGUI_ICON_DEFAULT_SIZE 35
#define IMGUI_ICON_DEFAULT_SPACING 3.f

namespace ImGui
{
	bool AnyModifiersDown();

	bool BeginWindowHiddenTabBar(const char* name, bool* open = 0, ImGuiWindowFlags flags = 0);
	bool BeginControlsWindow(const char* name);

	void Image(struct dx_cpu_descriptor_handle& handle, ImVec2 size);
	void Image(struct dx_cpu_descriptor_handle& handle, uint32 width, uint32 height);
	void Image(const ref<dx_texture>& texture, ImVec2 size);
	void Image(const ref<dx_texture>& texture, uint32 width = 0, uint32 height = 0);

	inline float CalcButtonWidth(const char* text) { return CalcTextSize(text).x + (GetStyle().FramePadding.x + GetStyle().FrameBorderSize) * 2.f; }

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
	bool BeginTreeColoredText(const char* label, vec3 color, bool defaultOpen = false);
	void EndTree();

	inline void Value(const char* prefix, int64 v) { ImGui::Text("%s: %lld", prefix, v); }
	inline void Value(const char* prefix, uint64 v) { ImGui::Text("%s: %llu", prefix, v); }
	inline void Value(const char* prefix, const char* v) { ImGui::Text("%s: %s", prefix, v); }

	void CenteredText(const char* text);

	void PopupOkButton(uint32 width = 120);

	bool AssetHandle(const char* label, const char* type, asset_handle& asset, const char* clearText = 0);


	bool BeginProperties();
	void EndProperties();

	void PropertyValue(const char* label, const char* format, ...);

	inline void PropertyValue(const char* label, bool v) { ImGui::PropertyValue(label, v ? "True" : "False"); }
	inline void PropertyValue(const char* label, float v, const char* format = "%.3f") { ImGui::PropertyValue(label, format, v); }
	inline void PropertyValue(const char* label, int32 v, const char* format = "%d") { ImGui::PropertyValue(label, format, v); }
	inline void PropertyValue(const char* label, uint32 v, const char* format = "%u") { ImGui::PropertyValue(label, format, v); }
	inline void PropertyValue(const char* label, int64 v, const char* format = "%lld") { ImGui::PropertyValue(label, format, v); }
	inline void PropertyValue(const char* label, uint64 v, const char* format = "%llu") { ImGui::PropertyValue(label, format, v); }
	inline void PropertyValue(const char* label, vec2 v, const char* format = "%.3f, %.3f") { ImGui::PropertyValue(label, format, v.x, v.y); }
	inline void PropertyValue(const char* label, vec3 v, const char* format = "%.3f, %.3f, %.3f") { ImGui::PropertyValue(label, format, v.x, v.y, v.z); }
	inline void PropertyValue(const char* label, vec4 v, const char* format = "%.3f, %.3f, %.3f, %.3f") { ImGui::PropertyValue(label, format, v.x, v.y, v.z, v.w); }

	bool PropertyCheckbox(const char* label, bool& v);

	bool PropertySlider(const char* label, float& f, float minValue = 0.f, float maxValue = 1.f, const char* format = "%.3f", ImGuiSliderFlags flags = ImGuiSliderFlags_None);
	bool PropertySlider(const char* label, vec2& f, float minValue = 0.f, float maxValue = 1.f, const char* format = "%.3f", ImGuiSliderFlags flags = ImGuiSliderFlags_None);
	bool PropertySlider(const char* label, vec3& f, float minValue = 0.f, float maxValue = 1.f, const char* format = "%.3f", ImGuiSliderFlags flags = ImGuiSliderFlags_None);
	bool PropertySlider(const char* label, vec4& f, float minValue = 0.f, float maxValue = 1.f, const char* format = "%.3f", ImGuiSliderFlags flags = ImGuiSliderFlags_None);

	bool PropertySliderAngle(const char* label, float& fRad, float minValueDeg = -360.f, float maxValueDeg = 360.f, const char* format = "%.0f deg");

	bool PropertySlider(const char* label, int32& v, int minValue, int maxValue, const char* format = "%d");
	bool PropertySlider(const char* label, uint32& v, uint32 minValue, uint32 maxValue, const char* format = "%u");

	bool PropertyInput(const char* label, float& f, const char* format = "%.3f");
	bool PropertyInput(const char* label, vec2& f, const char* format = "%.3f");
	bool PropertyInput(const char* label, vec3& f, const char* format = "%.3f");
	bool PropertyInput(const char* label, vec4& f, const char* format = "%.3f");
	bool PropertyInput(const char* label, int32& f, const char* format = "%d");
	bool PropertyInput(const char* label, uint32& f, const char* format = "%u");

	bool PropertyDrag(const char* label, float& f, float speed = 1.f, const char* format = "%.3f");
	bool PropertyDrag(const char* label, vec2& f, float speed = 1.f, const char* format = "%.3f");
	bool PropertyDrag(const char* label, vec3& f, float speed = 1.f, const char* format = "%.3f");
	bool PropertyDrag(const char* label, vec4& f, float speed = 1.f, const char* format = "%.3f");
	bool PropertyDrag(const char* label, int32& f, float speed = 1.f, const char* format = "%d");
	bool PropertyDrag(const char* label, uint32& f, float speed = 1.f, const char* format = "%u");

	bool PropertyDropdown(const char* label, const char** names, uint32 count, uint32& current);
	bool PropertyDropdown(const char* label, const char* (*name_func)(uint32, void*), uint32& current, void* data = 0);
	bool PropertyDropdownPowerOfTwo(const char* label, uint32 from, uint32 to, uint32& current);

	bool PropertyColor(const char* label, vec3& f);
	bool PropertyColor(const char* label, vec4& f);

	bool PropertyColorWheel(const char* label, vec3& f);
	bool PropertyColorWheel(const char* label, vec4& f);

	bool PropertyButton(const char* label, const char* buttonText, const char* hoverText = 0, ImVec2 size = ImVec2(0, 0));

	bool PropertyInputText(const char* label, char* buffer, uint32 bufferSize, bool disableInput = false);

	bool PropertyAssetHandle(const char* label, const char* type, asset_handle& asset, const char* clearText = 0);

	bool PropertyDragDropStringTarget(const char* label, const char* dragDropID, std::string& value, const char* clearLabel = 0);

	void PropertySeparator();




	float SplineValue(float p, const float* x, const float* y, uint32 numPoints);
	bool Spline(const char* label, ImVec2 size, uint32 maxNumPoints, float* x, float* y, uint32 drawResolution = 256);

#ifdef spline
	template <uint32 maxNumPoints> bool Spline(const char* label, ImVec2 size, struct spline(float, maxNumPoints)& s, uint32 drawResolution = 256)
	{
		return Spline(label, size, maxNumPoints, s.ts, s.values, drawResolution);
	}
#endif

	static const ImColor white(1.f, 1.f, 1.f, 1.f);
	static const ImColor yellow(1.f, 1.f, 0.f, 1.f);
	static const ImColor green(0.f, 1.f, 0.f, 1.f);
	static const ImColor red(1.f, 0.f, 0.f, 1.f);
	static const ImColor blue(0.f, 0.f, 1.f, 1.f);
}
