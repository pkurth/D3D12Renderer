#pragma once

#include "pbr_raytracer.h"

struct path_tracer : pbr_raytracer
{
    void initialize();

    void render(dx_command_list* cl, const raytracing_tlas& tlas,
        const ref<dx_texture>& output,
        const common_render_data& common);

    void resetRendering();


    const uint32 maxRecursionDepth = 4;
    const uint32 maxPayloadSize = 5 * sizeof(float); // Radiance-payload is 1 x float3, 2 x uint.



    // Parameters.

    uint32 numAveragedFrames = 0;
    uint32 recursionDepth = maxRecursionDepth - 1; // [0, maxRecursionDepth - 1]. 0 and 1 don't really make sense. 0 means, that no primary ray is shot. 1 means that no bounce is computed, which leads to 0 light reaching the primary hit.
    uint32 startRussianRouletteAfter = recursionDepth; // [0, recursionDepth].

    bool useThinLensCamera = false;
    float fNumber = 32.f;
    float focalLength = 1.f;

    bool useRealMaterials = false;
    bool enableDirectLighting = false;
    float lightIntensityScale = 1.f;
    float pointLightRadius = 0.1f;

    bool multipleImportanceSampling = true;


private:

    // Only descriptors in here!
    struct input_resources
    {
        dx_cpu_descriptor_handle tlas;
        dx_cpu_descriptor_handle sky;
    };

    struct output_resources
    {
        dx_cpu_descriptor_handle output;
    };
};
