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
    // Maybe we want to rebuild this every frame anyway.
    raytracing_instance_handle instantiate(raytracing_object_handle type, const trs& transform);
    void updateInstanceTransform(raytracing_instance_handle handle, const trs& transform);

    void buildAccelerationStructure();
    virtual void buildBindingTable() = 0;

    void buildAll();

protected:
    void initialize(acceleration_structure_rebuild_mode rebuildMode, uint32 reserveDescriptorsAtStart);

    void fillOutRayTracingRenderDesc(D3D12_DISPATCH_RAYS_DESC& raytraceDesc, uint32 renderWidth, uint32 renderHeight, uint32 numRayTypes);

    dx_gpu_descriptor_handle getTLASHandle();
    dx_gpu_descriptor_handle setOutputTexture(const ref<dx_texture>& output);
    dx_gpu_descriptor_handle setTextures(const ref<dx_texture>* textures);


    dx_raytracing_pipeline pipeline;
    ref<dx_buffer> bindingTableBuffer;

    com<ID3D12DescriptorHeap> descriptorHeap;
    dx_cpu_descriptor_handle cpuCurrentDescriptorHandle;

private:
    raytracing_tlas tlas;

    uint32 tlasDescriptorIndex = 0;
    uint32 reservedDescriptorsAtStart;
    acceleration_structure_rebuild_mode rebuildMode;
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> allInstances;

    dx_gpu_descriptor_handle gpuBaseDescriptorHandle;
    dx_cpu_descriptor_handle cpuBaseDescriptorHandle;
};

struct pbr_raytracing_batch : raytracing_batch
{
    raytracing_object_handle defineObjectType(const raytracing_blas& blas, const std::vector<ref<pbr_material>>& materials);
    virtual void buildBindingTable() override;

    void render(struct dx_command_list* cl, const ref<dx_texture>& output, uint32 numBounces, float environmentIntensity, float skyIntensity,
        dx_dynamic_constant_buffer cameraCBV, dx_dynamic_constant_buffer sunCBV, 
        const ref<dx_texture>& depthBuffer, const ref<dx_texture>& normalMap,
        const ref<pbr_environment>& environment, const ref<dx_texture>& brdf);

protected:
    void initialize(const wchar* shaderName, uint32 maxNumObjectTypes, acceleration_structure_rebuild_mode rebuildMode);

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

struct specular_reflections_raytracing_batch : pbr_raytracing_batch
{
    void initialize(uint32 maxNumObjectTypes = 32, acceleration_structure_rebuild_mode rebuildMode = acceleration_structure_rebuild);
};
