#include "pch.h"
#include "dx_context.h"
#include "dx_command_list.h"


dx_context dxContext;

static void enableDebugLayer()
{
#if defined(_DEBUG)
	com<ID3D12Debug> debugInterface;
	checkResult(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
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

static dx_adapter getAdapter(dx_factory factory)
{
	com<IDXGIAdapter1> dxgiAdapter1;
	dx_adapter dxgiAdapter;

	size_t maxDedicatedVideoMemory = 0;
	for (uint32 i = 0; factory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
		dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

		// Check to see if the adapter can create a D3D12 device without actually 
		// creating it. The adapter with the largest dedicated video memory
		// is favored.
		if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
			SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(),
				D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device), 0)) &&
			dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
		{
			maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
			checkResult(dxgiAdapter1.As(&dxgiAdapter));
		}
	}

	return dxgiAdapter;
}

static dx_device createDevice(dx_adapter adapter)
{
	dx_device device;
	checkResult(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device)));

#if defined(_DEBUG)
	com<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(device.As(&infoQueue)))
	{
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		// Suppress whole categories of messages
		//D3D12_MESSAGE_CATEGORY Categories[] = {};

		// Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID denyIDs[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
		};

		D3D12_INFO_QUEUE_FILTER newFilter = {};
		//NewFilter.DenyList.NumCategories = arraysize(Categories);
		//NewFilter.DenyList.pCategoryList = Categories;
		newFilter.DenyList.NumSeverities = arraysize(severities);
		newFilter.DenyList.pSeverityList = severities;
		newFilter.DenyList.NumIDs = arraysize(denyIDs);
		newFilter.DenyList.pIDList = denyIDs;

		checkResult(infoQueue->PushStorageFilter(&newFilter));
	}
#endif

	return device;
}

static bool checkRaytracingSupport(dx_device device)
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
	checkResult(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));
	return options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
}

static bool checkMeshShaderSupport(dx_device device)
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
	checkResult(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7)));
	return options7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1;
}


dx_page_pool createPagePool(dx_device device, uint64 pageSize);

void dx_context::initialize()
{
	enableDebugLayer();

	factory = createFactory();
	adapter = getAdapter(factory);
	device = createDevice(adapter);
	raytracingSupported = checkRaytracingSupport(device);
	meshShaderSupported = checkMeshShaderSupport(device);

	arena.minimumBlockSize = MB(2);
	allocationMutex = createMutex();
	bufferedFrameID = NUM_BUFFERED_FRAMES - 1;

	for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; ++i)
	{
		pagePools[i] = createPagePool(device, MB(2));
	}

	renderQueue.initialize(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	computeQueue.initialize(device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	copyQueue.initialize(device, D3D12_COMMAND_LIST_TYPE_COPY);


	descriptorAllocatorCPU.initialize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096, false);
	descriptorAllocatorGPU.initialize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
	rtvAllocator.initialize(1024, false);
	dsvAllocator.initialize(1024, false);
	frameDescriptorAllocator.initialize();
}

void dx_context::flushApplication()
{
	renderQueue.flush();
	computeQueue.flush();
	copyQueue.flush();
}

void dx_context::quit()
{
	running = false;
	flushApplication();
	WaitForSingleObject(renderQueue.processThreadHandle, INFINITE);
	WaitForSingleObject(computeQueue.processThreadHandle, INFINITE);
	WaitForSingleObject(copyQueue.processThreadHandle, INFINITE);
}

dx_command_list* dx_context::allocateCommandList(D3D12_COMMAND_LIST_TYPE type)
{
	allocationMutex.lock();
	dx_command_list* result = (dx_command_list*)arena.allocate(sizeof(dx_command_list), true);
	allocationMutex.unlock();

	new (result) dx_command_list();

	result->type = type;

	dx_command_allocator* allocator = getFreeCommandAllocator(type);
	result->commandAllocator = allocator;

	checkResult(device->CreateCommandList(0, type, allocator->commandAllocator.Get(), 0, IID_PPV_ARGS(&result->commandList)));

	result->dynamicDescriptorHeap.initialize();

	return result;
}

dx_command_allocator* dx_context::allocateCommandAllocator(D3D12_COMMAND_LIST_TYPE type)
{
	allocationMutex.lock();
	dx_command_allocator* result = (dx_command_allocator*)arena.allocate(sizeof(dx_command_allocator), true);
	allocationMutex.unlock();

	checkResult(device->CreateCommandAllocator(type, IID_PPV_ARGS(&result->commandAllocator)));

	return result;
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
		result = allocateCommandList(queue.commandListType);
	}
	else
	{
		dx_command_allocator* allocator = getFreeCommandAllocator(queue.commandListType);
		result->reset(allocator);
	}

	result->usedLastOnFrame = frameID;
	result->uploadBuffer.pagePool = &pagePools[bufferedFrameID];

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

dx_command_allocator* dx_context::getFreeCommandAllocator(dx_command_queue& queue)
{
	queue.commandListMutex.lock();
	dx_command_allocator* result = queue.freeCommandAllocators;
	if (result)
	{
		queue.freeCommandAllocators = result->next;
	}
	queue.commandListMutex.unlock();

	if (!result)
	{
		result = allocateCommandAllocator(queue.commandListType);
	}
	return result;
}

dx_command_allocator* dx_context::getFreeCommandAllocator(D3D12_COMMAND_LIST_TYPE type)
{
	dx_command_queue& queue = getQueue(type);
	return getFreeCommandAllocator(queue);
}

uint64 dx_context::executeCommandList(dx_command_list* commandList)
{
	dx_command_queue& queue = getQueue(commandList->type);

	checkResult(commandList->commandList->Close());

	ID3D12CommandList* d3d12List = commandList->commandList.Get();
	queue.commandQueue->ExecuteCommandLists(1, &d3d12List);

	uint64 fenceValue = queue.signal();

	dx_command_allocator* allocator = commandList->commandAllocator;
	allocator->lastExecutionFenceValue = fenceValue;

	queue.commandListMutex.lock();

	allocator->next = queue.runningCommandAllocators;
	queue.runningCommandAllocators = allocator;
	atomicIncrement(queue.numRunningCommandAllocators);

	commandList->next = queue.freeCommandLists;
	queue.freeCommandLists = commandList;

	queue.commandListMutex.unlock();

	return fenceValue;
}

dx_allocation dx_context::allocateDynamicBuffer(uint32 sizeInBytes, uint32 alignment)
{
	dx_allocation allocation = frameUploadBuffer.allocate(sizeInBytes, alignment);
	return allocation;
}

dx_dynamic_constant_buffer dx_context::uploadDynamicConstantBuffer(uint32 sizeInBytes, const void* data)
{
	dx_allocation allocation = allocateDynamicBuffer(sizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	memcpy(allocation.cpuPtr, data, sizeInBytes);
	return { allocation.gpuPtr, allocation.cpuPtr };
}

void dx_context::retireObject(dx_object obj)
{
	if (obj)
	{
		uint32 index = atomicIncrement(objectRetirement.numRetiredObjects[bufferedFrameID]);
		assert(!objectRetirement.retiredObjects[bufferedFrameID][index]);
		objectRetirement.retiredObjects[bufferedFrameID][index] = obj;
	}
}

void dx_context::newFrame(uint64 frameID)
{
	this->frameID = frameID;

	bufferedFrameID = (uint32)(frameID % NUM_BUFFERED_FRAMES);
	for (uint32 i = 0; i < objectRetirement.numRetiredObjects[bufferedFrameID]; ++i)
	{
		objectRetirement.retiredObjects[bufferedFrameID][i].Reset();
	}
	objectRetirement.numRetiredObjects[bufferedFrameID] = 0;

	frameUploadBuffer.reset();
	frameUploadBuffer.pagePool = &pagePools[bufferedFrameID];

	pagePools[bufferedFrameID].reset();
	frameDescriptorAllocator.newFrame(bufferedFrameID);
}


