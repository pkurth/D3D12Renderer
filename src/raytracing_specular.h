#pragma once

#include "raytracer.h"

#include "material.hlsli"
#include "pbr.h"

struct specular_reflections_raytracer : dx_raytracer
{
    void initialize();
    raytracing_object_type defineObjectType(const ref<raytracing_blas>& blas, const std::vector<ref<pbr_material>>& materials);

    void finish();

    void render(dx_command_list* cl, const raytracing_tlas& tlas,
        const ref<dx_texture>& output,
        const common_material_info& materialInfo) override;


    // Parameters. Can be changed in each frame.
    uint32 numBounces = 1;
    float fadeoutDistance = 80.f; 
    float maxDistance = 100.f;

private:
    struct shader_data
    {
        // Only set in radiance hit.
        pbr_material_cb materialCB;
        dx_cpu_descriptor_handle resources; // Vertex buffer, index buffer, pbr textures.
    };

    // Only descriptors in here!
    struct input_resources
    {
        dx_cpu_descriptor_handle tlas;
        dx_cpu_descriptor_handle depthBuffer;
        dx_cpu_descriptor_handle screenSpaceNormals;
        dx_cpu_descriptor_handle irradiance;
        dx_cpu_descriptor_handle environment;
        dx_cpu_descriptor_handle sky;
        dx_cpu_descriptor_handle brdf;
    };

    struct output_resources
    {
        dx_cpu_descriptor_handle output;
    };

    dx_pushable_resource_descriptor_heap descriptorHeap;

    uint32 instanceContributionToHitGroupIndex = 0;
    uint32 numRayTypes;

    const uint32 maxRecursionDepth = 4;

    raytracing_binding_table<shader_data> bindingTable;
};
