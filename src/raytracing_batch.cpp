#include "pch.h"
#include "raytracing_batch.h"
#include "dx_context.h"
#include "dx_command_list.h"

#include "raytracing.hlsl"

/*
Hit group shader table index =
     RayContributionToHitGroupIndex                      ~ from shader: TraceRay()
     + MultiplierForGeometryContributionToHitGroupIndex  ~ from shader: TraceRay()
     * GeometryContributionToHitGroupIndex               ~ system generated index
                                                           of geometry in BLAS
     + InstanceContributionToHitGroupIndex               ~ from BLAS instance desc

*/

#define PBR_RAYTRACING_RS_TLAS      0
#define PBR_RAYTRACING_RS_OUTPUT    1
#define PBR_RAYTRACING_RS_TEXTURES  2
#define PBR_RAYTRACING_RS_CAMERA    3
#define PBR_RAYTRACING_RS_SUN       4
#define PBR_RAYTRACING_RS_CB        5


void specular_reflections_raytracing_batch::initialize(uint32 maxNumObjectTypes, acceleration_structure_rebuild_mode rebuildMode)
{
    pbr_raytracing_batch::initialize(L"shaders/raytracing/specular_reflections_rts.hlsl", maxNumObjectTypes, rebuildMode);
}

void pbr_raytracing_batch::initialize(const wchar* shaderName, uint32 maxNumObjectTypes, acceleration_structure_rebuild_mode rebuildMode)
{
    uint32 numResources = 6; // Depth buffer, normal map, global irradiance, environment, sky, brdf.

    raytracing_batch::initialize(rebuildMode, numResources);

    CD3DX12_DESCRIPTOR_RANGE tlasRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
    CD3DX12_DESCRIPTOR_RANGE outputRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);
    CD3DX12_DESCRIPTOR_RANGE texturesRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, numResources, 0, 1);
    CD3DX12_ROOT_PARAMETER globalRootParameters[6];
    globalRootParameters[PBR_RAYTRACING_RS_TLAS].InitAsDescriptorTable(1, &tlasRange);
    globalRootParameters[PBR_RAYTRACING_RS_OUTPUT].InitAsDescriptorTable(1, &outputRange);
    globalRootParameters[PBR_RAYTRACING_RS_TEXTURES].InitAsDescriptorTable(1, &texturesRange);
    globalRootParameters[PBR_RAYTRACING_RS_CAMERA].InitAsConstantBufferView(0);
    globalRootParameters[PBR_RAYTRACING_RS_SUN].InitAsConstantBufferView(1);
    globalRootParameters[PBR_RAYTRACING_RS_CB].InitAsConstants(1, 2);

    CD3DX12_STATIC_SAMPLER_DESC globalStaticSamplers[2];
    globalStaticSamplers[0].Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
    globalStaticSamplers[1].Init(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    D3D12_ROOT_SIGNATURE_DESC globalDesc = { arraysize(globalRootParameters), globalRootParameters, arraysize(globalStaticSamplers), globalStaticSamplers };

    CD3DX12_DESCRIPTOR_RANGE hitSRVRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 0, 2);
    CD3DX12_ROOT_PARAMETER hitRootParameters[1];
    hitRootParameters[0].InitAsDescriptorTable(1, &hitSRVRange);
    D3D12_ROOT_SIGNATURE_DESC hitDesc = { arraysize(hitRootParameters), hitRootParameters };


    pipeline =
        raytracing_pipeline_builder(shaderName, 4 * sizeof(float), MAX_RAYTRACING_RECURSION_DEPTH)
        .globalRootSignature(globalDesc)
        .raygen(L"rayGen")
        .hitgroup(L"Radiance", L"radianceClosestHit", L"radianceAnyHit", L"radianceMiss", hitDesc)
        .hitgroup(L"Shadow", L"shadowClosestHit", L"shadowAnyHit", L"shadowMiss")
        .finish();


    assert(sizeof(binding_table_entry) == pipeline.shaderBindingTableDesc.entrySize);

    bindingTable.reserve(maxNumObjectTypes * 32 * numRayTypes + 1 + numRayTypes); // 32 is a random number. How many geometries do we expect per blas?

    binding_table_entry raygen;
    memcpy(raygen.identifier, pipeline.shaderBindingTableDesc.raygen, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    bindingTable.push_back(raygen);

    binding_table_entry radianceMiss;
    memcpy(radianceMiss.identifier, pipeline.shaderBindingTableDesc.miss[0], D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    bindingTable.push_back(radianceMiss);

    binding_table_entry shadowMiss;
    memcpy(shadowMiss.identifier, pipeline.shaderBindingTableDesc.miss[1], D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    bindingTable.push_back(shadowMiss);


}

raytracing_object_handle pbr_raytracing_batch::defineObjectType(const raytracing_blas& blas, const std::vector<ref<pbr_material>>& materials)
{
    assert(blas.geometries.size() == materials.size());

    uint32 numGeometries = (uint32)blas.geometries.size();

    for (uint32 i = 0; i < numGeometries; ++i)
    {
        submesh_info submesh = blas.geometries[i].submesh;
        const ref<pbr_material>& material = materials[i];

        dx_cpu_descriptor_handle base = cpuCurrentDescriptorHandle;

        (cpuCurrentDescriptorHandle++).createBufferSRV(blas.geometries[i].vertexBuffer, { submesh.baseVertex, submesh.numVertices });
        (cpuCurrentDescriptorHandle++).createRawBufferSRV(blas.geometries[i].indexBuffer, { submesh.firstTriangle * 3, submesh.numTriangles * 3 });
        
        if (material->albedo) 
        {
            (cpuCurrentDescriptorHandle++).create2DTextureSRV(material->albedo);
        }
        else
        {
            (cpuCurrentDescriptorHandle++).createNullTextureSRV();
        }

        if (material->normal)
        {
            (cpuCurrentDescriptorHandle++).create2DTextureSRV(material->normal);
        }
        else
        {
            (cpuCurrentDescriptorHandle++).createNullTextureSRV();
        }

        if (material->roughness)
        {
            (cpuCurrentDescriptorHandle++).create2DTextureSRV(material->roughness);
        }
        else
        {
            (cpuCurrentDescriptorHandle++).createNullTextureSRV();
        }

        if (material->metallic)
        {
            (cpuCurrentDescriptorHandle++).create2DTextureSRV(material->metallic);
        }
        else
        {
            (cpuCurrentDescriptorHandle++).createNullTextureSRV();
        }


        binding_table_entry radianceHit;
        memcpy(radianceHit.identifier, pipeline.shaderBindingTableDesc.hitGroups[0], D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        radianceHit.radianceHit.srvRange = base;
        bindingTable.push_back(radianceHit);

        binding_table_entry shadowHit;
        memcpy(shadowHit.identifier, pipeline.shaderBindingTableDesc.hitGroups[1], D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        bindingTable.push_back(shadowHit);
    }

    raytracing_object_handle result = { blas.blas->gpuVirtualAddress, instanceContributionToHitGroupIndex };
    
    instanceContributionToHitGroupIndex += numGeometries * numRayTypes;

    return result;
}

void pbr_raytracing_batch::buildBindingTable()
{
    assert(!bindingTableBuffer);
    bindingTableBuffer = createBuffer(sizeof(binding_table_entry), (uint32)bindingTable.size(), bindingTable.data());
}

void pbr_raytracing_batch::render(struct dx_command_list* cl, const ref<dx_texture>& output, uint32 numBounces, 
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

    dx_gpu_descriptor_handle tlasHandle = getTLASHandle();
    dx_gpu_descriptor_handle outputHandle = setOutputTexture(output);
    dx_gpu_descriptor_handle resourceHandle = setTextures(textures);

    auto desc = output->resource->GetDesc();

    D3D12_DISPATCH_RAYS_DESC raytraceDesc;
    fillOutRayTracingRenderDesc(raytraceDesc, (uint32)desc.Width, desc.Height, numRayTypes);

    cl->setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, descriptorHeap);

    cl->setPipelineState(pipeline.pipeline);
    cl->setComputeRootSignature(pipeline.rootSignature);

    raytracing_cb raytracingCB = { numBounces };

    cl->setComputeDescriptorTable(PBR_RAYTRACING_RS_TLAS, tlasHandle);
    cl->setComputeDescriptorTable(PBR_RAYTRACING_RS_OUTPUT, outputHandle);
    cl->setComputeDescriptorTable(PBR_RAYTRACING_RS_TEXTURES, resourceHandle);
    cl->setComputeDynamicConstantBuffer(PBR_RAYTRACING_RS_CAMERA, cameraCBV);
    cl->setComputeDynamicConstantBuffer(PBR_RAYTRACING_RS_SUN, sunCBV);
    cl->setCompute32BitConstants(PBR_RAYTRACING_RS_CB, raytracingCB);

    cl->raytrace(raytraceDesc);
}

void raytracing_batch::buildAll()
{
    buildAccelerationStructure();
    buildBindingTable();
}

void raytracing_batch::buildAccelerationStructure()
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
    if (!tlas.tlas || tlas.tlas->totalSize < info.ResultDataMaxSizeInBytes)
    {
        if (tlas.tlas)
        {
            resizeBuffer(tlas.tlas, (uint32)info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
        }
        else
        {
            tlas.tlas = createBuffer(1, (uint32)info.ResultDataMaxSizeInBytes, 0, true, false, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
            SET_NAME(tlas.tlas->resource, "TLAS Result");
        }

        fromScratch = true;
    }

    if (!tlas.scratch || tlas.scratch->totalSize < info.ScratchDataSizeInBytes)
    {
        if (tlas.scratch)
        {
            resizeBuffer(tlas.scratch, (uint32)info.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }
        else
        {
            tlas.scratch = createBuffer(1, (uint32)info.ScratchDataSizeInBytes, 0, true, false, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            SET_NAME(tlas.scratch->resource, "TLAS Scratch");
        }
    }

    

    dx_command_list* cl = dxContext.getFreeRenderCommandList();
    dx_dynamic_constant_buffer gpuInstances = cl->uploadDynamicConstantBuffer(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * totalNumInstances, allInstances.data());

    inputs.InstanceDescs = gpuInstances.gpuPtr;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
    asDesc.Inputs = inputs;
    asDesc.DestAccelerationStructureData = tlas.tlas->gpuVirtualAddress;
    asDesc.ScratchAccelerationStructureData = tlas.scratch->gpuVirtualAddress;

    if (!fromScratch)
    {
        assert(rebuildMode != acceleration_structure_no_rebuild);

        cl->uavBarrier(tlas.tlas);

        if (rebuildMode == acceleration_structure_refit)
        {
            asDesc.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
            asDesc.SourceAccelerationStructureData = tlas.tlas->gpuVirtualAddress;
        }
    }


    cl->commandList->BuildRaytracingAccelerationStructure(&asDesc, 0, 0);
    cl->uavBarrier(tlas.tlas);

    dxContext.executeCommandList(cl);

    tlasDescriptorIndex = (tlasDescriptorIndex + 1) % NUM_BUFFERED_FRAMES;
    (cpuBaseDescriptorHandle + tlasDescriptorIndex).createRaytracingAccelerationStructureSRV(tlas.tlas);
}

raytracing_instance_handle raytracing_batch::instantiate(raytracing_object_handle type, const trs& transform)
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

void raytracing_batch::updateInstanceTransform(raytracing_instance_handle handle, const trs& transform)
{
    D3D12_RAYTRACING_INSTANCE_DESC& instance = allInstances[handle.instanceIndex];
    mat4 m = transpose(trsToMat4(transform));
    memcpy(instance.Transform, &m, sizeof(instance.Transform));
}

void raytracing_batch::initialize(acceleration_structure_rebuild_mode rebuildMode, uint32 reserveDescriptorsAtStart)
{
    this->rebuildMode = rebuildMode;
    this->reservedDescriptorsAtStart = reserveDescriptorsAtStart;

    allInstances.reserve(4096); // TODO

    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.NumDescriptors = 4096; // TODO
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    checkResult(dxContext.device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap)));
    cpuBaseDescriptorHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    gpuBaseDescriptorHandle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();

    // Offset for the output and TLAS descriptor. Output is set every frame, and TLAS can be updated, so we ring buffer this.
    cpuCurrentDescriptorHandle = cpuBaseDescriptorHandle + (1 + 1 + reserveDescriptorsAtStart) * NUM_BUFFERED_FRAMES;
}

void raytracing_batch::fillOutRayTracingRenderDesc(D3D12_DISPATCH_RAYS_DESC& raytraceDesc, uint32 renderWidth, uint32 renderHeight, uint32 numRayTypes)
{
    raytraceDesc.Width = renderWidth;
    raytraceDesc.Height = renderHeight;
    raytraceDesc.Depth = 1;

    uint32 numHitShaders = bindingTableBuffer->elementCount - 1 - numRayTypes;

    // Pointer to the entry point of the ray-generation shader.
    raytraceDesc.RayGenerationShaderRecord.StartAddress = bindingTableBuffer->gpuVirtualAddress + 0;
    raytraceDesc.RayGenerationShaderRecord.SizeInBytes = bindingTableBuffer->elementSize;

    // Pointer to the entry point(s) of the miss shader.
    raytraceDesc.MissShaderTable.StartAddress = bindingTableBuffer->gpuVirtualAddress + bindingTableBuffer->elementSize;
    raytraceDesc.MissShaderTable.StrideInBytes = bindingTableBuffer->elementSize;
    raytraceDesc.MissShaderTable.SizeInBytes = bindingTableBuffer->elementSize * numRayTypes;

    // Pointer to the entry point(s) of the hit shader.
    raytraceDesc.HitGroupTable.StartAddress = bindingTableBuffer->gpuVirtualAddress + bindingTableBuffer->elementSize * (numRayTypes + 1);
    raytraceDesc.HitGroupTable.StrideInBytes = bindingTableBuffer->elementSize;
    raytraceDesc.HitGroupTable.SizeInBytes = bindingTableBuffer->elementSize * numHitShaders;

    raytraceDesc.CallableShaderTable = {};
}

dx_gpu_descriptor_handle raytracing_batch::getTLASHandle()
{
    dx_gpu_descriptor_handle tlasHandle = gpuBaseDescriptorHandle + tlasDescriptorIndex;
    return tlasHandle;
}

dx_gpu_descriptor_handle raytracing_batch::setOutputTexture(const ref<dx_texture>& output)
{
    uint32 outputIndex = 2 + dxContext.bufferedFrameID; // TLAS is at 0 or 1.
    dxContext.device->CopyDescriptorsSimple(1, (cpuBaseDescriptorHandle + outputIndex).cpuHandle, output->defaultUAV.cpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    dx_gpu_descriptor_handle outputHandle = gpuBaseDescriptorHandle + outputIndex;
    return outputHandle;
}

dx_gpu_descriptor_handle raytracing_batch::setTextures(const ref<dx_texture>* textures)
{
    uint32 index = 4 + reservedDescriptorsAtStart * dxContext.bufferedFrameID; // First 4 are TLAS (x2) and output (x2).

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
