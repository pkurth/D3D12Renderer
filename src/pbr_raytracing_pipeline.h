#pragma once

#include "raytracing.h"
#include "pbr.h"

#include "material.hlsli"

#define MAX_PBR_RAYTRACING_RECURSION_DEPTH         4

struct pbr_raytracing_pipeline
{
    void initialize(const wchar* shaderPath);

    raytracing_object_handle defineObjectType(const raytracing_blas& blas, const std::vector<ref<pbr_material>>& materials);
    void build();

    void render(struct dx_command_list* cl, const struct raytracing_tlas& tlas,
        const ref<dx_texture>& output,
        uint32 numBounces,
        float fadeoutDistance, float maxDistance,
        float environmentIntensity, float skyIntensity,
        dx_dynamic_constant_buffer cameraCBV, dx_dynamic_constant_buffer sunCBV,
        const ref<dx_texture>& depthBuffer, const ref<dx_texture>& normalMap,
        const ref<pbr_environment>& environment, const ref<dx_texture>& brdf);


    virtual ~pbr_raytracing_pipeline() { if (bindingTable) { free(bindingTable); } }

private:
    void allocateBindingTableBuffer();
    void fillOutRayTracingRenderDesc(D3D12_DISPATCH_RAYS_DESC& raytraceDesc, uint32 renderWidth, uint32 renderHeight, uint32 renderDepth, uint32 numRayTypes, uint32 numHitGroups);

    dx_gpu_descriptor_handle setTLASHandle(dx_cpu_descriptor_handle cpuHandle);
    dx_gpu_descriptor_handle setOutputTexture(const ref<dx_texture>& output);
    dx_gpu_descriptor_handle setTextures(const ref<dx_texture>* textures);


    dx_raytracing_pipeline pipeline;
    ref<dx_buffer> bindingTableBuffer;

    uint32 reservedDescriptorsAtStart;

    com<ID3D12DescriptorHeap> descriptorHeap;
    dx_cpu_descriptor_handle cpuCurrentDescriptorHandle;

    dx_gpu_descriptor_handle gpuBaseDescriptorHandle;
    dx_cpu_descriptor_handle cpuBaseDescriptorHandle;



    uint32 maxNumHitGroups;


    static const uint32 numRayTypes = 2;

    struct alignas(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT) binding_table_entry
    {
        uint8 identifier[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES];

        // Only set in radiance hit.
        pbr_material_cb materialCB;
        dx_cpu_descriptor_handle srvRange; // Vertex buffer, index buffer, pbr textures.
    };

    struct binding_table
    {
        alignas(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT) binding_table_entry raygen;
        alignas(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT) binding_table_entry miss[numRayTypes];

        alignas(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT) binding_table_entry hit[1]; // Dynamically allocated.
    };

    binding_table* bindingTable = 0;
    uint32 totalBindingTableSize;
    uint32 numHitGroups = 0;

    uint32 instanceContributionToHitGroupIndex = 0;
};
