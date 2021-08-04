#include "pch.h"
#include "raytracer.h"


void dx_raytracer::fillOutRayTracingRenderDesc(const ref<dx_buffer>& bindingTableBuffer,
    D3D12_DISPATCH_RAYS_DESC& raytraceDesc,
    uint32 renderWidth, uint32 renderHeight, uint32 renderDepth,
    uint32 numRayTypes, uint32 numHitGroups)
{
    raytraceDesc.Width = renderWidth;
    raytraceDesc.Height = renderHeight;
    raytraceDesc.Depth = renderDepth;

    uint32 numHitShaders = numHitGroups * numRayTypes;

    // Pointer to the entry point of the ray-generation shader.
    raytraceDesc.RayGenerationShaderRecord.StartAddress = bindingTableBuffer->gpuVirtualAddress + pipeline.shaderBindingTableDesc.raygenOffset;
    raytraceDesc.RayGenerationShaderRecord.SizeInBytes = pipeline.shaderBindingTableDesc.entrySize;

    // Pointer to the entry point(s) of the miss shader.
    raytraceDesc.MissShaderTable.StartAddress = bindingTableBuffer->gpuVirtualAddress + pipeline.shaderBindingTableDesc.missOffset;
    raytraceDesc.MissShaderTable.StrideInBytes = pipeline.shaderBindingTableDesc.entrySize;
    raytraceDesc.MissShaderTable.SizeInBytes = pipeline.shaderBindingTableDesc.entrySize * numRayTypes;

    // Pointer to the entry point(s) of the hit shader.
    raytraceDesc.HitGroupTable.StartAddress = bindingTableBuffer->gpuVirtualAddress + pipeline.shaderBindingTableDesc.hitOffset;
    raytraceDesc.HitGroupTable.StrideInBytes = pipeline.shaderBindingTableDesc.entrySize;
    raytraceDesc.HitGroupTable.SizeInBytes = pipeline.shaderBindingTableDesc.entrySize * numHitShaders;

    raytraceDesc.CallableShaderTable = {};
}
