#pragma once

#include <dx/d3dx12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl.h> 
#include <sdkddkver.h>

// The newer SDK brings new APIs (e.g. mesh shaders), which can be enabled with this switch.
// You should still design your program with lower capabilities in mind and switch on certain 
// features at runtime.
// For example, wrap all your mesh shader code in a runtime if. The dx_context exposes the actual
// hardware capabilities.
#if defined(TURING_GPU_OR_NEWER_AVAILABLE) && defined(WINDOWS_SDK_19041_OR_NEWER_AVAILABLE) && defined(NTDDI_WIN10_19H1) && (WDK_NTDDI_VERSION >= NTDDI_WIN10_19H1)
#define ADVANCED_GPU_FEATURES_ENABLED 1
#else
#if defined(WINDOWS_SDK_19041_OR_NEWER_AVAILABLE) && (!defined(NTDDI_WIN10_19H1) || (WDK_NTDDI_VERSION < NTDDI_WIN10_19H1))
#warning Your Windows SDK is newer than your Windows OS.Consider updating your OS or there might be compatability issues.
#endif
#define ADVANCED_GPU_FEATURES_ENABLED 0
#endif


#define USE_D3D12_BLOCK_ALLOCATOR 0
namespace D3D12MA { class Allocator; class Allocation; };

template <typename T>
using com = Microsoft::WRL::ComPtr<T>;


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

#if ADVANCED_GPU_FEATURES_ENABLED
typedef com<ID3D12GraphicsCommandList6> dx_graphics_command_list;
#else
typedef com<ID3D12GraphicsCommandList4> dx_graphics_command_list;
#endif

#define NUM_BUFFERED_FRAMES 2

#define SET_NAME(obj, name) checkResult(obj->SetName(L##name));


