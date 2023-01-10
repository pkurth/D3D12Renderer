#pragma once

#include <functional>

struct custom_window_style
{
	// Default values match the current ImGui style.

	uint8 titleBarRGB[3] = { 25, 27, 28 };
	uint8 titleBarUnfocusedRGB[3] = { 45, 47, 48 };
	uint8 titleTextRGB[3] = { 255, 255, 255 };
	uint8 buttonHoverRGB[3] = { 80, 80, 80 };

	uint8 closeButtonHoverRGB[3] = { 0xCC, 0, 0 };
	uint8 closeButtonHoverStrokeRGB[3] = { 255, 255, 255 };

	int32 titleLeftOffset = 30;
	int32 titleBarHeight = -1; // Set to -1 for default title bar height.
	int32 buttonHeight = -1; // Set to -1 for buttons as tall as the title bar.
	int32 iconPadding = 5; // Padding around icon. Size of icon will be titleBarHeight - iconPadding * 2.
	int32 borderWidthLeftAndRight = 0; // Set to -1 for default border width.
	int32 borderWidthBottom = 0; // Set to -1 for default border width. Will be used for top when window is full-screen.

	bool preventRoundedTopCorners = true;
};

struct win32_window
{
	win32_window() = default;
	win32_window(win32_window&) = delete;
	win32_window(win32_window&& o) noexcept;

	bool initialize(const TCHAR* name, uint32 clientWidth, uint32 clientHeight, bool visible = true);
	virtual void shutdown();

	virtual ~win32_window();

	virtual void swapBuffers() = 0;
	virtual void toggleFullscreen();
	void setFileDropCallback(std::function<void(const fs::path&)> cb);

	void toggleVisibility();

	void maximize();

	void setMinimumSize(int32 minimumWidth = -1, int32 minimumHeight = -1); // -1 means default Windows limits.

	void moveTo(int x, int y);
	void moveToScreenID(int screenID);
	void moveToMonitor(const std::string& uniqueID);
	void moveToMonitor(const struct monitor_info& monitor);

	void makeActive();

	void setCustomWindowStyle(custom_window_style style = {});
	void resetToDefaultWindowStyle();
	void setIcon(const fs::path& filepath);
	void changeTitle(const TCHAR* format, ...);

	uint32 clientWidth, clientHeight;
	HWND windowHandle = 0;

protected:
	// Internal callbacks.
	virtual void onResize() {}
	virtual void onMove() {}
	virtual void onWindowDisplayChange() {}

	WINDOWPLACEMENT windowPosition;

	bool fullscreen = false;
	bool open = false;
	bool visible = false;

	bool customWindowStyle = false;
	custom_window_style style;
	HICON customIcon = 0;

	int32 minimumWidth = -1;
	int32 minimumHeight = -1;

	std::function<void(const fs::path&)> fileDropCallback;

	int hoveredButton = -1; // Only used for custom window styles.
	bool trackingMouse = false;

	static win32_window* mainWindow;

	friend static int hitTest(POINT point, win32_window* window);
	friend static LRESULT CALLBACK windowCallBack(_In_ HWND hwnd, _In_ UINT msg, _In_ WPARAM wParam, _In_ LPARAM lParam);
	friend void setMainWindow(win32_window* window);
	friend bool handleWindowsMessages();
};


struct monitor_info
{
	// This is specific for the actual physical monitor / projector.
	std::string name;
	std::string uniqueID;

	// This can change from run to run!
	int screenID;
	int x, y;
	uint32 width, height;
};

std::vector<monitor_info> getAllDisplayDevices();
std::vector<monitor_info> getAllDisplayDevices(uint32 width, uint32 height);

void setMainWindow(win32_window* window);
