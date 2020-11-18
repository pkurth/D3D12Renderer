#pragma once

#include "dx.h"
#include "threading.h"

struct dx_command_list;

struct dx_command_queue
{
	void initialize(dx_device device, D3D12_COMMAND_LIST_TYPE type);

	uint64 signal();
	bool isFenceComplete(uint64 fenceValue);
	void waitForFence(uint64 fenceValue);
	void waitForOtherQueue(dx_command_queue& other);
	void flush();


	D3D12_COMMAND_LIST_TYPE commandListType;
	com<ID3D12CommandQueue>	commandQueue;
	com<ID3D12Fence> fence;
	volatile uint64 fenceValue;

	dx_command_list* runningCommandLists;
	dx_command_list* freeCommandLists;

	volatile uint32 numRunningCommandLists;
	volatile uint32 totalNumCommandLists; // Used only for validation.

	HANDLE processThreadHandle;

	thread_mutex commandListMutex;
};
