#include "pch.h"
#include "tree.h"

#include "core/color.h"
#include "core/nearest_neighbor.h"

#include "dx/dx_pipeline.h"

#include "rendering/render_resources.h"
#include "rendering/pbr.h"
#include "rendering/render_pass.h"

#include "tree_rs.hlsli"



static dx_pipeline treePipeline;


void initializeTreePipelines()
{
    {
        auto desc = CREATE_GRAPHICS_PIPELINE
            .inputLayout(inputLayout_position_uv_normal_tangent_colors)
            //.depthSettings(true, false, D3D12_COMPARISON_FUNC_EQUAL)
            .cullingOff()
            .renderTargets(opaqueLightPassFormats, OPQAUE_LIGHT_PASS_NO_VELOCITIES_NO_OBJECT_ID, depthStencilFormat);

        treePipeline = createReloadablePipeline(desc, { "tree_vs", "tree_ps" });
    }
}


struct tree_render_data
{
    D3D12_GPU_VIRTUAL_ADDRESS transformPtr;
    dx_vertex_buffer_group_view vertexBuffer;
    dx_index_buffer_view indexBuffer;
    submesh_info submesh;

    ref<pbr_material> material;

    float time;

    uint32 numInstances;
};

struct tree_pipeline
{
    PIPELINE_SETUP_DECL
    {
        cl->setPipelineState(*treePipeline.pipeline);
        cl->setGraphicsRootSignature(*treePipeline.rootSignature);

        cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        dx_cpu_descriptor_handle nullTexture = render_resources::nullTextureSRV;
        dx_cpu_descriptor_handle nullBuffer = render_resources::nullBufferSRV;

        cl->setDescriptorHeapSRV(TREE_RS_FRAME_CONSTANTS, 0, common.irradiance ? common.irradiance->defaultSRV : nullTexture);
        cl->setDescriptorHeapSRV(TREE_RS_FRAME_CONSTANTS, 1, common.prefilteredRadiance ? common.prefilteredRadiance->defaultSRV : nullTexture);
        cl->setDescriptorHeapSRV(TREE_RS_FRAME_CONSTANTS, 2, render_resources::brdfTex);
        cl->setDescriptorHeapSRV(TREE_RS_FRAME_CONSTANTS, 3, common.tiledCullingGrid ? common.tiledCullingGrid->defaultSRV : nullTexture);
        cl->setDescriptorHeapSRV(TREE_RS_FRAME_CONSTANTS, 4, common.tiledObjectsIndexList ? common.tiledObjectsIndexList->defaultSRV : nullBuffer);
        cl->setDescriptorHeapSRV(TREE_RS_FRAME_CONSTANTS, 5, common.pointLightBuffer ? common.pointLightBuffer->defaultSRV : nullBuffer);
        cl->setDescriptorHeapSRV(TREE_RS_FRAME_CONSTANTS, 6, common.spotLightBuffer ? common.spotLightBuffer->defaultSRV : nullBuffer);
        cl->setDescriptorHeapSRV(TREE_RS_FRAME_CONSTANTS, 7, common.decalBuffer ? common.decalBuffer->defaultSRV : nullBuffer);
        cl->setDescriptorHeapSRV(TREE_RS_FRAME_CONSTANTS, 8, common.shadowMap ? common.shadowMap->defaultSRV : nullTexture);
        cl->setDescriptorHeapSRV(TREE_RS_FRAME_CONSTANTS, 9, common.pointLightShadowInfoBuffer ? common.pointLightShadowInfoBuffer->defaultSRV : nullBuffer);
        cl->setDescriptorHeapSRV(TREE_RS_FRAME_CONSTANTS, 10, common.spotLightShadowInfoBuffer ? common.spotLightShadowInfoBuffer->defaultSRV : nullBuffer);
        cl->setDescriptorHeapSRV(TREE_RS_FRAME_CONSTANTS, 11, common.decalTextureAtlas ? common.decalTextureAtlas->defaultSRV : nullTexture);
        cl->setDescriptorHeapSRV(TREE_RS_FRAME_CONSTANTS, 12, common.aoTexture ? common.aoTexture : render_resources::whiteTexture);
        cl->setDescriptorHeapSRV(TREE_RS_FRAME_CONSTANTS, 13, common.sssTexture ? common.sssTexture : render_resources::whiteTexture);
        cl->setDescriptorHeapSRV(TREE_RS_FRAME_CONSTANTS, 14, common.ssrTexture ? common.ssrTexture->defaultSRV : nullTexture);
        cl->setDescriptorHeapSRV(TREE_RS_FRAME_CONSTANTS, 15, common.lightProbeIrradiance ? common.lightProbeIrradiance->defaultSRV : nullTexture);
        cl->setDescriptorHeapSRV(TREE_RS_FRAME_CONSTANTS, 16, common.lightProbeDepth ? common.lightProbeDepth->defaultSRV : nullTexture);

        cl->setGraphicsDynamicConstantBuffer(TREE_RS_CAMERA, common.cameraCBV);
        cl->setGraphicsDynamicConstantBuffer(TREE_RS_LIGHTING, common.lightingCBV);
    }

    PIPELINE_RENDER_DECL(tree_render_data)
    {
        const auto& mat = data.material;

        uint32 flags = 0;

        if (mat->albedo)
        {
            cl->setDescriptorHeapSRV(TREE_RS_PBR_TEXTURES, 0, mat->albedo);
            flags |= MATERIAL_USE_ALBEDO_TEXTURE;
        }
        if (mat->normal)
        {
            cl->setDescriptorHeapSRV(TREE_RS_PBR_TEXTURES, 1, mat->normal);
            flags |= MATERIAL_USE_NORMAL_TEXTURE;
        }
        if (mat->roughness)
        {
            cl->setDescriptorHeapSRV(TREE_RS_PBR_TEXTURES, 2, mat->roughness);
            flags |= MATERIAL_USE_ROUGHNESS_TEXTURE;
        }
        if (mat->metallic)
        {
            cl->setDescriptorHeapSRV(TREE_RS_PBR_TEXTURES, 3, mat->metallic);
            flags |= MATERIAL_USE_METALLIC_TEXTURE;
        }
        flags |= MATERIAL_DOUBLE_SIDED; // Always double sided.

        cl->setGraphics32BitConstants(TREE_RS_MATERIAL,
            pbr_material_cb(mat->albedoTint, mat->emission.xyz, mat->roughnessOverride, mat->metallicOverride, flags, 1.f, mat->translucency, mat->uvScale)
        );



        const submesh_info& submesh = data.submesh;

        cl->setRootGraphicsSRV(TREE_RS_TRANSFORM, data.transformPtr);
        cl->setGraphics32BitConstants(TREE_RS_CB, tree_cb{ data.time });

        cl->setVertexBuffer(0, data.vertexBuffer.positions);
        cl->setVertexBuffer(1, data.vertexBuffer.others);
        cl->setIndexBuffer(data.indexBuffer);
        cl->drawIndexed(submesh.numIndices, data.numInstances, submesh.firstIndex, submesh.baseVertex, 0);
    }
};

void renderTree(opaque_render_pass* renderPass, D3D12_GPU_VIRTUAL_ADDRESS transforms, uint32 numInstances, const multi_mesh* mesh, float dt)
{
    static float time = 0.f;
    time += dt;


    const dx_mesh& dxMesh = mesh->mesh;

    for (auto& sm : mesh->submeshes)
    {
        submesh_info submesh = sm.info;
        const ref<pbr_material>& material = sm.material;

        tree_render_data data;
        data.transformPtr = transforms;
        data.vertexBuffer = dxMesh.vertexBuffer;
        data.indexBuffer = dxMesh.indexBuffer;
        data.submesh = submesh;
        data.material = material;
        data.time = time;
        data.numInstances = numInstances;

        renderPass->renderObject<tree_pipeline>(data);
    }
}




struct tree_mesh_others
{
    vec2 uv;
    vec3 normal;
    vec3 tangent;
    uint32 color;
};

static uint32 countVertices(std::vector<submesh>& submeshes, const tree_mesh_others* others, uint32 color)
{
    uint32 count = 0;

    for (auto& sub : submeshes)
    {
        for (uint32 i = 0; i < sub.info.numVertices; ++i)
        {
            uint32 vertexID = i + sub.info.baseVertex;
            uint32 c = others[vertexID].color;

            count += c == color;
        }
    }

    return count;
}

static void fillVertices(std::vector<submesh>& submeshes, const vec3* positions, const tree_mesh_others* others, uint32 color, vec3* outPositions)
{
    for (auto& sub : submeshes)
    {
        for (uint32 i = 0; i < sub.info.numVertices; ++i)
        {
            uint32 vertexID = i + sub.info.baseVertex;
            uint32 c = others[vertexID].color;

            if (c == color)
            {
                *outPositions++ = positions[vertexID];
            }
        }
    }
}

static void analyzeTreeMesh(mesh_builder& builder, std::vector<submesh>& submeshes, const bounding_box& boundingBox)
{
    const uint32 trunkVertexColor = 0xFF000000;  // Black.
    const uint32 branchVertexColor = 0xFFFFFFFF; // White.

    vec3* positions = builder.getPositions();
    tree_mesh_others* others = (tree_mesh_others*)builder.getOthers();

    uint32 numTrunkVertices = countVertices(submeshes, others, trunkVertexColor);
    uint32 numBranchVertices = countVertices(submeshes, others, branchVertexColor);

    vec3* trunkPositions = new vec3[numTrunkVertices];
    vec3* branchPositions = new vec3[numBranchVertices];

    fillVertices(submeshes, positions, others, trunkVertexColor, trunkPositions);
    fillVertices(submeshes, positions, others, branchVertexColor, branchPositions);

    point_cloud trunkPC(trunkPositions, numTrunkVertices);
    point_cloud branchPC(branchPositions, numBranchVertices);


    float scale = 1.f / (boundingBox.maxCorner.y - boundingBox.minCorner.y);

    for (auto& sub : submeshes)
    {
        for (uint32 i = 0; i < sub.info.numVertices; ++i)
        {
            uint32 vertexID = i + sub.info.baseVertex;

            vec3 query = positions[vertexID];

            float distanceToTrunk = sqrt(trunkPC.nearestNeighborIndex(query).squaredDistance);
            float distanceToBranch = sqrt(branchPC.nearestNeighborIndex(query).squaredDistance);

            distanceToBranch = min(distanceToTrunk, distanceToBranch);

            others[vertexID].color = packColor(
                saturate(inverseLerp(boundingBox.minCorner.y, boundingBox.maxCorner.y, query.y)),
                distanceToTrunk * scale,
                distanceToBranch * scale,
                1.f);

            int a = 0;
        }
    }

    delete[] trunkPositions;
    delete[] branchPositions;
}

ref<multi_mesh> loadTreeMeshFromFile(const fs::path& sceneFilename)
{
    return loadMeshFromFile(sceneFilename, mesh_creation_flags_default | mesh_creation_flags_with_colors, analyzeTreeMesh);
}

ref<multi_mesh> loadTreeMeshFromHandle(asset_handle handle)
{
    return loadMeshFromHandle(handle, mesh_creation_flags_default | mesh_creation_flags_with_colors, analyzeTreeMesh);
}

ref<multi_mesh> loadTreeMeshFromFileAsync(const fs::path& sceneFilename, job_handle parentJob)
{
    return loadMeshFromFileAsync(sceneFilename, mesh_creation_flags_default | mesh_creation_flags_with_colors, parentJob, analyzeTreeMesh);
}

ref<multi_mesh> loadTreeMeshFromHandleAsync(asset_handle handle, job_handle parentJob)
{
    return loadMeshFromHandleAsync(handle, mesh_creation_flags_default | mesh_creation_flags_with_colors, parentJob, analyzeTreeMesh);
}

