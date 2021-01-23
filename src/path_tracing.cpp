#include "pch.h"
#include "path_tracing.h"
#include "color.h"

#include "raytracing.hlsli"

#define PATH_TRACING_RS_SRVS    0
#define PATH_TRACING_RS_OUTPUT  1
#define PATH_TRACING_RS_CAMERA  2
#define PATH_TRACING_RS_CB      3

void path_tracer::initialize()
{
    const wchar* shaderPath = L"shaders/raytracing/path_tracing_rts.hlsl";


    CD3DX12_DESCRIPTOR_RANGE resourceRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, arraysize(global_resources::resources), 0);
    CD3DX12_DESCRIPTOR_RANGE outputRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    CD3DX12_ROOT_PARAMETER globalRootParameters[] =
    {
        root_descriptor_table(1, &resourceRange),
        root_descriptor_table(1, &outputRange),
        root_cbv(0), // Camera.
        root_constants<path_tracing_cb>(1),
    };

    CD3DX12_STATIC_SAMPLER_DESC globalStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

    D3D12_ROOT_SIGNATURE_DESC globalDesc =
    {
        arraysize(globalRootParameters), globalRootParameters,
        1, &globalStaticSampler
    };



    CD3DX12_DESCRIPTOR_RANGE hitSRVRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 0, 1);
    CD3DX12_ROOT_PARAMETER hitRootParameters[] =
    {
        root_constants<pbr_material_cb>(0, 1),
        root_descriptor_table(1, &hitSRVRange),
    };

    D3D12_ROOT_SIGNATURE_DESC hitDesc =
    {
        arraysize(hitRootParameters), hitRootParameters
    };



    pipeline =
        raytracing_pipeline_builder(shaderPath, maxPayloadSize, maxRecursionDepth)
        .globalRootSignature(globalDesc)
        .raygen(L"rayGen")
        .hitgroup(L"RADIANCE", L"radianceClosestHit", L"radianceAnyHit", L"radianceMiss", hitDesc)
        .hitgroup(L"SHADOW", L"shadowClosestHit", L"shadowAnyHit", L"shadowMiss")
        .finish();

    numRayTypes = (uint32)pipeline.shaderBindingTableDesc.hitGroups.size();

    bindingTable.initialize(&pipeline);
    descriptorHeap.initialize(2048); // TODO.


    // Allocate space in descriptor heap for global resources.
    // These are not initialized here! This will happen in each frame.
    for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; ++i)
    {
        global_resources& gr = globalResources[i];

        gr.cpuBase = descriptorHeap.currentCPU;
        gr.gpuBase = descriptorHeap.currentGPU;

        for (uint32 j = 0; j < arraysize(gr.resources); ++j)
        {
            gr.resources[j] = descriptorHeap.push();
        }
    }
}

raytracing_object_type path_tracer::defineObjectType(const ref<raytracing_blas>& blas, const std::vector<ref<pbr_material>>& materials)
{
    uint32 numGeometries = (uint32)blas->geometries.size();

    shader_data* hitData = (shader_data*)alloca(sizeof(shader_data) * numRayTypes);

    for (uint32 i = 0; i < numGeometries; ++i)
    {
        submesh_info submesh = blas->geometries[i].submesh;
        const ref<pbr_material>& material = materials[i];

        dx_cpu_descriptor_handle base = descriptorHeap.currentCPU;

        descriptorHeap.push().createBufferSRV(blas->geometries[i].vertexBuffer, { submesh.baseVertex, submesh.numVertices });
        descriptorHeap.push().createRawBufferSRV(blas->geometries[i].indexBuffer, { submesh.firstTriangle * 3, submesh.numTriangles * 3 });


        uint32 flags = 0;

        if (material->albedo)
        {
            descriptorHeap.push().create2DTextureSRV(material->albedo);
            flags |= USE_ALBEDO_TEXTURE;
        }
        else
        {
            descriptorHeap.push().createNullTextureSRV();
        }

        if (material->normal)
        {
            descriptorHeap.push().create2DTextureSRV(material->normal);
            flags |= USE_NORMAL_TEXTURE;
        }
        else
        {
            descriptorHeap.push().createNullTextureSRV();
        }

        if (material->roughness)
        {
            descriptorHeap.push().create2DTextureSRV(material->roughness);
            flags |= USE_ROUGHNESS_TEXTURE;
        }
        else
        {
            descriptorHeap.push().createNullTextureSRV();
        }

        if (material->metallic)
        {
            descriptorHeap.push().create2DTextureSRV(material->metallic);
            flags |= USE_METALLIC_TEXTURE;
        }
        else
        {
            descriptorHeap.push().createNullTextureSRV();
        }

        hitData[0].materialCB = pbr_material_cb
        {
            material->emission.xyz,
            packColor(material->albedoTint),
            packRoughnessAndMetallic(material->roughnessOverride, material->metallicOverride),
            flags
        };
        hitData[0].resources = base;

        // The other shader types don't need any data, so we don't set it here.

        bindingTable.push(hitData);
    }


    raytracing_object_type result = { blas, instanceContributionToHitGroupIndex };

    instanceContributionToHitGroupIndex += numGeometries * numRayTypes;

    return result;
}

void path_tracer::finish()
{
    bindingTable.build();
}

void path_tracer::render(dx_command_list* cl, const raytracing_tlas& tlas,
    const ref<dx_texture>& output,
    const common_material_info& materialInfo)
{
    global_resources& gr = globalResources[dxContext.bufferedFrameID];

    D3D12_CPU_DESCRIPTOR_HANDLE handles[] =
    {
        output->defaultUAV,

        tlas.tlas->raytracingSRV,
        materialInfo.sky->defaultSRV,
    };

    static_assert(arraysize(handles) == arraysize(global_resources::resources));
    uint32 numHandles = arraysize(handles);
    D3D12_CPU_DESCRIPTOR_HANDLE dst = gr.cpuBase;

    dxContext.device->CopyDescriptors(
        1, &dst, &numHandles,
        numHandles, handles, nullptr,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


    D3D12_DISPATCH_RAYS_DESC raytraceDesc;
    fillOutRayTracingRenderDesc(bindingTable.getBuffer(), raytraceDesc,
        output->width, output->height, 1,
        numRayTypes, bindingTable.getNumberOfHitGroups());

    cl->setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, descriptorHeap.descriptorHeap);

    cl->setPipelineState(pipeline.pipeline);
    cl->setComputeRootSignature(pipeline.rootSignature);

    uint32 depth = min(recursionDepth, maxRecursionDepth - 1);

    cl->setComputeDescriptorTable(PATH_TRACING_RS_SRVS, gr.gpuBase + 1); // Offset for output.
    cl->setComputeDescriptorTable(PATH_TRACING_RS_OUTPUT, gr.gpuBase);
    cl->setComputeDynamicConstantBuffer(PATH_TRACING_RS_CAMERA, materialInfo.cameraCBV);
    cl->setCompute32BitConstants(PATH_TRACING_RS_CB, 
        path_tracing_cb
        { 
            (uint32)dxContext.frameID, 
            numAveragedFrames, 
            depth,
            clamp(startRussianRouletteAfter, 0u, depth),
            (uint32)useThinLensCamera, 
            focalLength, 
            focalLength / (2.f * fNumber),
            (uint32)useRealMaterials,
            (uint32)enableDirectLighting,
            lightIntensityScale,
            pointLightRadius,
        });

    ++numAveragedFrames;

    cl->raytrace(raytraceDesc);
}


