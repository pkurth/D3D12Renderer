#include "pch.h"
#include "window.h"
#include <Windowsx.h>
#include <shellapi.h>

#include <algorithm>

#include "software_window.h"
#include "core/imgui.h"
#include "core/string.h"
#include "core/image.h"



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

static bool isMaximized(HWND hwnd) 
{
	WINDOWPLACEMENT placement = { 0 };
	placement.length = sizeof(WINDOWPLACEMENT);
	if (GetWindowPlacement(hwnd, &placement)) {
		return placement.showCmd == SW_SHOWMAXIMIZED;
	}
	return false;
}

static HICON createIcon(const uint8* image, uint32 width, uint32 height)
{
	BITMAPV5HEADER bi;

	ZeroMemory(&bi, sizeof(bi));
	bi.bV5Size = sizeof(bi);
	bi.bV5Width = width;
	bi.bV5Height = -(int32)height;
	bi.bV5Planes = 1;
	bi.bV5BitCount = 32;
	bi.bV5Compression = BI_BITFIELDS;
	bi.bV5RedMask = 0x00ff0000;
	bi.bV5GreenMask = 0x0000ff00;
	bi.bV5BlueMask = 0x000000ff;
	bi.bV5AlphaMask = 0xff000000;

	uint8* target = NULL;
	HDC dc = GetDC(NULL);
	HBITMAP color = CreateDIBSection(dc,
		(BITMAPINFO*)&bi,
		DIB_RGB_COLORS,
		(void**)&target,
		NULL,
		(DWORD)0);
	ReleaseDC(NULL, dc);

	if (!color)
	{
		std::cerr << "Win32: Failed to create RGBA bitmap.\n";
		return NULL;
	}

	HBITMAP mask = CreateBitmap(width, height, 1, 1, NULL);
	if (!mask)
	{
		std::cerr << "Failed to create mask bitmap.\n";
		DeleteObject(color);
		return NULL;
	}

	for (uint32 i = 0; i < width * height; ++i)
	{
		target[0] = image[2];
		target[1] = image[1];
		target[2] = image[0];
		target[3] = image[3];
		target += 4;
		image += 4;
	}

	ICONINFO ii = {};
	ii.fIcon = true;
	ii.hbmMask = mask;
	ii.hbmColor = color;

	HICON handle = CreateIconIndirect(&ii);

	DeleteObject(color);
	DeleteObject(mask);

	if (!handle)
	{
		std::cerr << "Failed to create icon.\n";
	}

	return handle;
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
		wndClass.hInstance = GetModuleHandle(NULL);
		wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndClass.lpszClassName = windowClassName;

		if (!RegisterClassEx(&wndClass))
		{
			std::cerr << "Failed to create window class.\n";
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
			std::cerr << "Failed to create window.\n";
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

	fullscreen = false;
	open = false;
	visible = false;
}

void win32_window::toggleFullscreen()
{
	fullscreen = !fullscreen;
	setFullscreen(windowHandle, fullscreen, windowPosition);
}

void win32_window::setFileDropCallback(std::function<void(const fs::path&)> cb)
{
	if (!fileDropCallback)
	{
		DragAcceptFiles(windowHandle, true);
	}
	fileDropCallback = cb;
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

void win32_window::setIcon(const fs::path& filepath)
{
	DirectX::ScratchImage scratchImage;
	D3D12_RESOURCE_DESC desc;

	if (loadImageFromFile(filepath, image_load_flags_cache_to_dds, scratchImage, desc) && scratchImage.GetImageCount() > 0)
	{
		const auto& image = scratchImage.GetImages()[0];
		assert(getNumberOfChannels(image.format) == 4);
		uint8* pixels = image.pixels;
		uint32 width = (uint32)image.width;
		uint32 height = (uint32)image.height;

		HICON icon = createIcon(pixels, width, height);

		SendMessage(windowHandle, WM_SETICON, ICON_BIG, (LPARAM)icon);
		SendMessage(windowHandle, WM_SETICON, ICON_SMALL, (LPARAM)icon);
	}
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

	if (handleImGuiInput(hwnd, msg, wParam, lParam))
	{
		return true;
	}

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

		case WM_ACTIVATEAPP:
		{
			if (wParam)
			{
				//std::cout << "Activated\n";
			}
			else
			{
				//std::cout << "Deactivated\n";
			}
		} break;

		case WM_PAINT:
		{
			if (window)
			{
				// For software windows, we draw the content in the client area.
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
						result = DefWindowProc(hwnd, msg, wParam, lParam);
					}
				}
				else
				{
					result = DefWindowProc(hwnd, msg, wParam, lParam);
				}
			}
		} break;

		case WM_DROPFILES:
		{
			if (window && window->fileDropCallback)
			{
				HDROP hdrop = (HDROP)wParam;

				wchar nextFile[MAX_PATH];
				uint32 numFiles = DragQueryFileW(hdrop, -1, NULL, 0);

				for (uint32 i = 0; i < numFiles; ++i)
				{
					if (DragQueryFileW(hdrop, i, nextFile, MAX_PATH) > 0)
					{
						window->fileDropCallback(nextFile);
					}
				}

				DragFinish(hdrop);
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
