#include "pch.h"
#include "dx_context.h"
#include "dx_command_list.h"
#include "dx_texture.h"
#include "dx_buffer.h"
#include "dx_profiling.h"
#include "core/string.h"

#include <d3d12memoryallocator/D3D12MemAlloc.cpp>

extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

// Allocate dynamically on the heap to ensure that this thing never gets destroyed before anything else. This is quite a hack.
dx_context& dxContext = *new dx_context{};


LONG NTAPI handleVectoredException(PEXCEPTION_POINTERS exceptionInfo)
{
	PEXCEPTION_RECORD exceptionRecord = exceptionInfo->ExceptionRecord;

	switch (exceptionRecord->ExceptionCode)
	{
		case DBG_PRINTEXCEPTION_WIDE_C:
		case DBG_PRINTEXCEPTION_C:

			if (exceptionRecord->NumberParameters >= 2)
			{
				ULONG len = (ULONG)exceptionRecord->ExceptionInformation[0];

				union
				{
					ULONG_PTR up;
					PCWSTR pwz;
					PCSTR psz;
				};

				up = exceptionRecord->ExceptionInformation[1];

				HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);

				if (exceptionRecord->ExceptionCode == DBG_PRINTEXCEPTION_C)
				{
					// Localized text will be incorrect displayed, if used not CP_OEMCP encoding.
					// WriteConsoleA(hOut, psz, len, &len, 0);

					// assume CP_ACP encoding
					if (ULONG n = MultiByteToWideChar(CP_ACP, 0, psz, len, 0, 0))
					{
						PWSTR wz = (PWSTR)alloca(n * sizeof(WCHAR));

						if (len = MultiByteToWideChar(CP_ACP, 0, psz, len, wz, n))
						{
							pwz = wz;
						}
					}
				}

				if (len)
				{
					WriteConsoleW(hOut, pwz, len - 1, &len, 0);
				}

			}
			return EXCEPTION_CONTINUE_EXECUTION;
	}

	return EXCEPTION_CONTINUE_SEARCH;
}

static void enableDebugLayer()
{
#if defined(_DEBUG)
	com<ID3D12Debug3> debugInterface;
	checkResult(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
	//debugInterface->SetEnableAutoDebugName(true);
#endif
}

static dx_factory createFactory()
{
	dx_factory dxgiFactory;
	uint32 createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	checkResult(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

	return dxgiFactory;
}

struct adapter_desc
{
	dx_adapter adapter;
	D3D_FEATURE_LEVEL featureLevel;
	std::string name;
};

static adapter_desc getAdapter(dx_factory factory, D3D_FEATURE_LEVEL minimumFeatureLevel = D3D_FEATURE_LEVEL_11_0)
{
	com<IDXGIAdapter1> dxgiAdapter1;
	dx_adapter dxgiAdapter;
	DXGI_ADAPTER_DESC1 desc;

	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_9_1;

	D3D_FEATURE_LEVEL possibleFeatureLevels[] = 
	{
		D3D_FEATURE_LEVEL_9_1,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_12_1
	};

	uint32 firstFeatureLevel = 0;
	for (uint32 i = 0; i < arraysize(possibleFeatureLevels); ++i)
	{
		if (possibleFeatureLevels[i] == minimumFeatureLevel)
		{
			firstFeatureLevel = i;
			break;
		}
	}

	uint64 maxDedicatedVideoMemory = 0;
	for (uint32 i = 0; factory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
		dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

		// Check to see if the adapter can create a D3D12 device without actually 
		// creating it. Out of all adapters which support the minimum feature level,
		// the adapter with the largest dedicated video memory is favored.
		if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
		{
			D3D_FEATURE_LEVEL adapterFeatureLevel = D3D_FEATURE_LEVEL_9_1;
			bool supportsFeatureLevel = false;

			for (uint32 fl = firstFeatureLevel; fl < arraysize(possibleFeatureLevels); ++fl)
			{
				if (SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(),
					possibleFeatureLevels[fl], __uuidof(ID3D12Device), 0)))
				{
					adapterFeatureLevel = possibleFeatureLevels[fl];
					supportsFeatureLevel = true;
				}
			}

			if (supportsFeatureLevel && dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				checkResult(dxgiAdapter1.As(&dxgiAdapter));
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				featureLevel = adapterFeatureLevel;
				desc = dxgiAdapterDesc1;
			}
		}
	}

	std::string gpuName = wstringToString(desc.Description);
	std::cout << "Using GPU: " << gpuName << '\n';

	return { dxgiAdapter, featureLevel, gpuName };
}

static dx_device createDevice(dx_adapter adapter, D3D_FEATURE_LEVEL featureLevel)
{
	dx_device device;
	checkResult(D3D12CreateDevice(adapter.Get(), featureLevel, IID_PPV_ARGS(&device)));

#if defined(_DEBUG)
	com<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(device.As(&infoQueue)))
	{
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		// Suppress whole categories of messages.
		//D3D12_MESSAGE_CATEGORY categories[] = {};

		// Suppress messages based on their severity level.
		D3D12_MESSAGE_SEVERITY severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID.
		D3D12_MESSAGE_ID ids[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_OBJECT_ACCESSED_WHILE_STILL_IN_USE,			// Disabled to allow OBS to capture the window.
		};

		D3D12_INFO_QUEUE_FILTER filter = {};
		//filter.DenyList.NumCategories = arraysize(categories);
		//filter.DenyList.pCategoryList = categories;
		filter.DenyList.NumSeverities = arraysize(severities);
		filter.DenyList.pSeverityList = severities;
		filter.DenyList.NumIDs = arraysize(ids);
		filter.DenyList.pIDList = ids;

		checkResult(infoQueue->PushStorageFilter(&filter));
	}
#endif

	return device;
}

static dx_feature_support checkFeatureSupport(dx_device device)
{
	dx_feature_support result = {};

	D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
	if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5))))
	{
		result.raytracingTier = (dx_raytracing_tier)options5.RaytracingTier;
	}

#if ADVANCED_GPU_FEATURES_ENABLED
	D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
	if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7))))
	{
		result.meshShaderTier = (dx_mesh_shader_tier)options7.MeshShaderTier;
	}
	else
	{
		std::cerr << "Checking support for mesh shader feature failed. Maybe you need to update your Windows version.\n";
	}
#endif

	return result;
}

bool dx_context::initialize()
{
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	AddVectoredExceptionHandler(TRUE, handleVectoredException);

	checkResult(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));


	enableDebugLayer();

	factory = createFactory();
	adapter_desc adapterDesc = getAdapter(factory);
	this->adapter = adapterDesc.adapter;
	if (!adapter)
	{
		std::cerr << "No DX12 capable GPU found.\n";
		return false;
	}

	device = createDevice(adapter, adapterDesc.featureLevel);

#if USE_D3D12_BLOCK_ALLOCATOR
	D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
	allocatorDesc.pDevice = device.Get();
	allocatorDesc.pAdapter = adapter.Get();
	checkResult(D3D12MA::CreateAllocator(&allocatorDesc, &memoryAllocator));
#endif

	featureSupport = checkFeatureSupport(device);

	bufferedFrameID = NUM_BUFFERED_FRAMES - 1;

	descriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	srvUavAllocator.initialize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, false, 4096);
	srvUavAllocatorShaderVisible.initialize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true, 128);
	rtvAllocator.initialize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false, 1024);
	dsvAllocator.initialize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false, 1024);
	frameDescriptorAllocator.initialize();

	for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; ++i)
	{
		pagePools[i].initialize(MB(2));

#if ENABLE_DX_PROFILING
		timestampHeaps[i].initialize(MAX_NUM_DX_PROFILE_EVENTS);
		resolvedTimestampBuffers[i] = createReadbackBuffer(sizeof(uint64), MAX_NUM_DX_PROFILE_EVENTS);
#endif
	}

	frameUploadBuffer.reset();
	frameUploadBuffer.pagePool = &pagePools[bufferedFrameID];

	pagePools[bufferedFrameID].reset();


	renderQueue.initialize(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	computeQueue.initialize(device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	copyQueue.initialize(device, D3D12_COMMAND_LIST_TYPE_COPY);


	D3D12_INDIRECT_ARGUMENT_DESC argumentDesc;
	argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
	defaultDispatchCommandSignature = createCommandSignature({}, &argumentDesc, 1, sizeof(D3D12_DISPATCH_ARGUMENTS));

	return true;
}

void dx_context::flushApplication()
{
	renderQueue.flush();
	computeQueue.flush();
	copyQueue.flush();
}

void dx_context::quit()
{
#if ENABLE_DX_PROFILING
	for (uint32 b = 0; b < NUM_BUFFERED_FRAMES; ++b)
	{
		resolvedTimestampBuffers[b].reset();
	}
#endif

	running = false;
	flushApplication();
	WaitForSingleObject(renderQueue.processThreadHandle, INFINITE);
	WaitForSingleObject(computeQueue.processThreadHandle, INFINITE);
	WaitForSingleObject(copyQueue.processThreadHandle, INFINITE);

	for (uint32 b = 0; b < NUM_BUFFERED_FRAMES; ++b)
	{
		textureGraveyard[b].clear();
		bufferGraveyard[b].clear();
		objectGraveyard[b].clear();

		for (auto allocation : allocationGraveyard[b])
		{
			allocation->Release();
		}
		allocationGraveyard[b].clear();
	}
}

void dx_context::retire(texture_grave&& texture)
{
	mutex.lock();
	textureGraveyard[bufferedFrameID].push_back(std::move(texture));
	mutex.unlock();
}

void dx_context::retire(buffer_grave&& buffer)
{
	mutex.lock();
	bufferGraveyard[bufferedFrameID].push_back(std::move(buffer));
	mutex.unlock();
}

void dx_context::retire(dx_object obj)
{
	mutex.lock();
	objectGraveyard[bufferedFrameID].push_back(obj);
	mutex.unlock();
}

void dx_context::retire(D3D12MA::Allocation* allocation)
{
	mutex.lock();
	allocationGraveyard[bufferedFrameID].push_back(allocation);
	mutex.unlock();
}

dx_command_queue& dx_context::getQueue(D3D12_COMMAND_LIST_TYPE type)
{
	return type == D3D12_COMMAND_LIST_TYPE_DIRECT ? renderQueue :
		type == D3D12_COMMAND_LIST_TYPE_COMPUTE ? computeQueue :
		copyQueue;
}

dx_command_list* dx_context::getFreeCommandList(dx_command_queue& queue)
{
	queue.commandListMutex.lock();
	dx_command_list* result = queue.freeCommandLists;
	if (result)
	{
		queue.freeCommandLists = result->next;
	}
	queue.commandListMutex.unlock();

	if (!result)
	{
		result = new dx_command_list(queue.commandListType);
		atomicIncrement(queue.totalNumCommandLists);
	}

#if ENABLE_DX_PROFILING
	result->timeStampQueryHeap = timestampHeaps[bufferedFrameID].heap;
#endif

	return result;
}

dx_command_list* dx_context::getFreeCopyCommandList()
{
	return getFreeCommandList(copyQueue);
}

dx_command_list* dx_context::getFreeComputeCommandList(bool async)
{
	return getFreeCommandList(async ? computeQueue : renderQueue);
}

dx_command_list* dx_context::getFreeRenderCommandList()
{
	dx_command_list* cl = getFreeCommandList(renderQueue);
	CD3DX12_RECT scissorRect(0, 0, LONG_MAX, LONG_MAX);
	cl->setScissor(scissorRect);
	return cl;
}

static void enqueueRunningCommandList(dx_command_queue& queue, dx_command_list* commandList)
{
	commandList->next = 0;
	if (queue.newestRunningCommandList)
	{
		assert(queue.newestRunningCommandList->next == 0);
		queue.newestRunningCommandList->next = commandList;
	}
	queue.newestRunningCommandList = commandList;
	if (!queue.runningCommandLists)
	{
		queue.runningCommandLists = commandList;
	}
}

uint64 dx_context::executeCommandList(dx_command_list* commandList)
{
	dx_command_queue& queue = getQueue(commandList->type);

	checkResult(commandList->commandList->Close());

	ID3D12CommandList* d3d12List = commandList->commandList.Get();
	queue.commandQueue->ExecuteCommandLists(1, &d3d12List);

	uint64 fenceValue = queue.signal();

	commandList->lastExecutionFenceValue = fenceValue;

	queue.commandListMutex.lock();
	enqueueRunningCommandList(queue, commandList);
	atomicIncrement(queue.numRunningCommandLists);
	queue.commandListMutex.unlock();

	return fenceValue;
}

uint64 dx_context::executeCommandLists(dx_command_list** commandLists, uint32 count)
{
	dx_command_queue& queue = getQueue(commandLists[0]->type);

	ID3D12CommandList* d3d12Lists[16];

	for (uint32 i = 0; i < count; ++i)
	{
		checkResult(commandLists[i]->commandList->Close());
		d3d12Lists[i] = commandLists[i]->commandList.Get();
	}

	queue.commandQueue->ExecuteCommandLists(count, d3d12Lists);

	uint64 fenceValue = queue.signal();

	for (uint32 i = 0; i < count; ++i)
	{
		commandLists[i]->lastExecutionFenceValue = fenceValue;
	}

	queue.commandListMutex.lock();
	for (uint32 i = 0; i < count; ++i)
	{
		enqueueRunningCommandList(queue, commandLists[i]);
	}
	atomicAdd(queue.numRunningCommandLists, count);

	queue.commandListMutex.unlock();

	return fenceValue;
}

dx_allocation dx_context::allocateDynamicBuffer(uint32 sizeInBytes, uint32 alignment)
{
	dx_allocation allocation = frameUploadBuffer.allocate(sizeInBytes, alignment);
	return allocation;
}

std::pair<dx_dynamic_constant_buffer, void*> dx_context::uploadDynamicConstantBuffer(uint32 sizeInBytes, const void* data)
{
	dx_allocation allocation = allocateDynamicBuffer(sizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	memcpy(allocation.cpuPtr, data, sizeInBytes);
	return { { allocation.gpuPtr }, allocation.cpuPtr };
}

std::pair<dx_dynamic_vertex_buffer, void*> dx_context::createDynamicVertexBuffer(uint32 elementSize, uint32 elementCount)
{
	uint32 sizeInBytes = elementSize * elementCount;
	dx_allocation allocation = allocateDynamicBuffer(sizeInBytes, elementSize);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
	vertexBufferView.BufferLocation = allocation.gpuPtr;
	vertexBufferView.SizeInBytes = sizeInBytes;
	vertexBufferView.StrideInBytes = elementSize;

	return { { vertexBufferView }, allocation.cpuPtr };
}

std::pair<dx_dynamic_index_buffer, void*> dx_context::createDynamicIndexBuffer(uint32 elementSize, uint32 elementCount)
{
	uint32 sizeInBytes = elementSize * elementCount;
	dx_allocation allocation = allocateDynamicBuffer(sizeInBytes, elementSize);

	D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
	indexBufferView.BufferLocation = allocation.gpuPtr;
	indexBufferView.SizeInBytes = sizeInBytes;
	indexBufferView.Format = getIndexBufferFormat(elementSize);

	return { { indexBufferView }, allocation.cpuPtr };
}

dx_memory_usage dx_context::getMemoryUsage()
{
	DXGI_QUERY_VIDEO_MEMORY_INFO memoryInfo;
	checkResult(dxContext.adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memoryInfo));
	
	return { (uint32)BYTE_TO_MB(memoryInfo.CurrentUsage), (uint32)BYTE_TO_MB(memoryInfo.Budget) };
}

void dx_context::endFrame(dx_command_list* cl)
{
#if ENABLE_DX_PROFILING
	dxProfilingFrameEndMarker(cl);
	cl->commandList->ResolveQueryData(timestampHeaps[bufferedFrameID].heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, timestampQueryIndex[bufferedFrameID], resolvedTimestampBuffers[bufferedFrameID]->resource.Get(), 0);
#endif
}

void dx_context::newFrame(uint64 frameID)
{
	this->frameID = frameID;

	mutex.lock();
	bufferedFrameID = (uint32)(frameID % NUM_BUFFERED_FRAMES);

#if ENABLE_DX_PROFILING
	uint64* timestamps = (uint64*)mapBuffer(resolvedTimestampBuffers[bufferedFrameID], true);
	dxProfilingResolveTimeStamps(timestamps);
	unmapBuffer(resolvedTimestampBuffers[bufferedFrameID], false);

	timestampQueryIndex[bufferedFrameID] = 0;
#endif


	textureGraveyard[bufferedFrameID].clear();
	bufferGraveyard[bufferedFrameID].clear();
	objectGraveyard[bufferedFrameID].clear();
	for (auto allocation : allocationGraveyard[bufferedFrameID])
	{
		allocation->Release();
	}
	allocationGraveyard[bufferedFrameID].clear();


	frameUploadBuffer.reset();
	frameUploadBuffer.pagePool = &pagePools[bufferedFrameID];

	pagePools[bufferedFrameID].reset();
	frameDescriptorAllocator.newFrame(bufferedFrameID);

	mutex.unlock();
}


