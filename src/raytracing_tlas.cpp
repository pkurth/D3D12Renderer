#include "pch.h"
#include "raytracing_tlas.h"

#include "dx_command_list.h"
#include "dx_barrier_batcher.h"

void raytracing_tlas::initialize(acceleration_structure_rebuild_mode rebuildMode)
{
    this->rebuildMode = rebuildMode;
    allInstances.reserve(4096); // TODO

    for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; ++i)
    {
        tlasSRVs[i] = dxContext.descriptorAllocatorCPU.getFreeHandle();
    }
}

raytracing_instance_handle raytracing_tlas::instantiate(raytracing_object_handle type, const trs& transform)
{
    D3D12_RAYTRACING_INSTANCE_DESC instance;

    instance.Flags = 0;// D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
    instance.InstanceContributionToHitGroupIndex = type.instanceContributionToHitGroupIndex;

    mat4 m = transpose(trsToMat4(transform));
    memcpy(instance.Transform, &m, sizeof(instance.Transform));
    instance.AccelerationStructure = type.blas;
    instance.InstanceMask = 0xFF;
    instance.InstanceID = 0; // This value will be exposed to the shader via InstanceID().

    uint32 result = (uint32)allInstances.size();
    allInstances.push_back(instance);

    return { result };
}

void raytracing_tlas::updateInstanceTransform(raytracing_instance_handle handle, const trs& transform)
{
    D3D12_RAYTRACING_INSTANCE_DESC& instance = allInstances[handle.instanceIndex];
    mat4 m = transpose(trsToMat4(transform));
    memcpy(instance.Transform, &m, sizeof(instance.Transform));
}

void raytracing_tlas::build()
{
    uint32 totalNumInstances = (uint32)allInstances.size();

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    inputs.NumDescs = totalNumInstances;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    if (rebuildMode == acceleration_structure_refit)
    {
        inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    }

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
    dxContext.device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);
    info.ScratchDataSizeInBytes = alignTo(info.ScratchDataSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    info.ResultDataMaxSizeInBytes = alignTo(info.ResultDataMaxSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    bool fromScratch = false;

    // Allocate.
    if (!tlas || tlas->totalSize < info.ResultDataMaxSizeInBytes)
    {
        if (tlas)
        {
            resizeBuffer(tlas, (uint32)info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
        }
        else
        {
            tlas = createBuffer(1, (uint32)info.ResultDataMaxSizeInBytes, 0, true, false, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
            SET_NAME(tlas->resource, "TLAS Result");
        }

        fromScratch = true;
    }

    if (!scratch || scratch->totalSize < info.ScratchDataSizeInBytes)
    {
        if (scratch)
        {
            resizeBuffer(scratch, (uint32)info.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }
        else
        {
            scratch = createBuffer(1, (uint32)info.ScratchDataSizeInBytes, 0, true, false, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            SET_NAME(scratch->resource, "TLAS Scratch");
        }
    }



    dx_command_list* cl = dxContext.getFreeRenderCommandList();
    dx_dynamic_constant_buffer gpuInstances = cl->uploadDynamicConstantBuffer(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * totalNumInstances, allInstances.data());

    inputs.InstanceDescs = gpuInstances.gpuPtr;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
    asDesc.Inputs = inputs;
    asDesc.DestAccelerationStructureData = tlas->gpuVirtualAddress;
    asDesc.ScratchAccelerationStructureData = scratch->gpuVirtualAddress;

    if (!fromScratch)
    {
        barrier_batcher(cl)
            .uav(tlas)
            .uav(scratch);

        if (rebuildMode == acceleration_structure_refit)
        {
            asDesc.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
            asDesc.SourceAccelerationStructureData = tlas->gpuVirtualAddress;
        }
    }


    cl->commandList->BuildRaytracingAccelerationStructure(&asDesc, 0, 0);
    cl->uavBarrier(tlas);

    dxContext.executeCommandList(cl);

    tlasDescriptorIndex = (tlasDescriptorIndex + 1) % NUM_BUFFERED_FRAMES;
    tlasSRVs[tlasDescriptorIndex].createRaytracingAccelerationStructureSRV(tlas);
}
