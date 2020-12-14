#pragma once

#include "raytracing.h"
#include "pbr.h"
#include "dx_texture.h"

#define MAX_PBR_RAYTRACING_RECURSION_DEPTH         4

struct pbr_raytracing_pipeline
{
    static const uint32 numRayTypes = 2; // Currently all pbr pipelines must support radiance and shadow rays.

    virtual uint32 getNumberOfRequiredResources() { return 0; }

    dx_raytracing_pipeline pipeline;

protected:
    void fillOutRayTracingRenderDesc(struct pbr_raytracing_binding_table& bindingTable, D3D12_DISPATCH_RAYS_DESC& raytraceDesc, uint32 renderWidth, uint32 renderHeight, uint32 renderDepth, uint32 numRayTypes, uint32 numHitGroups);
};

struct pbr_specular_reflections_raytracing_pipeline : pbr_raytracing_pipeline
{
	void initialize(const wchar* shaderPath);

    void render(struct dx_command_list* cl, 
        struct pbr_raytracing_binding_table& bindingTable, const struct raytracing_tlas& tlas,
        const ref<dx_texture>& output,
        uint32 numBounces,
        float fadeoutDistance, float maxDistance,
        float environmentIntensity, float skyIntensity,
        dx_dynamic_constant_buffer cameraCBV, dx_dynamic_constant_buffer sunCBV,
        const ref<dx_texture>& depthBuffer, const ref<dx_texture>& normalMap,
        const ref<pbr_environment>& environment, const ref<dx_texture>& brdf);


    virtual uint32 getNumberOfRequiredResources() override { return 6; }  // Depth buffer, normal map, global irradiance, environment, sky, brdf.
};
