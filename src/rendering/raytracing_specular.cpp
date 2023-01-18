#include "pch.h"
#include "raytracing_specular.h"
#include "core/color.h"
#include "render_resources.h"
#include "pbr_raytracer.h"

#include "dx/dx_profiling.h"
#include "dx/dx_barrier_batcher.h"

#include "rt_reflections_rs.hlsli"


struct rt_reflections_raytracer : pbr_raytracer
{
    void initialize();

    void render(dx_command_list* cl, const raytracing_tlas& tlas,
        ref<dx_texture> depthStencilBuffer,
        ref<dx_texture> worldNormalsRoughnessTexture,
        ref<dx_texture> screenVelocitiesTexture,
        ref<dx_texture> raycastTexture,
        ref<dx_texture> resolveTexture,
        ref<dx_texture> temporalHistory,
        ref<dx_texture> temporalOutput,
        const common_render_data& common);

private:

    const uint32 maxRecursionDepth = 2;

    // Only descriptors in here!
    struct input_resources
    {
        dx_cpu_descriptor_handle tlas;
        dx_cpu_descriptor_handle sky;
        dx_cpu_descriptor_handle probeIrradiance;
        dx_cpu_descriptor_handle probeDepth;
        dx_cpu_descriptor_handle depthBuffer;
        dx_cpu_descriptor_handle worldNormalsAndRoughness;
        dx_cpu_descriptor_handle noise;
        dx_cpu_descriptor_handle screenVelocitiesTexture;
    };

    struct output_resources
    {
        dx_cpu_descriptor_handle output;
    };
};




static rt_reflections_raytracer rtReflectionsTracer;


void initializeRTReflectionsPipelines()
{
    if (!dxContext.featureSupport.raytracing())
    {
        return;
    }

    rtReflectionsTracer.initialize();
}

void raytraceRTReflections(dx_command_list* cl, const raytracing_tlas& tlas, 
    ref<dx_texture> depthStencilBuffer,
    ref<dx_texture> worldNormalsRoughnessTexture,
    ref<dx_texture> screenVelocitiesTexture,
    ref<dx_texture> raycastTexture,
    ref<dx_texture> resolveTexture,
    ref<dx_texture> temporalHistory,	
    ref<dx_texture> temporalOutput,
    const common_render_data& common)
{
    if (!dxContext.featureSupport.raytracing())
    {
        return;
    }

    rtReflectionsTracer.finalizeForRender();
    rtReflectionsTracer.render(cl, tlas, depthStencilBuffer, worldNormalsRoughnessTexture, screenVelocitiesTexture, 
        raycastTexture, resolveTexture, temporalHistory, temporalOutput, common);
}

void rt_reflections_raytracer::initialize()
{
    const wchar* shaderPath = L"shaders/reflections/rt_reflections_rts.hlsl";


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
        root_constants<rt_reflections_cb>(0),
        root_cbv(1), // Camera.
        root_cbv(2), // Sun.
    };

    CD3DX12_STATIC_SAMPLER_DESC globalStaticSamplers[] =
    {
       CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR),
       CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
    };

    D3D12_ROOT_SIGNATURE_DESC globalDesc =
    {
        arraysize(globalRootParameters), globalRootParameters,
        arraysize(globalStaticSamplers), globalStaticSamplers
    };

    pbr_raytracer::initialize(shaderPath, 3 * sizeof(float), maxRecursionDepth, globalDesc);

    allocateDescriptorHeapSpaceForGlobalResources<input_resources, output_resources>(descriptorHeap);
}

void rt_reflections_raytracer::render(dx_command_list* cl, const raytracing_tlas& tlas,
    ref<dx_texture> depthStencilBuffer,
    ref<dx_texture> worldNormalsRoughnessTexture,
    ref<dx_texture> screenVelocitiesTexture,
    ref<dx_texture> raycastTexture,
    ref<dx_texture> resolveTexture,
    ref<dx_texture> temporalHistory,
    ref<dx_texture> temporalOutput,
    const common_render_data& common)
{
    if (!tlas.tlas)
    {
        return;
    }

    {
        PROFILE_ALL(cl, "Raytrace reflections");

        input_resources in;
        in.tlas = tlas.tlas->raytracingSRV;
        in.sky = common.sky->defaultSRV;
        in.probeIrradiance = common.lightProbeIrradiance->defaultSRV;
        in.probeDepth = common.lightProbeDepth->defaultSRV;
        in.depthBuffer = depthStencilBuffer->defaultSRV;
        in.worldNormalsAndRoughness = worldNormalsRoughnessTexture->defaultSRV;
        in.noise = render_resources::noiseTexture->defaultSRV;
        in.screenVelocitiesTexture = screenVelocitiesTexture->defaultSRV;

        output_resources out;
        out.output = resolveTexture->defaultUAV;


        dx_gpu_descriptor_handle gpuHandle = copyGlobalResourcesToDescriptorHeap(in, out);


        // Fill out description.
        D3D12_DISPATCH_RAYS_DESC raytraceDesc;
        fillOutRayTracingRenderDesc(bindingTable.getBuffer(), raytraceDesc,
            resolveTexture->width, resolveTexture->height, 1,
            numRayTypes, bindingTable.getNumberOfHitGroups());

        rt_reflections_cb cb;
        cb.sampleSkyFromTexture = !common.proceduralSky;
        cb.frameIndex = (uint32)dxContext.frameID;


        // Set up pipeline.
        cl->setDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, descriptorHeap.descriptorHeap);

        cl->setPipelineState(pipeline.pipeline);
        cl->setComputeRootSignature(pipeline.rootSignature);

        cl->setComputeDescriptorTable(RT_REFLECTIONS_RS_RESOURCES, gpuHandle);
        cl->setCompute32BitConstants(RT_REFLECTIONS_RS_CB, cb);
        cl->setComputeDynamicConstantBuffer(RT_REFLECTIONS_RS_CAMERA, common.cameraCBV);
        cl->setComputeDynamicConstantBuffer(RT_REFLECTIONS_RS_LIGHTING, common.lightingCBV);

        cl->raytrace(raytraceDesc);

        cl->resetToDynamicDescriptorHeap();
    }

    {
        void ssrTemporal(dx_command_list* cl,
            ref<dx_texture> screenVelocitiesTexture,
            ref<dx_texture> resolveTexture,
            ref<dx_texture> ssrTemporalHistory,
            ref<dx_texture> ssrTemporalOutput);

        barrier_batcher(cl)
            //.uav(resolveTexture)
            .transition(resolveTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        ssrTemporal(cl, screenVelocitiesTexture, resolveTexture, temporalHistory, temporalOutput);
    }
}
