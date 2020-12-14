#include "pch.h"
#include "pbr_raytracing_binding_table.h"
#include "dx_command_list.h"
#include "raytracing_tlas.h"
#include "dx_renderer.h"


void pbr_raytracing_binding_table::initialize()
{
    pipeline = dx_renderer::getRaytracingPipeline();
    reservedDescriptorsAtStart = pipeline->getNumberOfRequiredResources();


    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.NumDescriptors = 4096; // TODO
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    checkResult(dxContext.device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap)));
    cpuBaseDescriptorHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    gpuBaseDescriptorHandle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();

    // Offset for the output and TLAS descriptor. Output is set every frame, and TLAS can be updated, so we ring buffer this.
    cpuCurrentDescriptorHandle = cpuBaseDescriptorHandle + (1 + 1 + reservedDescriptorsAtStart) * NUM_BUFFERED_FRAMES;



    assert(sizeof(binding_table_entry) == pipeline->pipeline.shaderBindingTableDesc.entrySize);
    assert(offsetof(binding_table, raygen) == pipeline->pipeline.shaderBindingTableDesc.raygenOffset);
    assert(offsetof(binding_table, miss) == pipeline->pipeline.shaderBindingTableDesc.missOffset);
    assert(offsetof(binding_table, hit) == pipeline->pipeline.shaderBindingTableDesc.hitOffset);

    maxNumHitGroups = 1024;
    allocateBindingTableBuffer();

    memcpy(bindingTable->raygen.identifier, pipeline->pipeline.shaderBindingTableDesc.raygen, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    for (uint32 i = 0; i < pbr_raytracing_pipeline::numRayTypes; ++i)
    {
        memcpy(bindingTable->miss[i].identifier, pipeline->pipeline.shaderBindingTableDesc.miss[i], D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    }
}

raytracing_object_handle pbr_raytracing_binding_table::defineObjectType(const raytracing_blas& blas, const std::vector<ref<pbr_material>>& materials)
{
    assert(blas.geometries.size() == materials.size());

    uint32 numGeometries = (uint32)blas.geometries.size();


    if (maxNumHitGroups < numHitGroups + numGeometries)
    {
        maxNumHitGroups = max(maxNumHitGroups * 2, numHitGroups + numGeometries);
        allocateBindingTableBuffer();
    }


    binding_table_entry* currentHitEntry = bindingTable->hit + pbr_raytracing_pipeline::numRayTypes * numHitGroups;

    for (uint32 i = 0; i < numGeometries; ++i)
    {
        submesh_info submesh = blas.geometries[i].submesh;
        const ref<pbr_material>& material = materials[i];

        dx_cpu_descriptor_handle base = cpuCurrentDescriptorHandle;

        (cpuCurrentDescriptorHandle++).createBufferSRV(blas.geometries[i].vertexBuffer, { submesh.baseVertex, submesh.numVertices });
        (cpuCurrentDescriptorHandle++).createRawBufferSRV(blas.geometries[i].indexBuffer, { submesh.firstTriangle * 3, submesh.numTriangles * 3 });

        uint32 flags = 0;

        if (material->albedo)
        {
            (cpuCurrentDescriptorHandle++).create2DTextureSRV(material->albedo);
            flags |= USE_ALBEDO_TEXTURE;
        }
        else
        {
            (cpuCurrentDescriptorHandle++).createNullTextureSRV();
        }

        if (material->normal)
        {
            (cpuCurrentDescriptorHandle++).create2DTextureSRV(material->normal);
            flags |= USE_NORMAL_TEXTURE;
        }
        else
        {
            (cpuCurrentDescriptorHandle++).createNullTextureSRV();
        }

        if (material->roughness)
        {
            (cpuCurrentDescriptorHandle++).create2DTextureSRV(material->roughness);
            flags |= USE_ROUGHNESS_TEXTURE;
        }
        else
        {
            (cpuCurrentDescriptorHandle++).createNullTextureSRV();
        }

        if (material->metallic)
        {
            (cpuCurrentDescriptorHandle++).create2DTextureSRV(material->metallic);
            flags |= USE_METALLIC_TEXTURE;
        }
        else
        {
            (cpuCurrentDescriptorHandle++).createNullTextureSRV();
        }

        memcpy(currentHitEntry->identifier, pipeline->pipeline.shaderBindingTableDesc.hitGroups[0], D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        currentHitEntry->materialCB = pbr_material_cb
        {
            material->albedoTint.x, material->albedoTint.y, material->albedoTint.z, material->albedoTint.w,
            packRoughnessAndMetallic(material->roughnessOverride, material->metallicOverride),
            flags
        };
        currentHitEntry->srvRange = base;
        ++currentHitEntry;

        memcpy(currentHitEntry->identifier, pipeline->pipeline.shaderBindingTableDesc.hitGroups[1], D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        ++currentHitEntry;



        ++numHitGroups;
    }

    raytracing_object_handle result = { blas.blas->gpuVirtualAddress, instanceContributionToHitGroupIndex };

    instanceContributionToHitGroupIndex += numGeometries * pbr_raytracing_pipeline::numRayTypes;

    return result;
}

void pbr_raytracing_binding_table::build()
{
    bindingTableBuffer = createBuffer(1, totalBindingTableSize, bindingTable);
}

void pbr_raytracing_binding_table::allocateBindingTableBuffer()
{
    totalBindingTableSize = (uint32)
        (alignTo(sizeof(binding_table_entry), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT)
            + alignTo(sizeof(binding_table_entry) * pbr_raytracing_pipeline::numRayTypes, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT)
            + alignTo(sizeof(binding_table_entry) * pbr_raytracing_pipeline::numRayTypes * maxNumHitGroups, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));

    if (!bindingTable)
    {
        bindingTable = (binding_table*)malloc(totalBindingTableSize);
    }
    else
    {
        bindingTable = (binding_table*)realloc(bindingTable, totalBindingTableSize);
    }
}

dx_gpu_descriptor_handle pbr_raytracing_binding_table::setTLASHandle(dx_cpu_descriptor_handle cpuHandle)
{
    uint32 outputIndex = dxContext.bufferedFrameID; // TLAS is at 0 or 1.
    dxContext.device->CopyDescriptorsSimple(1, (cpuBaseDescriptorHandle + outputIndex).cpuHandle, cpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    dx_gpu_descriptor_handle outputHandle = gpuBaseDescriptorHandle + outputIndex;
    return outputHandle;
}

dx_gpu_descriptor_handle pbr_raytracing_binding_table::setOutputTexture(const ref<dx_texture>& output)
{
    uint32 outputIndex = NUM_BUFFERED_FRAMES + dxContext.bufferedFrameID; // TLAS is at 0 or 1.
    dxContext.device->CopyDescriptorsSimple(1, (cpuBaseDescriptorHandle + outputIndex).cpuHandle, output->defaultUAV.cpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    dx_gpu_descriptor_handle outputHandle = gpuBaseDescriptorHandle + outputIndex;
    return outputHandle;
}

dx_gpu_descriptor_handle pbr_raytracing_binding_table::setTextures(const ref<dx_texture>* textures)
{
    uint32 index = 2 * NUM_BUFFERED_FRAMES + reservedDescriptorsAtStart * dxContext.bufferedFrameID; // First 4 are TLAS (x2) and output (x2).

    D3D12_CPU_DESCRIPTOR_HANDLE destDescriptorRangeStart = (cpuBaseDescriptorHandle + index).cpuHandle;
    uint32 numSrcDescriptors = reservedDescriptorsAtStart;

    D3D12_CPU_DESCRIPTOR_HANDLE srcHandles[32];
    assert(numSrcDescriptors <= arraysize(srcHandles));

    for (uint32 i = 0; i < numSrcDescriptors; ++i)
    {
        srcHandles[i] = textures[i]->defaultSRV;
    }

    dxContext.device->CopyDescriptors(
        1, &destDescriptorRangeStart, &numSrcDescriptors,
        numSrcDescriptors, srcHandles, nullptr,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    dx_gpu_descriptor_handle handle = gpuBaseDescriptorHandle + index;
    return handle;
}

