#pragma once

#include "raytracing.h"
#include "dx_descriptor.h"
#include "pbr.h"



#define MAX_RAYTRACING_RECURSION_DEPTH         4

struct raytracing_object_handle
{
    D3D12_GPU_VIRTUAL_ADDRESS blas;
    uint32 instanceContributionToHitGroupIndex;
};

struct raytracing_instance_handle
{
    uint32 instanceIndex;
};

struct raytracing_batch
{
    raytracing_instance_handle instantiate(raytracing_object_handle type, const trs& transform);
    void updateInstanceTransform(raytracing_instance_handle handle, const trs& transform);

    void buildAccelerationStructure();
    virtual void buildBindingTable() = 0;

    void buildAll();

    virtual void render(struct dx_command_list* cl, const ref<dx_texture>& output, uint32 numBounces, dx_dynamic_constant_buffer cameraCBV, dx_dynamic_constant_buffer sunCBV) = 0;

protected:
    void fillOutRayTracingRenderDesc(D3D12_DISPATCH_RAYS_DESC& raytraceDesc, uint32 renderWidth, uint32 renderHeight, uint32 numRayTypes);

    com<ID3D12DescriptorHeap> descriptorHeap;
    dx_cpu_descriptor_handle cpuBaseDescriptorHandle;
    dx_gpu_descriptor_handle gpuBaseDescriptorHandle;
    dx_cpu_descriptor_handle cpuCurrentDescriptorHandle;

    dx_raytracing_pipeline pipeline;
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> allInstances;

    raytracing_tlas tlas;
    ref<dx_buffer> bindingTableBuffer;

    uint32 tlasDescriptorIndex = 0;

    acceleration_structure_rebuild_mode rebuildMode;
};

struct pbr_raytracing_batch : raytracing_batch
{
    void initialize(uint32 maxNumObjectTypes = 0, acceleration_structure_rebuild_mode rebuildMode = acceleration_structure_rebuild);

    raytracing_object_handle defineObjectType(const raytracing_blas& blas, const std::vector<ref<pbr_material>>& materials);
    virtual void buildBindingTable() override;

    virtual void render(struct dx_command_list* cl, const ref<dx_texture>& output, uint32 numBounces, dx_dynamic_constant_buffer cameraCBV, dx_dynamic_constant_buffer sunCBV) override;

private:
    struct raygen_table_entry
    {
    };
    struct miss_table_entry
    {
    };
    struct radiance_hit_entry
    {
        dx_cpu_descriptor_handle srvRange; // Vertex buffer, index buffer, pbr textures.
    };
    struct shadow_hit_entry
    {
    };

    const uint32 numRayTypes = 2;

    struct alignas(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT) binding_table_entry
    {
        uint8 identifier[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES];

        union
        {
            raygen_table_entry raygen;
            miss_table_entry miss;
            radiance_hit_entry radianceHit;
            shadow_hit_entry shadowHit;
        };
    };

    std::vector<binding_table_entry> bindingTable;

    uint32 instanceContributionToHitGroupIndex = 0;
};
