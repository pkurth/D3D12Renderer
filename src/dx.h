#pragma once

#include <dx/d3dx12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl.h> 

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
typedef com<ID3D12GraphicsCommandList4> dx_graphics_command_list;
typedef com<ID3DBlob> dx_blob;
typedef com<ID3D12RootSignature> dx_root_signature;
typedef com<ID3D12PipelineState> dx_pipeline_state;
typedef com<ID3D12Resource> dx_resource;
typedef com<ID3D12CommandSignature> dx_command_signature;
typedef com<ID3D12Heap> dx_heap;
typedef com<ID3D12StateObject> dx_raytracing_pipeline_state;

#define NUM_BUFFERED_FRAMES 2

#define SET_NAME(obj, name) checkResult(obj->SetName(L##name));

extern struct dx_context dxContext;

enum color_depth
{
	color_depth_8,
	color_depth_10,
};

