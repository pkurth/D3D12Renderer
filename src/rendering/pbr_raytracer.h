#pragma once

#include "raytracer.h"
#include "material.hlsli"
#include "pbr.h"


struct pbr_raytracer : dx_raytracer
{
    static raytracing_object_type defineObjectType(const ref<raytracing_blas>& blas, const std::vector<ref<pbr_material>>& materials);
    void finalizeForRender();

protected:

    void initialize(const wchar* shaderPath, uint32 maxPayloadSize, uint32 maxRecursionDepth, const D3D12_ROOT_SIGNATURE_DESC& globalDesc);
 
    struct shader_data // This struct is 32 bytes large, which together with the 32 byte shader identifier is a nice multiple of the required 32-byte-alignment of the binding table entries.
    {
        pbr_material_cb materialCB;
        dx_cpu_descriptor_handle resources; // Vertex buffer, index buffer, PBR textures.
    };

    static_assert(sizeof(pbr_material_cb) == 24);
    static_assert(sizeof(shader_data) == 32);


    static inline uint32 numRayTypes = 0;

    raytracing_binding_table<shader_data> bindingTable;

    static inline dx_pushable_descriptor_heap descriptorHeap;

private:
    static void pushTexture(const ref<dx_texture>& tex, uint32& flags, uint32 flag);

    static inline uint32 instanceContributionToHitGroupIndex = 0;

    bool dirty = true;
    static inline std::vector<pbr_raytracer*> registeredPBRRaytracers;
};
