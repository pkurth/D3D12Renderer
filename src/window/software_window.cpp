#include "pch.h"
#include "software_window.h"


software_window::software_window(software_window&& o) noexcept
	: win32_window(std::move(o))
{
	bitmapInfo = o.bitmapInfo;
	o.bitmapInfo = 0;

	buffer = o.buffer;
	o.buffer = 0;
}

bool software_window::initialize(const TCHAR* name, uint32 requestedClientWidth, uint32 requestedClientHeight, 
	uint8* buffer, uint32 numChannels, uint32 bufferWidth, uint32 bufferHeight)
{
	assert(numChannels == 1 || numChannels == 3 || numChannels == 4);

	if (!win32_window::initialize(name, requestedClientWidth, requestedClientHeight))
	{
		return false;
	}

	if (!bitmapInfo)
	{
		bitmapInfo = (BITMAPINFO*)malloc(sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 256);
		if (!bitmapInfo)
		{
			return false;
		}

		bitmapInfo->bmiHeader.biSize = sizeof(bitmapInfo->bmiHeader);
		bitmapInfo->bmiHeader.biCompression = BI_RGB;
		bitmapInfo->bmiHeader.biSizeImage = 0;
		bitmapInfo->bmiHeader.biXPelsPerMeter = 0;
		bitmapInfo->bmiHeader.biYPelsPerMeter = 0;
		bitmapInfo->bmiHeader.biClrUsed = 0;
		bitmapInfo->bmiHeader.biClrImportant = 0;

		for (int i = 0; i < 256; ++i)
		{
			bitmapInfo->bmiColors[i].rgbRed = i;
			bitmapInfo->bmiColors[i].rgbGreen = i;
			bitmapInfo->bmiColors[i].rgbBlue = i;
			bitmapInfo->bmiColors[i].rgbReserved = 0;
		}
	}

	bitmapInfo->bmiHeader.biWidth = bufferWidth == 0 ? clientWidth : bufferWidth;
	bitmapInfo->bmiHeader.biHeight = bufferHeight == 0 ? clientHeight : bufferHeight;
	bitmapInfo->bmiHeader.biPlanes = 1;
	bitmapInfo->bmiHeader.biBitCount = 8 * numChannels;

	blitX = 0;
	blitY = 0;
	blitWidth = bufferWidth == 0 ? clientWidth : bufferWidth;
	blitHeight = bufferHeight == 0 ? clientHeight : bufferHeight;

	this->buffer = buffer;

	return true;
}

void software_window::changeBlitRegion(uint32 x, uint32 y, uint32 width, uint32 height)
{
	blitX = x;
	blitY = y;
	blitWidth = width;
	blitHeight = height;
}

void software_window::shutdown()
{
	if (bitmapInfo)
	{
		free(bitmapInfo);
		bitmapInfo = 0;
	}
	win32_window::shutdown();
}

software_window::~software_window()
{
	shutdown();
}

void software_window::swapBuffers()
{
	RECT r = { 0, 0, (LONG)clientWidth, (LONG)clientHeight };
	InvalidateRect(windowHandle, &r, TRUE);
	UpdateWindow(windowHandle);
}
