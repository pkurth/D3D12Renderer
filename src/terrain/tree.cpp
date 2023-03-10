#include "pch.h"
#include "tree.h"

#include "core/color.h"
#include "core/nearest_neighbor.h"

#include "dx/dx_pipeline.h"

#include "rendering/render_resources.h"
#include "rendering/pbr.h"

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

PIPELINE_SETUP_IMPL(tree_pipeline)
{
	cl->setPipelineState(*treePipeline.pipeline);
	cl->setGraphicsRootSignature(*treePipeline.rootSignature);

	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

PIPELINE_RENDER_IMPL(tree_pipeline)
{
    const mat4& m = rc.data.transform;
    const submesh_info& submesh = rc.data.submesh;

    cl->setGraphics32BitConstants(TREE_RS_MVP, transform_cb{ viewProj * m, m });

    cl->setVertexBuffer(0, rc.data.vertexBuffer.positions);
    cl->setVertexBuffer(1, rc.data.vertexBuffer.others);
    cl->setIndexBuffer(rc.data.indexBuffer);
    cl->drawIndexed(submesh.numIndices, 1, submesh.firstIndex, submesh.baseVertex, 0);
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

static void analyzeTreeMesh(mesh_builder& builder, std::vector<submesh>& submeshes)
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


    for (auto& sub : submeshes)
    {
        for (uint32 i = 0; i < sub.info.numVertices; ++i)
        {
            uint32 vertexID = i + sub.info.baseVertex;

            vec3 query = positions[vertexID];

            float distanceToTrunk = sqrt(trunkPC.nearestNeighborIndex(query).squaredDistance);
            float distanceToBranch = sqrt(branchPC.nearestNeighborIndex(query).squaredDistance);

            distanceToBranch = min(distanceToTrunk, distanceToBranch);

            others[vertexID].color = packColor(query.y / 100.f, distanceToTrunk / 100.f, distanceToBranch / 100.f, 1.f);
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
