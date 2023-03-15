#include "pch.h"
#include "window.h"
#include <Windowsx.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <vssym32.h>

#include <algorithm>

#include "software_window.h"
#include "core/imgui.h"
#include "core/string.h"
#include "core/image.h"


bool handleWindowsMessages();

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

static int dpiScale(int value, UINT dpi) 
{
	return (int)((float)value * dpi / 96);
}

static int getDefaultTitleBarHeight(HWND handle)
{
	SIZE titleBarSize = { };
	const int topAndBottomBorders = 2;
	HTHEME theme = OpenThemeData(handle, L"WINDOW");
	UINT dpi = GetDpiForWindow(handle);
	GetThemePartSize(theme, NULL, WP_CAPTION, CS_ACTIVE, NULL, TS_TRUE, &titleBarSize);
	CloseThemeData(theme);

	int height = dpiScale(titleBarSize.cy, dpi) + topAndBottomBorders;
	
	return height;
}

static RECT getTitleBarRect(HWND handle, int32 titleBarHeight)
{
	RECT rect;
	GetClientRect(handle, &rect);
	rect.bottom = rect.top + titleBarHeight;
	return rect;
}

static void centerRectInRect(RECT& to_center, const RECT& outer_rect) 
{
	int toWidth = to_center.right - to_center.left;
	int toHeight = to_center.bottom - to_center.top;
	int outerWidth = outer_rect.right - outer_rect.left;
	int outerHeight = outer_rect.bottom - outer_rect.top;

	int paddingX = (outerWidth - toWidth) / 2;
	int paddingY = (outerHeight - toHeight) / 2;

	to_center.left = outer_rect.left + paddingX;
	to_center.top = outer_rect.top + paddingY;
	to_center.right = to_center.left + toWidth;
	to_center.bottom = to_center.top + toHeight;
}

struct custom_titlebar_button_rects 
{
	RECT close;
	RECT maximize;
	RECT minimize;
};

enum titlebar_button_name
{
	titlebar_button_none = -1,
	titlebar_button_close,
	titlebar_button_minimize,
	titlebar_button_maximize,
};

static custom_titlebar_button_rects getTitleBarButtonRects(HWND handle, int32 buttonHeight) 
{
	UINT dpi = GetDpiForWindow(handle);
	custom_titlebar_button_rects result;
	int buttonWidth = dpiScale(47, dpi);
	result.close = getTitleBarRect(handle, buttonHeight);

	result.close.left = result.close.right - buttonWidth;
	result.maximize = result.close;
	result.maximize.left -= buttonWidth;
	result.maximize.right -= buttonWidth;
	result.minimize = result.maximize;
	result.minimize.left -= buttonWidth;
	result.minimize.right -= buttonWidth;
	return result;
}

static int hitTest(POINT point, win32_window* window)
{
	if (!window->fullscreen)
	{
		RECT frameRect;
		GetWindowRect(window->windowHandle, &frameRect);

		int32 border = 5;
		int32 corner = 10;

		if (point.x < frameRect.left + border)
		{
			if (point.y < frameRect.top + corner) { return HTTOPLEFT; }
			if (point.y > frameRect.bottom - corner) { return HTBOTTOMLEFT; }
			return HTLEFT;
		}
		if (point.x > frameRect.right - border)
		{
			if (point.y < frameRect.top + corner) { return HTTOPRIGHT; }
			if (point.y > frameRect.bottom - corner) { return HTBOTTOMRIGHT; }
			return HTRIGHT;
		}
		if (point.y < frameRect.top + border)
		{
			if (point.x < frameRect.left + corner) { return HTTOPLEFT; }
			if (point.x > frameRect.right - corner) { return HTTOPRIGHT; }
			return HTTOP;
		}
		if (point.y > frameRect.bottom - border)
		{
			if (point.x < frameRect.left + corner) { return HTBOTTOMLEFT; }
			if (point.x > frameRect.right - corner) { return HTBOTTOMRIGHT; }
			return HTBOTTOM;
		}

		custom_titlebar_button_rects buttonRects = getTitleBarButtonRects(window->windowHandle, window->style.buttonHeight);
		OffsetRect(&buttonRects.close, frameRect.left, frameRect.top);
		OffsetRect(&buttonRects.minimize, frameRect.left, frameRect.top);
		OffsetRect(&buttonRects.maximize, frameRect.left, frameRect.top);

		if (PtInRect(&buttonRects.close, point))
		{
			return HTCLOSE;
		}
		else if (PtInRect(&buttonRects.maximize, point))
		{
			return HTMAXBUTTON;
		}
		else if (PtInRect(&buttonRects.minimize, point))
		{
			return HTMINBUTTON;
		}

		if (point.y < frameRect.top + window->style.titleBarHeight)
		{
			int32 iconPadding = window->style.iconPadding;
			int32 iconSize = window->style.titleBarHeight - iconPadding * 2;

			RECT iconRect = { iconPadding, iconPadding, iconPadding + iconSize, iconPadding + iconSize };
			OffsetRect(&iconRect, frameRect.left, frameRect.top);
			if (PtInRect(&iconRect, point))
			{
				return HTSYSMENU;
			}

			return HTCAPTION;
		}
	}

	return HTCLIENT;
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
			return false;
		}

		RegisterHotKey(0, 0, 0, VK_SNAPSHOT);
		RegisterHotKey(0, 1, MOD_CONTROL, VK_SNAPSHOT);

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

	if (!mainWindow)
	{
		mainWindow = this;
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

win32_window::win32_window(win32_window&& o) noexcept
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

static void redrawWindowFrame(HWND windowHandle)
{
	RECT rect;
	GetClientRect(windowHandle, &rect);

	AdjustWindowRectExForDpi(&rect, GetWindowLong(windowHandle, GWL_STYLE), FALSE,
		0,
		GetDpiForWindow(windowHandle));

	ClientToScreen(windowHandle, (POINT*)&rect.left);
	ClientToScreen(windowHandle, (POINT*)&rect.right);
	SetWindowPos(windowHandle, HWND_TOP,
		rect.left, rect.top,
		rect.right - rect.left, rect.bottom - rect.top,
		SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOZORDER);

	handleWindowsMessages(); // Handle messages, so that frame is custom right from the start.
}

void win32_window::setCustomWindowStyle(custom_window_style style)
{
	customWindowStyle = true;

	if (style.titleBarHeight == -1)
	{
		style.titleBarHeight = getDefaultTitleBarHeight(windowHandle);
	}
	if (style.buttonHeight == -1)
	{
		style.buttonHeight = style.titleBarHeight;
	}
	if (style.borderWidthLeftAndRight == -1)
	{
		UINT dpi = GetDpiForWindow(windowHandle);
		style.borderWidthLeftAndRight = GetSystemMetricsForDpi(SM_CXFRAME, dpi);
	}
	if (style.borderWidthBottom == -1)
	{
		UINT dpi = GetDpiForWindow(windowHandle);
		style.borderWidthBottom = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
	}
	this->style = style;

	redrawWindowFrame(windowHandle);
}

void win32_window::resetToDefaultWindowStyle()
{
	customWindowStyle = false;
	redrawWindowFrame(windowHandle);
}

void win32_window::setIcon(const fs::path& filepath)
{
	DirectX::ScratchImage scratchImage;
	D3D12_RESOURCE_DESC desc;

	if (loadImageFromFile(filepath, image_load_flags_cache_to_dds, scratchImage, desc) && scratchImage.GetImageCount() > 0)
	{
		const auto& image = scratchImage.GetImages()[0];
		ASSERT(getNumberOfChannels(image.format) == 4);
		uint8* pixels = image.pixels;
		uint32 width = (uint32)image.width;
		uint32 height = (uint32)image.height;

		HICON icon = createIcon(pixels, width, height);

		if (customIcon)
			DestroyIcon(customIcon);
		customIcon = icon;

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

void win32_window::maximize()
{
	ShowWindow(windowHandle, SW_MAXIMIZE);
}

void win32_window::setMinimumSize(int32 minimumWidth, int32 minimumHeight)
{
	this->minimumWidth = minimumWidth;
	this->minimumHeight = minimumHeight;
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

	if ((msg == WM_MOUSELEAVE || msg == WM_NCMOUSELEAVE || msg == WM_MOUSEMOVE) 
		&& window && window->customWindowStyle)
	{
		if (window->hoveredButton != titlebar_button_none)
		{
			window->hoveredButton = titlebar_button_none;
			//InvalidateRect(0, 0, FALSE);
			RedrawWindow(hwnd, 0, 0, RDW_INVALIDATE | RDW_FRAME);
		}

		window->trackingMouse = false;
	}

	if (handleImGuiInput(hwnd, msg, wParam, lParam))
	{
		return true;
	}

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

				if (window->customWindowStyle)
				{
					if (IsZoomed(hwnd))
					{
						SetWindowRgn(hwnd, 0, true);
					}
					else
					{
						RECT frameRect;
						GetWindowRect(hwnd, &frameRect);

						HRGN region = CreateRectRgn(0, 0, frameRect.right - frameRect.left, frameRect.bottom - frameRect.top);
						SetWindowRgn(hwnd, region, true);
						DeleteObject(region);
					}
				}
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
			if (window && window->customWindowStyle)
			{
				//InvalidateRect(0, 0, FALSE);
				RedrawWindow(hwnd, 0, 0, RDW_INVALIDATE | RDW_FRAME);
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


		// Custom window rendering is inspired by Allen Webster's video: https://www.youtube.com/watch?v=qZZePCNLBuQ
		// and Dmitriy Kubyshkin's blog post: https://kubyshkin.name/posts/win32-window-custom-title-bar-caption/

		case WM_GETMINMAXINFO:
		{
			MINMAXINFO* info = (MINMAXINFO*)lParam;
		
			if (window)
			{
				if (window->minimumWidth != -1) { info->ptMinTrackSize.x = window->minimumWidth; }
				if (window->minimumHeight != -1) { info->ptMinTrackSize.y = window->minimumHeight; }
			}
			return 0;
		} break;

		case WM_NCACTIVATE:
		case WM_NCPAINT:
		{
			if (!window || !window->customWindowStyle)
			{
				return DefWindowProc(hwnd, msg, wParam, lParam);
			}

			HDC hdc = GetDCEx(hwnd, 0, DCX_WINDOW);

			// Render bars and buttons.
			RECT frameRect;
			GetWindowRect(hwnd, &frameRect);

			RECT clientRect;
			GetClientRect(hwnd, &clientRect);

			ClientToScreen(hwnd, (POINT*)&clientRect.left);
			ClientToScreen(hwnd, (POINT*)&clientRect.right);

			OffsetRect(&clientRect, -frameRect.left, -frameRect.top);
			OffsetRect(&frameRect, -frameRect.left, -frameRect.top);

			bool hasFocus = GetActiveWindow() == hwnd;
			bool isMaximized = IsZoomed(hwnd);

			COLORREF titleBarColor = hasFocus
				? RGB(window->style.titleBarRGB[0], window->style.titleBarRGB[1], window->style.titleBarRGB[2])
				: RGB(window->style.titleBarUnfocusedRGB[0], window->style.titleBarUnfocusedRGB[1], window->style.titleBarUnfocusedRGB[2]);
			HBRUSH titleBarBrush = CreateSolidBrush(titleBarColor);
			HPEN titleBarPen = CreatePen(PS_SOLID, 0, titleBarColor);

			SelectObject(hdc, titleBarBrush);
			SelectObject(hdc, titleBarPen);

			Rectangle(hdc, frameRect.left, frameRect.top, frameRect.right, clientRect.top);			// Top bar.
			Rectangle(hdc, frameRect.left, clientRect.top, clientRect.left, clientRect.bottom);		// Left bar.
			Rectangle(hdc, clientRect.right, clientRect.top, frameRect.right, clientRect.bottom);	// Right bar.
			Rectangle(hdc, frameRect.left, clientRect.bottom, frameRect.right, frameRect.bottom);	// Bottom bar.

			COLORREF itemColor = RGB(window->style.titleTextRGB[0], window->style.titleTextRGB[1], window->style.titleTextRGB[2]);

			HBRUSH buttonIconBrush = CreateSolidBrush(itemColor);
			HPEN buttonIconPen = CreatePen(PS_SOLID, 1, itemColor);

			HBRUSH hoverBrush = CreateSolidBrush(RGB(window->style.buttonHoverRGB[0], window->style.buttonHoverRGB[1], window->style.buttonHoverRGB[2]));

			custom_titlebar_button_rects buttonRects = getTitleBarButtonRects(hwnd, window->style.buttonHeight);

			UINT dpi = GetDpiForWindow(hwnd);
			int iconDimension = dpiScale(10, dpi);

			// Minimize button.
			{
				if (window->hoveredButton == titlebar_button_minimize) 
				{
					FillRect(hdc, &buttonRects.minimize, hoverBrush);
				}
				RECT iconRect = { 0 };
				iconRect.right = iconDimension;
				iconRect.bottom = 1;
				centerRectInRect(iconRect, buttonRects.minimize);
				FillRect(hdc, &iconRect, buttonIconBrush);
			}

			// Maximize button.
			{
				HBRUSH frontBrush = titleBarBrush;
				if (window->hoveredButton == titlebar_button_maximize) 
				{
					FillRect(hdc, &buttonRects.maximize, hoverBrush);
					frontBrush = hoverBrush;
				}
				RECT iconRect = { 0 };
				iconRect.right = iconDimension;
				iconRect.bottom = iconDimension;
				centerRectInRect(iconRect, buttonRects.maximize);
				SelectObject(hdc, buttonIconPen);
				SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
				Rectangle(hdc, iconRect.left, iconRect.top, iconRect.right, iconRect.bottom);

				if (isMaximized)
				{
					SelectObject(hdc, frontBrush);
					Rectangle(hdc, iconRect.left - 2, iconRect.top + 2, iconRect.right - 2, iconRect.bottom + 2);
				}
			}

			// Close button.
			{
				HPEN customPen = 0;
				if (window->hoveredButton == titlebar_button_close)
				{
					// Highlight button on hover.
					HBRUSH fillBrush = CreateSolidBrush(RGB(window->style.closeButtonHoverRGB[0], window->style.closeButtonHoverRGB[1], window->style.closeButtonHoverRGB[2]));
					FillRect(hdc, &buttonRects.close, fillBrush);
					DeleteObject(fillBrush);
					customPen = CreatePen(PS_SOLID, 1, RGB(window->style.closeButtonHoverStrokeRGB[0], window->style.closeButtonHoverStrokeRGB[1], window->style.closeButtonHoverStrokeRGB[2]));
					SelectObject(hdc, customPen);
				}
				RECT iconRect = { 0, 0, iconDimension, iconDimension };
				centerRectInRect(iconRect, buttonRects.close);
				MoveToEx(hdc, iconRect.left, iconRect.top, NULL);
				LineTo(hdc, iconRect.right + 1, iconRect.bottom + 1);
				MoveToEx(hdc, iconRect.left, iconRect.bottom, NULL);
				LineTo(hdc, iconRect.right + 1, iconRect.top - 1);
				if (customPen)
				{
					DeleteObject(customPen);
				}
			}

			DeleteObject(titleBarBrush);
			DeleteObject(titleBarPen);

			DeleteObject(hoverBrush);
			DeleteObject(buttonIconPen);
			DeleteObject(buttonIconBrush);


			// Extra padding for title and icon if window is maximized.
			int32 leftAndTopPadding = 0;
			if (isMaximized)
			{
				leftAndTopPadding = 5;
			}


			HTHEME theme = OpenThemeData(hwnd, L"WINDOW");

			// Draw window title.
			LOGFONT logicalFont;
			HFONT oldFont = NULL;
			if (SUCCEEDED(GetThemeSysFont(theme, TMT_CAPTIONFONT, &logicalFont))) 
			{
				HFONT themeFont = CreateFontIndirect(&logicalFont);
				oldFont = (HFONT)SelectObject(hdc, themeFont);
			}

			wchar_t titleTextBuffer[255] = { 0 };
			GetWindowTextW(hwnd, titleTextBuffer, arraysize(titleTextBuffer));
			RECT titleBarTextRect = { window->style.titleLeftOffset + leftAndTopPadding, leftAndTopPadding, buttonRects.minimize.left - 10 + leftAndTopPadding, window->style.titleBarHeight };
			DTTOPTS drawThemeOptions = { sizeof(drawThemeOptions) };
			drawThemeOptions.dwFlags = DTT_TEXTCOLOR;
			drawThemeOptions.crText = itemColor;
			DrawThemeTextEx(
				theme,
				hdc,
				0, 0,
				titleTextBuffer,
				-1,
				DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS,
				&titleBarTextRect,
				&drawThemeOptions
			);

			if (oldFont)
			{
				SelectObject(hdc, oldFont);
			}
			CloseThemeData(theme);




			// Draw window icon.
			HICON icon = window->customIcon;
			if (!icon) 
			{ 
				icon = LoadIcon(NULL, IDI_APPLICATION); 
			}

			if (icon)
			{
				int32 iconPadding = window->style.iconPadding;
				int32 iconSize = window->style.titleBarHeight - iconPadding * 2;

				DrawIconEx(hdc, iconPadding + leftAndTopPadding, iconPadding + leftAndTopPadding, icon, iconSize, iconSize, 0, NULL, DI_NORMAL);
			}



			ReleaseDC(hwnd, hdc);

			return true;
		} break;

		case WM_NCHITTEST:
		{
			if (!window || !window->customWindowStyle)
			{
				return DefWindowProc(hwnd, msg, wParam, lParam);
			}

			POINT point = { LOWORD(lParam), HIWORD(lParam) };
			return hitTest(point, window);
		} break;

		case WM_NCCALCSIZE: 
		{
			if (!window || !window->customWindowStyle)
			{
				return DefWindowProc(hwnd, msg, wParam, lParam);
			}
			
			RECT* rect = (RECT*)lParam;
			if (rect->top > -10000) // Minimized windows seem to have a top of -32000.
			{
				rect->left += window->style.borderWidthLeftAndRight;
				rect->right -= window->style.borderWidthLeftAndRight;
				rect->bottom -= window->style.borderWidthBottom;

				if (!window->fullscreen)
				{
					rect->top += window->style.titleBarHeight;
				}
				else
				{
					rect->top += window->style.borderWidthBottom;
				}
			}
		} break;

		case WM_NCMOUSEMOVE: 
		{
			if (window && window->customWindowStyle)
			{
				POINT cursorPoint = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

				int hoveredButton = titlebar_button_none;
				int hit = hitTest(cursorPoint, window);

				if (hit == HTCLOSE)
				{
					hoveredButton = titlebar_button_close;
				}
				else if (hit == HTMAXBUTTON)
				{
					hoveredButton = titlebar_button_maximize;
				}
				else if (hit == HTMINBUTTON)
				{
					hoveredButton = titlebar_button_minimize;
				}
				
				if (hoveredButton != window->hoveredButton)
				{
					window->hoveredButton = hoveredButton;
					//InvalidateRect(0, 0, FALSE);
					RedrawWindow(hwnd, 0, 0, RDW_INVALIDATE | RDW_FRAME);
				}

				if (!window->trackingMouse)
				{
					TRACKMOUSEEVENT eventTrack = { sizeof(TRACKMOUSEEVENT) };
					eventTrack.dwFlags = TME_NONCLIENT;
					eventTrack.hwndTrack = hwnd;
					eventTrack.dwHoverTime = HOVER_DEFAULT;

					TrackMouseEvent(&eventTrack);

					window->trackingMouse = true;
				}
				return 0;
			}
			return DefWindowProc(hwnd, msg, wParam, lParam);
		}

		case WM_NCLBUTTONDOWN: 
		{
			// Clicks on buttons will be handled in WM_NCLBUTTONUP, but we still need
			// to remove default handling of the click to avoid it counting as drag.
			if (window && window->customWindowStyle && window->hoveredButton != titlebar_button_none) 
			{
				return 0;
			}
			// Default handling allows for dragging and double click to maximize
			return DefWindowProc(hwnd, msg, wParam, lParam);
		}

		case WM_NCLBUTTONUP: 
		{
			if (window && window->customWindowStyle)
			{
				if (window->hoveredButton == titlebar_button_close) 
				{
					window->hoveredButton = titlebar_button_none;
					PostMessage(hwnd, WM_CLOSE, 0, 0);
					return 0;
				}
				else if (window->hoveredButton == titlebar_button_minimize) 
				{
					window->hoveredButton = titlebar_button_none;
					ShowWindow(hwnd, SW_MINIMIZE);
					return 0;
				}
				else if (window->hoveredButton == titlebar_button_maximize) 
				{
					window->hoveredButton = titlebar_button_none;
					int mode = IsZoomed(hwnd) ? SW_NORMAL : SW_MAXIMIZE;
					ShowWindow(hwnd, mode);
					return 0;
				}
			}
			return DefWindowProc(hwnd, msg, wParam, lParam);
		}

		case WM_CREATE:
		{
			RECT rect;
			GetWindowRect(hwnd, &rect);

			// Inform the application of the frame change to force redrawing with the new
			// client area that is extended into the title bar
			SetWindowPos(
				hwnd, NULL,
				rect.left, rect.top,
				rect.right - rect.left, rect.bottom - rect.top,
				SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE
			);
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
		if (msg.message == WM_HOTKEY)
		{
			ImGui::GetIO().KeysDown[VK_SNAPSHOT] = true;
		}
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
