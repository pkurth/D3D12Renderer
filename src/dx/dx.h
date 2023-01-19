#pragma once

#include <dx/d3dx12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <sdkddkver.h>


#define USE_D3D12_BLOCK_ALLOCATOR 0
namespace D3D12MA { class Allocator; class Allocation; };

#ifdef __ID3D12GraphicsCommandList6_INTERFACE_DEFINED__
#define SDK_SUPPORTS_MESH_SHADERS
#endif


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
typedef com<ID3D12QueryHeap> dx_query_heap;

#ifdef SDK_SUPPORTS_MESH_SHADERS
typedef com<ID3D12GraphicsCommandList6> dx_graphics_command_list;
#else
typedef com<ID3D12GraphicsCommandList4> dx_graphics_command_list;
#endif

#define NUM_BUFFERED_FRAMES 2

#define SET_NAME(obj, name) checkResult(obj->SetName(L##name));


