#include "pch.h"
#include "system.h"
#include "string.h"

#include "dx/dx_context.h"

#include <intrin.h>

system_info getSystemInfo()
{
	system_info system;
	char cpuName[64] = "";

	int cpuInfo[4] = { -1 };
	__cpuid(cpuInfo, 0x80000000);
	uint32 nExIds = cpuInfo[0];
	// Get the information associated with each extended ID.
	for (uint32 i = 0x80000000; i <= nExIds; ++i)
	{
		__cpuid(cpuInfo, i);
		// Interpret CPU brand string and cache information.
		if (i == 0x80000002)
		{
			memcpy(cpuName,
				cpuInfo,
				sizeof(cpuInfo));
		}
		else if (i == 0x80000003)
		{
			memcpy(cpuName + 16,
				cpuInfo,
				sizeof(cpuInfo));
		}
		else if (i == 0x80000004)
		{
			memcpy(cpuName + 32, cpuInfo, sizeof(cpuInfo));
		}
	}

	MEMORYSTATUSEX memoryState;
	memoryState.dwLength = sizeof(memoryState);

	GlobalMemoryStatusEx(&memoryState);

	system.cpuName = cpuName;
	system.mainMemory = memoryState.ullTotalPhys;


	DXGI_ADAPTER_DESC1 gpuDesc;
	dxContext.adapter->GetDesc1(&gpuDesc);

	system.gpuName = wstringToString(gpuDesc.Description);
	system.videoMemory = gpuDesc.DedicatedVideoMemory;

	return system;
}
