#include "pch.h"
#include "dx_command_queue.h"
#include "dx_command_list.h"
#include "dx_context.h"
#include "threading.h"

static DWORD processRunningCommandAllocators(void* data);

void dx_command_queue::initialize(dx_device device, D3D12_COMMAND_LIST_TYPE type)
{
	fenceValue = 0;
	commandListType = type;

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	checkResult(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&commandQueue)));
	checkResult(device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

	timeStampFrequency = 0; // Default value, if timing is not supported on this queue.
	if (SUCCEEDED(commandQueue->GetTimestampFrequency(&timeStampFrequency)))
	{
		// TODO: Calibrate command queue time line with CPU.
	}

	processThreadHandle = CreateThread(0, 0, processRunningCommandAllocators, this, 0, 0);

	switch (type)
	{
		case D3D12_COMMAND_LIST_TYPE_DIRECT: SET_NAME(commandQueue, "Render command queue"); break;
		case D3D12_COMMAND_LIST_TYPE_COMPUTE: SET_NAME(commandQueue, "Compute command queue"); break;
		case D3D12_COMMAND_LIST_TYPE_COPY: SET_NAME(commandQueue, "Copy command queue"); break;
	}
}

uint64 dx_command_queue::signal()
{
	uint64 fenceValueForSignal = atomicIncrement(fenceValue) + 1;
	checkResult(commandQueue->Signal(fence.Get(), fenceValueForSignal));

	return fenceValueForSignal;
}

bool dx_command_queue::isFenceComplete(uint64 fenceValue)
{
	return fence->GetCompletedValue() >= fenceValue;
}

void dx_command_queue::waitForFence(uint64 fenceValue)
{
	if (!isFenceComplete(fenceValue))
	{
		HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		assert(fenceEvent && "Failed to create fence event handle.");

		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, DWORD_MAX);

		CloseHandle(fenceEvent);
	}
}

void dx_command_queue::waitForOtherQueue(dx_command_queue& other)
{
	commandQueue->Wait(other.fence.Get(), other.signal());
}

void dx_command_queue::flush()
{
	while (numRunningCommandLists) {}

	waitForFence(signal());
}

static DWORD processRunningCommandAllocators(void* data)
{
	dx_command_queue& queue = *(dx_command_queue*)data;

	while (dxContext.running)
	{
		while (true)
		{
			queue.commandListMutex.lock();
			dx_command_list* list = queue.runningCommandLists;
			if (list)
			{
				queue.runningCommandLists = list->next;
			}
			queue.commandListMutex.unlock();

			if (list)
			{
				queue.waitForFence(list->lastExecutionFenceValue);
				list->reset();

				queue.commandListMutex.lock();
				list->next = queue.freeCommandLists;
				queue.freeCommandLists = list;
				atomicDecrement(queue.numRunningCommandLists);
				queue.commandListMutex.unlock();
			}
			else
			{
				break;
			}
		}

		SwitchToThread(); // Yield.
	}

	return 0;
}

