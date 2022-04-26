#pragma once

#include "raytracer.h"
#include "material.hlsli"
#include "pbr.h"


struct pbr_raytracer : dx_raytracer
{
    raytracing_object_type defineObjectType(const ref<raytracing_blas>& blas, const std::vector<ref<pbr_material>>& materials);
    void finalizeForRender();

protected:

    void initialize();
 
    struct shader_data // This struct is 32 bytes large, which together with the 32 byte shader identifier is a nice multiple of the required 32-byte-alignment of the binding table entries.
    {
        pbr_material_cb materialCB;
        dx_cpu_descriptor_handle resources; // Vertex buffer, index buffer, PBR textures.
    };


    bool dirty = true;

    // TODO: The descriptor heap shouldn't be a member of this structure. If we have multiple pbr raytracers, they can share the descriptor heap.
    dx_pushable_descriptor_heap descriptorHeap;

    uint32 instanceContributionToHitGroupIndex = 0;
    uint32 numRayTypes;

    raytracing_binding_table<shader_data> bindingTable;


private:
    void pushTexture(const ref<dx_texture>& tex, uint32& flags, uint32 flag);

};
