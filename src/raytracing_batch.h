#pragma once

#include "raytracing.h"
#include "dx_descriptor.h"
#include "pbr.h"



struct raytracing_batch
{
    void build();
    virtual void render(struct dx_command_list* cl, const ref<dx_texture>& output, dx_dynamic_constant_buffer cameraCBV) = 0;

protected:
    void fillOutRayTracingRenderDesc(D3D12_DISPATCH_RAYS_DESC& raytraceDesc, uint32 renderWidth, uint32 renderHeight, uint32 numRayTypes);
    virtual void buildBindingTable() = 0;

    com<ID3D12DescriptorHeap> descriptorHeap;
    dx_cpu_descriptor_handle cpuBaseDescriptorHandle;
    dx_gpu_descriptor_handle gpuBaseDescriptorHandle;
    dx_cpu_descriptor_handle cpuCurrentDescriptorHandle;

    dx_raytracing_pipeline pipeline;
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> allInstances;

    raytracing_tlas tlas;
    ref<dx_buffer> bindingTableBuffer;

    friend struct dx_renderer;
};

struct pbr_raytracing_batch : raytracing_batch
{
    void initialize(uint32 maxNumObjectTypes = 0);

    void beginObjectType(const raytracing_blas& blas, const std::vector<ref<pbr_material>>& materials);
    void pushInstance(const trs& transform);
    virtual void buildBindingTable() override;

    virtual void render(struct dx_command_list* cl, const ref<dx_texture>& output, dx_dynamic_constant_buffer cameraCBV) override;

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


    uint32 currentInstanceContributionToHitGroupIndex = 0;
    D3D12_GPU_VIRTUAL_ADDRESS currentBlas;

    uint32 instanceContributionToHitGroupIndex = 0;
};
