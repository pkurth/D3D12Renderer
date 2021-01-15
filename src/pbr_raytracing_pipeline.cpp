#include "pch.h"
#include "pbr_raytracing_pipeline.h"
#include "raytracing_tlas.h"
#include "pbr_raytracing_binding_table.h"
#include "dx_command_list.h"

#include "raytracing.hlsli"
#include "material.hlsli"

#define PBR_RAYTRACING_RS_TLAS      0
#define PBR_RAYTRACING_RS_OUTPUT    1
#define PBR_RAYTRACING_RS_TEXTURES  2
#define PBR_RAYTRACING_RS_CAMERA    3
#define PBR_RAYTRACING_RS_SUN       4
#define PBR_RAYTRACING_RS_CB        5

void pbr_specular_reflections_raytracing_pipeline::initialize(const wchar* shaderPath)
{
    uint32 numResources = getNumberOfRequiredResources();

    CD3DX12_DESCRIPTOR_RANGE tlasRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
    CD3DX12_DESCRIPTOR_RANGE outputRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);
    CD3DX12_DESCRIPTOR_RANGE texturesRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, numResources, 0, 1);
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
}

void pbr_raytracing_pipeline::fillOutRayTracingRenderDesc(pbr_raytracing_binding_table& bindingTable, D3D12_DISPATCH_RAYS_DESC& raytraceDesc, uint32 renderWidth, uint32 renderHeight, uint32 renderDepth, uint32 numRayTypes, uint32 numHitGroups)
{
    raytraceDesc.Width = renderWidth;
    raytraceDesc.Height = renderHeight;
    raytraceDesc.Depth = renderDepth;

    uint32 numHitShaders = numHitGroups * numRayTypes;

    ref<dx_buffer> buffer = bindingTable.getBindingTableBuffer();

    // Pointer to the entry point of the ray-generation shader.
    raytraceDesc.RayGenerationShaderRecord.StartAddress = buffer->gpuVirtualAddress + pipeline.shaderBindingTableDesc.raygenOffset;
    raytraceDesc.RayGenerationShaderRecord.SizeInBytes = pipeline.shaderBindingTableDesc.entrySize;

    // Pointer to the entry point(s) of the miss shader.
    raytraceDesc.MissShaderTable.StartAddress = buffer->gpuVirtualAddress + pipeline.shaderBindingTableDesc.missOffset;
    raytraceDesc.MissShaderTable.StrideInBytes = pipeline.shaderBindingTableDesc.entrySize;
    raytraceDesc.MissShaderTable.SizeInBytes = pipeline.shaderBindingTableDesc.entrySize * numRayTypes;

    // Pointer to the entry point(s) of the hit shader.
    raytraceDesc.HitGroupTable.StartAddress = buffer->gpuVirtualAddress + pipeline.shaderBindingTableDesc.hitOffset;
    raytraceDesc.HitGroupTable.StrideInBytes = pipeline.shaderBindingTableDesc.entrySize;
    raytraceDesc.HitGroupTable.SizeInBytes = pipeline.shaderBindingTableDesc.entrySize * numHitShaders;

    raytraceDesc.CallableShaderTable = {};
}

void pbr_specular_reflections_raytracing_pipeline::render(struct dx_command_list* cl,
    pbr_raytracing_binding_table& bindingTable, const raytracing_tlas& tlas,
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

    dx_gpu_descriptor_handle tlasHandle = bindingTable.setTLASHandle(tlas.tlas->raytracingSRV);
    dx_gpu_descriptor_handle outputHandle = bindingTable.setOutputTexture(output);
    dx_gpu_descriptor_handle resourceHandle = bindingTable.setTextures(textures);

    D3D12_DISPATCH_RAYS_DESC raytraceDesc;
    fillOutRayTracingRenderDesc(bindingTable, raytraceDesc, output->width, output->height, 1, numRayTypes, bindingTable.getNumberOfHitGroups());

    cl->setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, bindingTable.getDescriptorHeap());

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
