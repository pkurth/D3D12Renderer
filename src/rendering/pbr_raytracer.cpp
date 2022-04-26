#include "pch.h"
#include "pbr_raytracer.h"



void pbr_raytracer::initialize(const dx_raytracing_pipeline& pipeline)
{
    numRayTypes = (uint32)pipeline.shaderBindingTableDesc.hitGroups.size();
    
    bindingTable.initialize(&pipeline);
    descriptorHeap.initialize(2048); // TODO.
}

void pbr_raytracer::beginFrame()
{
    instanceContributionToHitGroupIndex = 0;
    descriptorHeap.reset();
}

void pbr_raytracer::pushTexture(const ref<dx_texture>& tex, uint32& flags, uint32 flag)
{
    if (tex)
    {
        descriptorHeap.push().create2DTextureSRV(tex);
        flags |= flag;
    }
    else
    {
        descriptorHeap.push().createNullTextureSRV();
    }
}

raytracing_object_type pbr_raytracer::defineObjectType(const ref<raytracing_blas>& blas, const std::vector<ref<pbr_material>>& materials)
{
    uint32 numGeometries = (uint32)blas->geometries.size();

    shader_data* hitData = (shader_data*)alloca(sizeof(shader_data) * numRayTypes);

    for (uint32 i = 0; i < numGeometries; ++i)
    {
        assert(blas->geometries[i].type == raytracing_mesh_geometry); // For now we only support meshes, not procedurals.

        submesh_info submesh = blas->geometries[i].submesh;
        const ref<pbr_material>& material = materials[i];

        dx_cpu_descriptor_handle base = descriptorHeap.currentCPU;

        descriptorHeap.push().createBufferSRV(blas->geometries[i].vertexBuffer.others, { submesh.baseVertex, submesh.numVertices });
        descriptorHeap.push().createRawBufferSRV(blas->geometries[i].indexBuffer, { submesh.firstIndex, submesh.numIndices });

        uint32 flags = 0;
        pushTexture(material->albedo, flags, USE_ALBEDO_TEXTURE);
        pushTexture(material->normal, flags, USE_NORMAL_TEXTURE);
        pushTexture(material->roughness, flags, USE_ROUGHNESS_TEXTURE);
        pushTexture(material->metallic, flags, USE_METALLIC_TEXTURE);

        hitData[0].materialCB.initialize(
            material->albedoTint,
            material->emission.xyz,
            material->roughnessOverride,
            material->metallicOverride,
            flags
        );
        hitData[0].resources = base;

        // The other shader types don't need any data, so we don't set it here.

        bindingTable.push(hitData);
    }


    raytracing_object_type result = { blas, instanceContributionToHitGroupIndex };

    instanceContributionToHitGroupIndex += numGeometries * numRayTypes;

    dirty = true;

    return result;
}

void pbr_raytracer::finalizeForRender()
{
    if (dirty)
    {
        bindingTable.build();
        dirty = false;
    }
}
