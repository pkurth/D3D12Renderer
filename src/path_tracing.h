#pragma once

#include "raytracer.h"

#include "material.hlsli"
#include "pbr.h"

struct path_tracer : dx_raytracer
{
    void initialize();
    raytracing_object_type defineObjectType(const ref<raytracing_blas>& blas, const std::vector<ref<pbr_material>>& materials);

    void finish();

    void render(dx_command_list* cl, const raytracing_tlas& tlas,
        const ref<dx_texture>& output,
        const common_material_info& materialInfo) override;

    uint32 numAveragedFrames = 0;

private:
    struct shader_data
    {
        pbr_material_cb materialCB;
        dx_cpu_descriptor_handle resources; // Vertex buffer, index buffer, pbr textures.
    };

    struct global_resources
    {
        union
        {
            struct
            {
                // Must be first.
                dx_cpu_descriptor_handle output;


                dx_cpu_descriptor_handle tlas;
                dx_cpu_descriptor_handle sky;
            };

            dx_cpu_descriptor_handle resources[3];
        };

        dx_cpu_descriptor_handle cpuBase;
        dx_gpu_descriptor_handle gpuBase;
    };

    dx_pushable_resource_descriptor_heap descriptorHeap;

    uint32 instanceContributionToHitGroupIndex = 0;
    uint32 numRayTypes;

    const uint32 maxRecursionDepth = 4;
    const uint32 maxPayloadSize = 5 * sizeof(float);

    raytracing_binding_table<shader_data> bindingTable;

    global_resources globalResources[NUM_BUFFERED_FRAMES];
};
