#include "pch.h"
#include "raytracing_tlas.h"

#include "dx/dx_command_list.h"
#include "dx/dx_barrier_batcher.h"

void raytracing_tlas::initialize(raytracing_as_rebuild_mode rebuildMode)
{
    this->rebuildMode = rebuildMode;
    allInstances.reserve(4096); // TODO
}

void raytracing_tlas::reset()
{
    allInstances.clear();
}

raytracing_instance_handle raytracing_tlas::instantiate(raytracing_object_type type, const trs& transform)
{
    uint32 result = (uint32)allInstances.size();
    D3D12_RAYTRACING_INSTANCE_DESC& instance = allInstances.emplace_back();

    instance.Flags = 0;// D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
    instance.InstanceContributionToHitGroupIndex = type.instanceContributionToHitGroupIndex;

    mat4 m = transpose(trsToMat4(transform));
    memcpy(instance.Transform, &m, sizeof(instance.Transform));
    instance.AccelerationStructure = type.blas->blas->gpuVirtualAddress;
    instance.InstanceMask = 0xFF;
    instance.InstanceID = 0; // This value will be exposed to the shader via InstanceID().

    return { result };
}

void raytracing_tlas::build(dx_command_list* cl)
{
    uint32 totalNumInstances = (uint32)allInstances.size();

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    inputs.NumDescs = totalNumInstances;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    if (rebuildMode == raytracing_as_refit)
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
            tlas = createRaytracingTLASBuffer((uint32)info.ResultDataMaxSizeInBytes);
            SET_NAME(tlas->resource, "TLAS Result");
        }

        fromScratch = true;
    }

    if (!scratch || scratch->totalSize < info.ScratchDataSizeInBytes)
    {
        if (scratch)
        {
            resizeBuffer(scratch, (uint32)info.ScratchDataSizeInBytes);
        }
        else
        {
            scratch = createBuffer(1, (uint32)info.ScratchDataSizeInBytes, 0, true, false);
            SET_NAME(scratch->resource, "TLAS Scratch");
        }
    }



    dx_dynamic_constant_buffer gpuInstances = dxContext.uploadDynamicConstantBuffer(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * totalNumInstances, allInstances.data()).first;

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

        if (rebuildMode == raytracing_as_refit)
        {
            asDesc.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
            asDesc.SourceAccelerationStructureData = tlas->gpuVirtualAddress;
        }
    }


    cl->commandList->BuildRaytracingAccelerationStructure(&asDesc, 0, 0);
    cl->uavBarrier(tlas);
}
