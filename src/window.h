#pragma once

#include <functional>

struct win32_window
{
	win32_window() = default;
	win32_window(win32_window&) = delete;
	win32_window(win32_window&& o);

	bool initialize(const TCHAR* name, uint32 clientWidth, uint32 clientHeight, bool visible = true);
	virtual void shutdown();

	virtual ~win32_window();

	virtual void swapBuffers() = 0;
	virtual void toggleFullscreen();
	void setFileDropCallback(std::function<void(const std::string&)> cb);

	// Internal callbacks.
	virtual void onResize() {}
	virtual void onMove() {}
	virtual void onWindowDisplayChange() {}

	void toggleVisibility();

	void moveTo(int x, int y);
	void moveToScreenID(int screenID);
	void moveToMonitor(const std::string& uniqueID);
	void moveToMonitor(const struct monitor_info& monitor);

	void makeActive();

	void changeTitle(const TCHAR* format, ...);

	uint32 clientWidth, clientHeight;
	HWND windowHandle = 0;

	WINDOWPLACEMENT windowPosition;

	bool fullscreen;
	bool open;
	bool visible;

	std::function<void(const std::string&)> fileDropCallback;

	static win32_window* mainWindow;
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
