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
    struct shader_data // This struct is 32 bytes large, which together with the 32 byte shader identifier is a nice multiple of the required 32-byte-alignment of the binding table entries.
    {
        pbr_material_cb materialCB;
        dx_cpu_descriptor_handle resources; // Vertex buffer, index buffer, PBR textures.
    };

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



    // TODO: The descriptor heap shouldn't be a member of this structure. If we have multiple raytracers which use the same object types, they can share the descriptor heap.
    // For example, this path tracer defines objects with vertex buffer, index buffer and their PBR textures. Other raytracers, which use the same layout (e.g. a specular reflections
    // raytracer) may very well use the same descriptor heap.
    dx_pushable_resource_descriptor_heap descriptorHeap;

    uint32 instanceContributionToHitGroupIndex = 0;
    uint32 numRayTypes;

    raytracing_binding_table<shader_data> bindingTable;
};
