#pragma once

#include <dx/d3dx12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl.h> 
#include <sdkddkver.h>

// The newer SDK brings new APIs (e.g. mesh shaders), which can be enabled with this switch.
// However, you still have to check at runtime for mesh shader support, because older devices
// can have the newest SDK installed as well.
#if WDK_NTDDI_VERSION >= NTDDI_WIN10_19H1
#define WIN_SDK_VERSION_2019 1
#else
#define WIN_SDK_VERSION_2019 0
#endif


template <typename T>
using com = Microsoft::WRL::ComPtr<T>;

static void checkResult(HRESULT hr)
{
	assert(SUCCEEDED(hr));
}

typedef com<ID3D12Object> dx_object;
typedef com<IDXGIAdapter4> dx_adapter;
typedef com<ID3D12Device5> dx_device;
typedef com<IDXGIFactory4> dx_factory;
typedef com<IDXGISwapChain4> dx_swapchain;
typedef com<ID3D12Resource> dx_resource;
typedef com<ID3D12CommandAllocator> dx_command_allocator;
typedef com<ID3DBlob> dx_blob;
typedef com<ID3D12PipelineState> dx_pipeline_state;
typedef com<ID3D12Resource> dx_resource;
typedef com<ID3D12CommandSignature> dx_command_signature;
typedef com<ID3D12Heap> dx_heap;
typedef com<ID3D12StateObject> dx_raytracing_pipeline_state;

#if WIN_SDK_VERSION_2019
typedef com<ID3D12GraphicsCommandList6> dx_graphics_command_list;
#else
typedef com<ID3D12GraphicsCommandList4> dx_graphics_command_list;
#endif

#define NUM_BUFFERED_FRAMES 2

#define SET_NAME(obj, name) checkResult(obj->SetName(L##name));

extern struct dx_context dxContext;

enum color_depth
{
	color_depth_8,
	color_depth_10,
};

static DXGI_FORMAT getScreenFormat(color_depth colorDepth)
{
	return (colorDepth == color_depth_8) ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R10G10B10A2_UNORM;
}

