#pragma once

#include "raytracing.h"
#include "pbr.h"
#include "pbr_raytracing_pipeline.h"

#include "material.hlsli"

struct pbr_raytracing_binding_table
{
    void initialize();

    raytracing_object_type defineObjectType(const ref<raytracing_blas>& blas, const std::vector<ref<pbr_material>>& materials);
    void build();

    virtual ~pbr_raytracing_binding_table() { if (bindingTable) { free(bindingTable); } }


    // Called from pipeline.
    dx_gpu_descriptor_handle setTLASHandle(dx_cpu_descriptor_handle cpuHandle);
    dx_gpu_descriptor_handle setOutputTexture(const ref<dx_texture>& output);
    dx_gpu_descriptor_handle setTextures(const ref<dx_texture>* textures);

    ref<dx_buffer> getBindingTableBuffer() { return bindingTableBuffer; }
    uint32 getNumberOfHitGroups() { return numHitGroups; }
    com<ID3D12DescriptorHeap> getDescriptorHeap() { return descriptorHeap; }

private:
    void allocateBindingTableBuffer();

    struct pbr_raytracing_pipeline* pipeline;
    ref<dx_buffer> bindingTableBuffer;

    uint32 reservedDescriptorsAtStart;

    com<ID3D12DescriptorHeap> descriptorHeap;
    dx_cpu_descriptor_handle cpuCurrentDescriptorHandle;

    dx_gpu_descriptor_handle gpuBaseDescriptorHandle;
    dx_cpu_descriptor_handle cpuBaseDescriptorHandle;


    uint32 maxNumHitGroups;


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
        alignas(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT) binding_table_entry miss[pbr_raytracing_pipeline::numRayTypes];

        alignas(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT) binding_table_entry hit[1]; // Dynamically allocated.
    };

    binding_table* bindingTable = 0;
    uint32 totalBindingTableSize;
    uint32 numHitGroups = 0;

    uint32 instanceContributionToHitGroupIndex = 0;
};
