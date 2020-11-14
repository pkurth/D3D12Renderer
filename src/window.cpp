#include "pch.h"
#include "window.h"
#include <Windowsx.h>

#include <algorithm>

#include "software_window.h"
#include "imgui.h"



static bool running = true;
win32_window* win32_window::mainWindow = 0;

static bool windowClassInitialized;
static const TCHAR* windowClassName = TEXT("APP WINDOW");

static bool atLeastOneWindowWasOpened;
static uint32 numOpenWindows;

static LRESULT CALLBACK windowCallBack(
	_In_ HWND   hwnd,
	_In_ UINT   msg,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam
);

static void setFullscreen(HWND windowHandle, bool fullscreen, WINDOWPLACEMENT& windowPosition)
{
	DWORD style = GetWindowLong(windowHandle, GWL_STYLE);

	if (fullscreen)
	{
		if (style & WS_OVERLAPPEDWINDOW)
		{
			MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
			if (GetWindowPlacement(windowHandle, &windowPosition) &&
				GetMonitorInfo(MonitorFromWindow(windowHandle, MONITOR_DEFAULTTOPRIMARY), &monitorInfo))
			{
				SetWindowLong(windowHandle, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
				SetWindowPos(windowHandle, HWND_TOP,
					monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
					monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
					monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
					SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
			}
		}
	}
	else
	{
		if (!(style & WS_OVERLAPPEDWINDOW))
		{
			SetWindowLong(windowHandle, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
			SetWindowPlacement(windowHandle, &windowPosition);
			SetWindowPos(windowHandle, 0, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	}
}

bool win32_window::initialize(const TCHAR* name, uint32 clientWidth, uint32 clientHeight, bool visible)
{
	if (!windowClassInitialized)
	{
		WNDCLASSEX wndClass;
		ZeroMemory(&wndClass, sizeof(WNDCLASSEX));
		wndClass.cbSize = sizeof(WNDCLASSEX);
		wndClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
		wndClass.lpfnWndProc = windowCallBack;
		//wndClass.hInstance = instance;
		wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndClass.lpszClassName = windowClassName;

		if (!RegisterClassEx(&wndClass))
		{
			std::cerr << "Failed to create window class." << std::endl;
			return 1;
		}

		windowClassInitialized = true;
	}

	if (!windowHandle)
	{
		++numOpenWindows;

		this->clientWidth = clientWidth;
		this->clientHeight = clientHeight;

		DWORD windowStyle = WS_OVERLAPPEDWINDOW;

		RECT r = { 0, 0, (LONG)clientWidth, (LONG)clientHeight };
		AdjustWindowRect(&r, windowStyle, FALSE);
		int width = r.right - r.left;
		int height = r.bottom - r.top;

		windowHandle = CreateWindowEx(0, windowClassName, name, windowStyle,
#if 1
			CW_USEDEFAULT, CW_USEDEFAULT,
#else
			0, 0
#endif
			width, height,
			0, 0, 0, 0);

		fullscreen = false;

		SetWindowLongPtr(windowHandle, GWLP_USERDATA, (LONG_PTR)this);

		if (!windowHandle)
		{
			std::cerr << "Failed to create window." << std::endl;
			return false;
		}

		atLeastOneWindowWasOpened = true;
	}

	open = true;
	this->visible = visible;
	if (visible)
	{
		ShowWindow(windowHandle, SW_SHOW);
	}

	return true;
}

void win32_window::shutdown()
{
	if (windowHandle)
	{
		HWND handle = windowHandle;
		windowHandle = 0;
		DestroyWindow(handle);
	}
}

void win32_window::toggleFullscreen()
{
	fullscreen = !fullscreen;
	setFullscreen(windowHandle, fullscreen, windowPosition);
}

win32_window::win32_window(win32_window&& o)
{
	open = o.open;
	windowHandle = o.windowHandle;
	clientWidth = o.clientWidth;
	clientHeight = o.clientHeight;
	windowPosition = o.windowPosition;
	fullscreen = o.fullscreen;

	o.windowHandle = 0;

	if (windowHandle && open)
	{
		SetWindowLongPtr(windowHandle, GWLP_USERDATA, (LONG_PTR)this);
	}

	if (mainWindow == &o)
	{
		mainWindow = this;
	}
}

win32_window::~win32_window()
{
	shutdown();
}

void win32_window::makeActive()
{
	SetForegroundWindow(windowHandle);
}

void win32_window::changeTitle(const TCHAR* format, ...)
{
	TCHAR titleBuffer[128];

	va_list arg;
	va_start(arg, format);
	_vstprintf_s(titleBuffer, format, arg);
	va_end(arg);

	SetWindowText(windowHandle, titleBuffer);
}

void win32_window::toggleVisibility()
{
	visible = !visible;
	if (visible)
	{
		ShowWindow(windowHandle, SW_SHOW);
	}
	else
	{
		ShowWindow(windowHandle, SW_HIDE);
	}
}

void win32_window::moveTo(int x, int y)
{
	SetWindowPos(windowHandle, HWND_TOP, x, y, clientWidth, clientHeight, SWP_NOSIZE);
}

void win32_window::moveToScreenID(int screenID)
{
	DISPLAY_DEVICEA dispDevice = { 0 };
	dispDevice.cb = sizeof(dispDevice);

	DEVMODEA devMode = { 0 };
	devMode.dmSize = sizeof(devMode);

	if (EnumDisplayDevicesA(NULL, screenID, &dispDevice, 0))
	{
		if (EnumDisplaySettingsExA(dispDevice.DeviceName, ENUM_CURRENT_SETTINGS, &devMode, NULL))
		{
			moveTo(devMode.dmPosition.x, devMode.dmPosition.y);
		}
	}
}

struct monitor_iterator
{
	DISPLAY_DEVICEA dispDevice;
	DEVMODEA devMode;
	DWORD screenID;

	monitor_iterator();

	bool step(monitor_info& info);
};

void win32_window::moveToMonitor(const std::string& uniqueID)
{
	monitor_iterator it;
	monitor_info monitor;
	while (it.step(monitor))
	{
		if (monitor.uniqueID == uniqueID)
		{
			moveTo(monitor.x, monitor.y);
			return;
		}
	}
}

void win32_window::moveToMonitor(const monitor_info& monitor)
{
	moveTo(monitor.x, monitor.y);
}

monitor_iterator::monitor_iterator()
{
	ZeroMemory(&dispDevice, sizeof(dispDevice));
	ZeroMemory(&devMode, sizeof(devMode));
	dispDevice.cb = sizeof(dispDevice);
	devMode.dmSize = sizeof(devMode);
	screenID = 0;
}

static std::string convertUniqueIDToFolderFriendlyName(const std::string& uniqueID)
{
	std::string result = uniqueID;
	std::replace(result.begin(), result.end(), '\\', '_');
	std::replace(result.begin(), result.end(), '?', '_');
	return result;
}

bool monitor_iterator::step(monitor_info& info)
{
	bool result = false;

	if (EnumDisplayDevicesA(NULL, screenID, &dispDevice, 0))
	{
		char name[sizeof(dispDevice.DeviceName)];
		strcpy_s(name, dispDevice.DeviceName);
		if (EnumDisplayDevicesA(name, 0, &dispDevice, EDD_GET_DEVICE_INTERFACE_NAME))
		{
			if (EnumDisplaySettingsExA(name, ENUM_CURRENT_SETTINGS, &devMode, NULL))
			{
				info.x = devMode.dmPosition.x;
				info.y = devMode.dmPosition.y;
				info.width = devMode.dmPelsWidth;
				info.height = devMode.dmPelsHeight;
				info.screenID = screenID;
				info.uniqueID = convertUniqueIDToFolderFriendlyName(dispDevice.DeviceID);
				info.name = dispDevice.DeviceString;
				result = true;
			}
		}
	}

	++screenID;

	return result;
}

std::vector<monitor_info> getAllDisplayDevices()
{
	std::vector<monitor_info> result;

	monitor_iterator it;
	monitor_info monitor;
	while (it.step(monitor))
	{
		result.push_back(monitor);
	}

	return result;
}

std::vector<monitor_info> getAllDisplayDevices(uint32 width, uint32 height)
{
	std::vector<monitor_info> result;

	for (monitor_info& monitor : getAllDisplayDevices())
	{
		if (width == monitor.width && height == monitor.height)
		{
			result.push_back(monitor);
		}
	}

	return result;
}

void setMainWindow(win32_window* window)
{
	win32_window::mainWindow = window;
}

static LRESULT CALLBACK windowCallBack(
	_In_ HWND   hwnd,
	_In_ UINT   msg,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam
)
{
	LRESULT result = 0;

	handleImGuiInput(hwnd, msg, wParam, lParam);

	win32_window* window = (win32_window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch (msg)
	{
		// The default window procedure will play a system notification sound 
		// when pressing the Alt+Enter keyboard combination if this message is 
		// not handled.
		case WM_SYSCHAR:
			break;
		case WM_SIZE:
		{
			if (window && window->open)
			{
				window->clientWidth = LOWORD(lParam);
				window->clientHeight = HIWORD(lParam);
				window->onResize();
			}
		} break;
		case WM_CLOSE:
		{
			DestroyWindow(hwnd);
		} break;
		case WM_DESTROY:
		{
			if (window && window->windowHandle && window->open)
			{
				window->open = false;
				--numOpenWindows;
				window->shutdown();
			}
		} break;
		case WM_PAINT:
		{
			if (window)
			{
				software_window* sWindow = dynamic_cast<software_window*>(window);

				if (sWindow)
				{
					const uint8* image = sWindow->buffer;
					if (image)
					{
						PAINTSTRUCT ps;

						HDC hdc = BeginPaint(hwnd, &ps);
						StretchDIBits(hdc,
#if 0
							0, 0, window->clientWidth, window->clientHeight,
							0, 0, sWindow->bitmapInfo->bmiHeader.biWidth, abs(sWindow->bitmapInfo->bmiHeader.biHeight),
#else
							0, window->clientHeight - 1, window->clientWidth, -(int)window->clientHeight,
							sWindow->blitX, sWindow->blitY, sWindow->blitWidth, sWindow->blitHeight,
#endif
							image, sWindow->bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
						EndPaint(hwnd, &ps);
				}
					else
					{
						result = DefWindowProcW(hwnd, msg, wParam, lParam);
					}
			}
				else
				{
					result = DefWindowProc(hwnd, msg, wParam, lParam);
				}
		}
	} break;
		default:
	{
		result = DefWindowProc(hwnd, msg, wParam, lParam);
	} break;
	}


	return result;
}

bool handleWindowsMessages()
{
	MSG msg = { 0 };
	while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			running = false;
			break;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if (atLeastOneWindowWasOpened && numOpenWindows == 0)
	{
		running = false;
	}

	if (win32_window::mainWindow && !win32_window::mainWindow->open)
	{
		running = false;
	}

	return running;
}
