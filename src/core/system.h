#pragma once

struct system_info
{
	std::string cpuName;
	std::string gpuName;

	// In bytes.
	uint64 mainMemory;
	uint64 videoMemory;
};

system_info getSystemInfo();

