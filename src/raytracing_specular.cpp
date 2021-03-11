#include "pch.h"
#include "raytracing_specular.h"
#include "color.h"

#include "raytracing.hlsli"

#define SPECULAR_REFLECTIONS_RS_RESOURCES   0
#define SPECULAR_REFLECTIONS_RS_CAMERA      1
#define SPECULAR_REFLECTIONS_RS_SUN         2
#define SPECULAR_REFLECTIONS_RS_CB          3

void specular_reflections_raytracer::initialize()
{
    const wchar* shaderPath = L"shaders/raytracing/specular_reflections_rts.hlsl";


    const uint32 numInputResources = sizeof(input_resources) / sizeof(dx_cpu_descriptor_handle);
    const uint32 numOutputResources = sizeof(output_resources) / sizeof(dx_cpu_descriptor_handle);

    CD3DX12_DESCRIPTOR_RANGE resourceRanges[] =
    {
        // Must be input first, then output.
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, numInputResources, 0),
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, numOutputResources, 0),
    };

    CD3DX12_ROOT_PARAMETER globalRootParameters[] =
    {
        root_descriptor_table(arraysize(resourceRanges), resourceRanges),
        root_cbv(0), // Camera.
        root_cbv(1), // Sun.
        root_constants<raytracing_cb>(2),
    };

    CD3DX12_STATIC_SAMPLER_DESC globalStaticSamplers[] =
    {
       CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR),
       CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
    };

    D3D12_ROOT_SIGNATURE_DESC globalDesc =
    {
        arraysize(globalRootParameters), globalRootParameters,
        arraysize(globalStaticSamplers), globalStaticSamplers
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


    raytracing_mesh_hitgroup radianceHitgroup = { L"radianceClosestHit" };
    raytracing_mesh_hitgroup shadowHitgroup = { L"shadowClosestHit" };

    pipeline =
        raytracing_pipeline_builder(shaderPath, 4 * sizeof(float), maxRecursionDepth, true, false)
        .globalRootSignature(globalDesc)
        .raygen(L"rayGen")
        .hitgroup(L"RADIANCE", L"radianceMiss", radianceHitgroup, hitDesc)
        .hitgroup(L"SHADOW", L"shadowMiss", shadowHitgroup)
        .finish();

    numRayTypes = (uint32)pipeline.shaderBindingTableDesc.hitGroups.size();

    bindingTable.initialize(&pipeline);
    descriptorHeap.initialize(2048); // TODO.

    allocateDescriptorHeapSpaceForGlobalResources<input_resources, output_resources>(descriptorHeap);
}

raytracing_object_type specular_reflections_raytracer::defineObjectType(const ref<raytracing_blas>& blas, const std::vector<ref<pbr_material>>& materials)
{
    uint32 numGeometries = (uint32)blas->geometries.size();


    for (uint32 i = 0; i < numGeometries; ++i)
    {
        submesh_info submesh = blas->geometries[i].submesh;
        const ref<pbr_material>& material = materials[i];

        dx_cpu_descriptor_handle base = descriptorHeap.currentCPU;

        descriptorHeap.push().createBufferSRV(blas->geometries[i].vertexBuffer.others, { submesh.baseVertex, submesh.numVertices });
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

        shader_data hitData[2];
        hitData[0].materialCB.initialize(
            material->albedoTint,
            material->emission.xyz,
            material->roughnessOverride,
            material->metallicOverride,
            flags
        );
        hitData[0].resources = base;

        // Shadow ray does not need anything. Therefore we don't set its properties.

        assert(arraysize(hitData) == numRayTypes);

        bindingTable.push(hitData);
    }


    raytracing_object_type result = { blas, instanceContributionToHitGroupIndex };

    instanceContributionToHitGroupIndex += numGeometries * numRayTypes;

    return result;
}

void specular_reflections_raytracer::finish()
{
    bindingTable.build();
}

void specular_reflections_raytracer::render(dx_command_list* cl, const raytracing_tlas& tlas,
    const ref<dx_texture>& output,
    const common_material_info& materialInfo)
{
    input_resources in;
    in.tlas = tlas.tlas->raytracingSRV;
    in.depthBuffer = materialInfo.opaqueDepth->defaultSRV;
    in.screenSpaceNormals = materialInfo.worldNormals->defaultSRV;
    in.irradiance = materialInfo.irradiance->defaultSRV;
    in.environment = materialInfo.environment->defaultSRV;
    in.sky = materialInfo.sky->defaultSRV;
    in.brdf = materialInfo.brdf->defaultSRV;

    output_resources out;
    out.output = output->defaultUAV;


    dx_gpu_descriptor_handle gpuHandle = copyGlobalResourcesToDescriptorHeap(in, out);


    // Fill out description.
    D3D12_DISPATCH_RAYS_DESC raytraceDesc;
    fillOutRayTracingRenderDesc(bindingTable.getBuffer(), raytraceDesc,
        output->width, output->height, 1,
        numRayTypes, bindingTable.getNumberOfHitGroups());

    raytracing_cb raytracingCB = { numBounces, fadeoutDistance, maxDistance, materialInfo.environmentIntensity, materialInfo.skyIntensity };


    // Set up pipeline.
    cl->setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, descriptorHeap.descriptorHeap);

    cl->setPipelineState(pipeline.pipeline);
    cl->setComputeRootSignature(pipeline.rootSignature);

    cl->setComputeDescriptorTable(SPECULAR_REFLECTIONS_RS_RESOURCES, gpuHandle);
    cl->setComputeDynamicConstantBuffer(SPECULAR_REFLECTIONS_RS_CAMERA, materialInfo.cameraCBV);
    cl->setComputeDynamicConstantBuffer(SPECULAR_REFLECTIONS_RS_SUN, materialInfo.sunCBV);
    cl->setCompute32BitConstants(SPECULAR_REFLECTIONS_RS_CB, raytracingCB);

    cl->raytrace(raytraceDesc);
}
