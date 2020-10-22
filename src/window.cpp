#include "pch.h"
#include "window.h"
#include <Windowsx.h>

#include <iostream>
#include <algorithm>

#include "software_window.h"
#include "input.h"



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

	win32_window* window = (win32_window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch (msg)
	{
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
		if (window->windowHandle && window->open)
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

static kb_button mapVKCodeToRawButton(uint32 vkCode)
{
	if (vkCode >= '0' && vkCode <= '9')
	{
		return (kb_button)(vkCode + button_0 - '0');
	}
	else if (vkCode >= 'A' && vkCode <= 'Z')
	{
		return (kb_button)(vkCode + button_a - 'A');
	}
	else if (vkCode >= VK_F1 && vkCode <= VK_F12)
	{
		return (kb_button)(vkCode + button_f1 - VK_F1);
	}
	else
	{
		switch (vkCode)
		{
		case VK_SPACE: return button_space;
		case VK_TAB: return button_tab;
		case VK_RETURN: return button_enter;
		case VK_SHIFT: return button_shift;
		case VK_CONTROL: return button_ctrl;
		case VK_ESCAPE: return button_esc;
		case VK_UP: return button_up;
		case VK_DOWN: return button_down;
		case VK_LEFT: return button_left;
		case VK_RIGHT: return button_right;
		case VK_MENU: return button_alt;
		case VK_BACK: return button_backspace;
		case VK_DELETE: return button_delete;
		}
	}
	return button_unknown;
}

bool handleWindowsMessages(user_input& input)
{
	for (int buttonIndex = 0; buttonIndex < button_count; ++buttonIndex)
	{
		input.keyboard[buttonIndex].wasDown = input.keyboard[buttonIndex].isDown;
	}
	input.mouse.left.wasDown = input.mouse.left.isDown;
	input.mouse.right.wasDown = input.mouse.right.isDown;
	input.mouse.middle.wasDown = input.mouse.middle.isDown;
	input.mouse.scroll = 0.f;

	int oldMouseX = input.mouse.x;
	int oldMouseY = input.mouse.y;
	float oldMouseRelX = input.mouse.relX;
	float oldMouseRelY = input.mouse.relY;

	MSG msg = { 0 };
	while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			running = false;
		}

		if (win32_window::mainWindow && win32_window::mainWindow->open && msg.hwnd == win32_window::mainWindow->windowHandle)
		{
			switch (msg.message)
			{
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP:
			{
				uint32 vkCode = (uint32)msg.wParam;
				bool wasDown = ((msg.lParam & (1 << 30)) != 0);
				bool isDown = ((msg.lParam & (1 << 31)) == 0);
				kb_button button = mapVKCodeToRawButton(vkCode);
				if (button != button_unknown)
				{
					input.keyboard[button].isDown = isDown;
					input.keyboard[button].wasDown = wasDown;
				}

				if (button == button_alt)
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				if (vkCode == VK_F4 && (msg.lParam & (1 << 29))) // Alt + F4.
				{
					running = false;
				}
				TranslateMessage(&msg); // This generates the WM_CHAR message.
				DispatchMessage(&msg);
			} break;

			// The default window procedure will play a system notification sound 
			// when pressing the Alt+Enter keyboard combination if this message is 
			// not handled.
			case WM_SYSCHAR:
				break;

			case WM_CHAR:
			{
				//ImGui::GetIO().AddInputCharacter((uint16)msg.wParam);
			} break;
			case WM_LBUTTONDOWN:
			{
				input.mouse.left.isDown = true;
				input.mouse.left.wasDown = false;
			} break;
			case WM_LBUTTONUP:
			{
				input.mouse.left.isDown = false;
				input.mouse.left.wasDown = true;
			} break;
			case WM_RBUTTONDOWN:
			{
				input.mouse.right.isDown = true;
				input.mouse.right.wasDown = false;
			} break;
			case WM_RBUTTONUP:
			{
				input.mouse.right.isDown = false;
				input.mouse.right.wasDown = true;
			} break;
			case WM_MBUTTONDOWN:
			{
				input.mouse.middle.isDown = true;
				input.mouse.middle.wasDown = false;
			} break;
			case WM_MBUTTONUP:
			{
				input.mouse.middle.isDown = false;
				input.mouse.middle.wasDown = true;
			} break;
			case WM_MOUSEMOVE:
			{
				int mousePosX = GET_X_LPARAM(msg.lParam); // Relative to client area.
				int mousePosY = GET_Y_LPARAM(msg.lParam);
				input.mouse.x = mousePosX;
				input.mouse.y = mousePosY;
			} break;
			case WM_MOUSEWHEEL:
			{
				float scroll = GET_WHEEL_DELTA_WPARAM(msg.wParam) / 120.f;
				input.mouse.scroll = scroll;
			} break;
			default:
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			} break;
			}
		}
		else
		{
			if (msg.message == WM_KEYDOWN)
			{
				uint32 vkCode = (uint32)msg.wParam;
				if (vkCode == VK_F4 && (msg.lParam & (1 << 29)) || vkCode == VK_ESCAPE)
				{
					running = false;
				}
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (win32_window::mainWindow)
	{
		input.mouse.relX = (float)input.mouse.x / (win32_window::mainWindow->clientWidth - 1);
		input.mouse.relY = (float)input.mouse.y / (win32_window::mainWindow->clientHeight - 1);
		input.mouse.dx = input.mouse.x - oldMouseX;
		input.mouse.dy = input.mouse.y - oldMouseY;
		input.mouse.reldx = input.mouse.relX - oldMouseRelX;
		input.mouse.reldy = input.mouse.relY - oldMouseRelY;

		if (buttonDownEvent(input, button_enter) && input.keyboard[button_alt].isDown)
		{
			win32_window::mainWindow->toggleFullscreen();
		}
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
