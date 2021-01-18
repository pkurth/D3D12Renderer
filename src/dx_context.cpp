#include "pch.h"
#include "dx_context.h"
#include "dx_command_list.h"
#include "dx_texture.h"
#include "dx_buffer.h"

extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

dx_context dxContext;

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

static std::pair<dx_adapter, D3D_FEATURE_LEVEL> getAdapter(dx_factory factory, D3D_FEATURE_LEVEL minimumFeatureLevel = D3D_FEATURE_LEVEL_11_0)
{
	com<IDXGIAdapter1> dxgiAdapter1;
	dx_adapter dxgiAdapter;

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
			}
		}
	}

	return { dxgiAdapter, featureLevel };
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

		// Suppress whole categories of messages
		//D3D12_MESSAGE_CATEGORY categories[] = {};

		// Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID ids[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
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

static bool checkRaytracingSupport(dx_device device)
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
	checkResult(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));
	return options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
}

static bool checkMeshShaderSupport(dx_device device)
{
#if ADVANCED_GPU_FEATURES_ENABLED
	D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
	checkResult(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7)));
	return options7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1;
#else
	return false;
#endif
}

bool dx_context::initialize()
{
	enableDebugLayer();

	factory = createFactory();
	auto [adapter, featureLevel] = getAdapter(factory);
	if (!adapter)
	{
		std::cerr << "No DX12 capable GPU found." << std::endl;
		return false;
	}
	this->adapter = adapter;

	device = createDevice(adapter, featureLevel);
	raytracingSupported = checkRaytracingSupport(device);
	meshShaderSupported = checkMeshShaderSupport(device);

	bufferedFrameID = NUM_BUFFERED_FRAMES - 1;

	for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; ++i)
	{
		pagePools[i].initialize(MB(2));
	}

	renderQueue.initialize(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	computeQueue.initialize(device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	copyQueue.initialize(device, D3D12_COMMAND_LIST_TYPE_COPY);

	descriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	descriptorAllocatorCPU.initialize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096, false);
	descriptorAllocatorGPU.initialize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
	rtvAllocator.initialize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1024, false);
	dsvAllocator.initialize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1024, false);
	frameDescriptorAllocator.initialize();

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
	running = false;
	flushApplication();
	WaitForSingleObject(renderQueue.processThreadHandle, INFINITE);
	WaitForSingleObject(computeQueue.processThreadHandle, INFINITE);
	WaitForSingleObject(copyQueue.processThreadHandle, INFINITE);

	for (uint32 b = 0; b < NUM_BUFFERED_FRAMES; ++b)
	{
		textureGraveyard[b].clear();
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

	mutex.lock();
	result->uploadBuffer.pagePool = &pagePools[bufferedFrameID];
	mutex.unlock();

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

uint64 dx_context::executeCommandList(dx_command_list* commandList)
{
	dx_command_queue& queue = getQueue(commandList->type);

	checkResult(commandList->commandList->Close());

	ID3D12CommandList* d3d12List = commandList->commandList.Get();
	queue.commandQueue->ExecuteCommandLists(1, &d3d12List);

	uint64 fenceValue = queue.signal();

	commandList->lastExecutionFenceValue = fenceValue;

	queue.commandListMutex.lock();

	commandList->next = queue.runningCommandLists;
	queue.runningCommandLists = commandList;
	atomicIncrement(queue.numRunningCommandLists);

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

dx_memory_usage dx_context::getMemoryUsage()
{
	DXGI_QUERY_VIDEO_MEMORY_INFO memoryInfo;
	checkResult(dxContext.adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memoryInfo));
	
	return { (uint32)BYTE_TO_MB(memoryInfo.CurrentUsage), (uint32)BYTE_TO_MB(memoryInfo.Budget) };
}

void dx_context::newFrame(uint64 frameID)
{
	this->frameID = frameID;

	mutex.lock();
	bufferedFrameID = (uint32)(frameID % NUM_BUFFERED_FRAMES);

	textureGraveyard[bufferedFrameID].clear();
	bufferGraveyard[bufferedFrameID].clear();
	objectGraveyard[bufferedFrameID].clear();


	frameUploadBuffer.reset();
	frameUploadBuffer.pagePool = &pagePools[bufferedFrameID];

	pagePools[bufferedFrameID].reset();
	frameDescriptorAllocator.newFrame(bufferedFrameID);

	mutex.unlock();
}


