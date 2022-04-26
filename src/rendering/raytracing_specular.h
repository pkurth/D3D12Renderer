#pragma once

#include "pbr_raytracer.h"

struct specular_reflections_raytracer : pbr_raytracer
{
    void initialize();

    void render(dx_command_list* cl, const raytracing_tlas& tlas,
        const ref<dx_texture>& output,
        const common_material_info& materialInfo);


    // Parameters. Can be changed in each frame.
    uint32 numBounces = 1;
    float fadeoutDistance = 80.f; 
    float maxDistance = 100.f;

private:

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

    const uint32 maxRecursionDepth = 4;
};
