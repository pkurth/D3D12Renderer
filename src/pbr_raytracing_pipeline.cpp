#include "pch.h"
#include "pbr_raytracing_pipeline.h"
#include "dx_command_list.h"
#include "raytracing_tlas.h"

#include "raytracing.hlsli"

#define PBR_RAYTRACING_RS_TLAS      0
#define PBR_RAYTRACING_RS_OUTPUT    1
#define PBR_RAYTRACING_RS_TEXTURES  2
#define PBR_RAYTRACING_RS_CAMERA    3
#define PBR_RAYTRACING_RS_SUN       4
#define PBR_RAYTRACING_RS_CB        5

void pbr_raytracing_pipeline::initialize(const wchar* shaderPath)
{
    reservedDescriptorsAtStart = 6; // Depth buffer, normal map, global irradiance, environment, sky, brdf.


    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.NumDescriptors = 4096; // TODO
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    checkResult(dxContext.device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap)));
    cpuBaseDescriptorHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    gpuBaseDescriptorHandle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();

    // Offset for the output and TLAS descriptor. Output is set every frame, and TLAS can be updated, so we ring buffer this.
    cpuCurrentDescriptorHandle = cpuBaseDescriptorHandle + (1 + 1 + reservedDescriptorsAtStart) * NUM_BUFFERED_FRAMES;




    CD3DX12_DESCRIPTOR_RANGE tlasRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
    CD3DX12_DESCRIPTOR_RANGE outputRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);
    CD3DX12_DESCRIPTOR_RANGE texturesRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, reservedDescriptorsAtStart, 0, 1);
    CD3DX12_ROOT_PARAMETER globalRootParameters[6];
    globalRootParameters[PBR_RAYTRACING_RS_TLAS].InitAsDescriptorTable(1, &tlasRange);
    globalRootParameters[PBR_RAYTRACING_RS_OUTPUT].InitAsDescriptorTable(1, &outputRange);
    globalRootParameters[PBR_RAYTRACING_RS_TEXTURES].InitAsDescriptorTable(1, &texturesRange);
    globalRootParameters[PBR_RAYTRACING_RS_CAMERA].InitAsConstantBufferView(0);
    globalRootParameters[PBR_RAYTRACING_RS_SUN].InitAsConstantBufferView(1);
    globalRootParameters[PBR_RAYTRACING_RS_CB].InitAsConstants(sizeof(raytracing_cb) / 4, 2);

    CD3DX12_STATIC_SAMPLER_DESC globalStaticSamplers[2];
    globalStaticSamplers[0].Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
    globalStaticSamplers[1].Init(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    D3D12_ROOT_SIGNATURE_DESC globalDesc = { arraysize(globalRootParameters), globalRootParameters, arraysize(globalStaticSamplers), globalStaticSamplers };

    CD3DX12_DESCRIPTOR_RANGE hitSRVRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 0, 2);
    CD3DX12_ROOT_PARAMETER hitRootParameters[2];
    hitRootParameters[0].InitAsConstants(sizeof(pbr_material_cb) / 4, 0, 2);
    hitRootParameters[1].InitAsDescriptorTable(1, &hitSRVRange);
    D3D12_ROOT_SIGNATURE_DESC hitDesc = { arraysize(hitRootParameters), hitRootParameters };


    pipeline =
        raytracing_pipeline_builder(shaderPath, 4 * sizeof(float), MAX_PBR_RAYTRACING_RECURSION_DEPTH)
        .globalRootSignature(globalDesc)
        .raygen(L"rayGen")
        .hitgroup(L"Radiance", L"radianceClosestHit", L"radianceAnyHit", L"radianceMiss", hitDesc)
        .hitgroup(L"Shadow", L"shadowClosestHit", L"shadowAnyHit", L"shadowMiss")
        .finish();


    assert(sizeof(binding_table_entry) == pipeline.shaderBindingTableDesc.entrySize);
    assert(offsetof(binding_table, raygen) == pipeline.shaderBindingTableDesc.raygenOffset);
    assert(offsetof(binding_table, miss) == pipeline.shaderBindingTableDesc.missOffset);
    assert(offsetof(binding_table, hit) == pipeline.shaderBindingTableDesc.hitOffset);

    maxNumHitGroups = 1024;
    allocateBindingTableBuffer();

    memcpy(bindingTable->raygen.identifier, pipeline.shaderBindingTableDesc.raygen, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    for (uint32 i = 0; i < numRayTypes; ++i)
    {
        memcpy(bindingTable->miss[i].identifier, pipeline.shaderBindingTableDesc.miss[i], D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    }
}

raytracing_object_handle pbr_raytracing_pipeline::defineObjectType(const raytracing_blas& blas, const std::vector<ref<pbr_material>>& materials)
{
    assert(blas.geometries.size() == materials.size());

    uint32 numGeometries = (uint32)blas.geometries.size();


    if (maxNumHitGroups < numHitGroups + numGeometries)
    {
        maxNumHitGroups = max(maxNumHitGroups * 2, numHitGroups + numGeometries);
        allocateBindingTableBuffer();
    }


    binding_table_entry* currentHitEntry = bindingTable->hit + numRayTypes * numHitGroups;

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

        memcpy(currentHitEntry->identifier, pipeline.shaderBindingTableDesc.hitGroups[0], D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        currentHitEntry->materialCB = pbr_material_cb
        {
            material->albedoTint.x, material->albedoTint.y, material->albedoTint.z, material->albedoTint.w,
            packRoughnessAndMetallic(material->roughnessOverride, material->metallicOverride),
            flags
        };
        currentHitEntry->srvRange = base;
        ++currentHitEntry;

        memcpy(currentHitEntry->identifier, pipeline.shaderBindingTableDesc.hitGroups[1], D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        ++currentHitEntry;



        ++numHitGroups;
    }

    raytracing_object_handle result = { blas.blas->gpuVirtualAddress, instanceContributionToHitGroupIndex };

    instanceContributionToHitGroupIndex += numGeometries * numRayTypes;

    return result;
}

void pbr_raytracing_pipeline::build()
{
    bindingTableBuffer = createBuffer(1, totalBindingTableSize, bindingTable);
}

void pbr_raytracing_pipeline::allocateBindingTableBuffer()
{
    totalBindingTableSize = (uint32)
        (alignTo(sizeof(binding_table_entry), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT)
            + alignTo(sizeof(binding_table_entry) * numRayTypes, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT)
            + alignTo(sizeof(binding_table_entry) * numRayTypes * maxNumHitGroups, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));

    if (!bindingTable)
    {
        bindingTable = (binding_table*)malloc(totalBindingTableSize);
    }
    else
    {
        bindingTable = (binding_table*)realloc(bindingTable, totalBindingTableSize);
    }
}

void pbr_raytracing_pipeline::fillOutRayTracingRenderDesc(D3D12_DISPATCH_RAYS_DESC& raytraceDesc, uint32 renderWidth, uint32 renderHeight, uint32 renderDepth, uint32 numRayTypes, uint32 numHitGroups)
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

dx_gpu_descriptor_handle pbr_raytracing_pipeline::setTLASHandle(dx_cpu_descriptor_handle cpuHandle)
{
    uint32 outputIndex = dxContext.bufferedFrameID; // TLAS is at 0 or 1.
    dxContext.device->CopyDescriptorsSimple(1, (cpuBaseDescriptorHandle + outputIndex).cpuHandle, cpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    dx_gpu_descriptor_handle outputHandle = gpuBaseDescriptorHandle + outputIndex;
    return outputHandle;
}

dx_gpu_descriptor_handle pbr_raytracing_pipeline::setOutputTexture(const ref<dx_texture>& output)
{
    uint32 outputIndex = NUM_BUFFERED_FRAMES + dxContext.bufferedFrameID; // TLAS is at 0 or 1.
    dxContext.device->CopyDescriptorsSimple(1, (cpuBaseDescriptorHandle + outputIndex).cpuHandle, output->defaultUAV.cpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    dx_gpu_descriptor_handle outputHandle = gpuBaseDescriptorHandle + outputIndex;
    return outputHandle;
}

dx_gpu_descriptor_handle pbr_raytracing_pipeline::setTextures(const ref<dx_texture>* textures)
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

void pbr_raytracing_pipeline::render(struct dx_command_list* cl, const raytracing_tlas& tlas,
    const ref<dx_texture>& output,
    uint32 numBounces,
    float fadeoutDistance, float maxDistance,
    float environmentIntensity, float skyIntensity,
    dx_dynamic_constant_buffer cameraCBV, dx_dynamic_constant_buffer sunCBV,
    const ref<dx_texture>& depthBuffer, const ref<dx_texture>& normalMap,
    const ref<pbr_environment>& environment, const ref<dx_texture>& brdf)
{
    const ref<dx_texture> textures[] =
    {
        depthBuffer,
        normalMap,
        environment->irradiance,
        environment->environment,
        environment->sky,
        brdf
    };

    dx_gpu_descriptor_handle tlasHandle = setTLASHandle(tlas.getHandle());
    dx_gpu_descriptor_handle outputHandle = setOutputTexture(output);
    dx_gpu_descriptor_handle resourceHandle = setTextures(textures);

    D3D12_DISPATCH_RAYS_DESC raytraceDesc;
    fillOutRayTracingRenderDesc(raytraceDesc, output->width, output->height, 1, numRayTypes, numHitGroups);

    cl->setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, descriptorHeap);

    cl->setPipelineState(pipeline.pipeline);
    cl->setComputeRootSignature(pipeline.rootSignature);

    raytracing_cb raytracingCB = { numBounces, fadeoutDistance, maxDistance, environmentIntensity, skyIntensity };

    cl->setComputeDescriptorTable(PBR_RAYTRACING_RS_TLAS, tlasHandle);
    cl->setComputeDescriptorTable(PBR_RAYTRACING_RS_OUTPUT, outputHandle);
    cl->setComputeDescriptorTable(PBR_RAYTRACING_RS_TEXTURES, resourceHandle);
    cl->setComputeDynamicConstantBuffer(PBR_RAYTRACING_RS_CAMERA, cameraCBV);
    cl->setComputeDynamicConstantBuffer(PBR_RAYTRACING_RS_SUN, sunCBV);
    cl->setCompute32BitConstants(PBR_RAYTRACING_RS_CB, raytracingCB);

    cl->raytrace(raytraceDesc);
}

