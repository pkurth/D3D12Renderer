#include "pch.h"
#include "skinning.h"
#include "dx_command_list.h"

#include "skinning_rs.hlsli"

#define MAX_NUM_SKINNING_MATRICES_PER_FRAME 4096
#define MAX_NUM_SKINNED_VERTICES_PER_FRAME (1 << 20)

static ref<dx_buffer> skinningMatricesBuffer[NUM_BUFFERED_FRAMES];

static uint32 currentSkinnedVertexBuffer;
static ref<dx_vertex_buffer> skinnedVertexBuffer[2]; // We have two of these, so that we can compute screen space velocities.

static dx_pipeline skinningPipeline;


struct skinning_call
{
	ref<dx_vertex_buffer> vertexBuffer;
	vertex_range range;
	uint32 jointOffset;
	uint32 numJoints;
	uint32 vertexOffset;
};

static std::vector<skinning_call> calls;
static std::vector<mat4> skinningMatrices;
static uint32 totalNumVertices;


void initializeSkinning()
{
	for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; ++i)
	{
		skinningMatricesBuffer[i] = createUploadBuffer(sizeof(mat4), MAX_NUM_SKINNING_MATRICES_PER_FRAME, 0);
	}

	for (uint32 i = 0; i < 2; ++i)
	{
		skinnedVertexBuffer[i] = createVertexBuffer(getVertexSize(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents),
			MAX_NUM_SKINNED_VERTICES_PER_FRAME, 0, true);
	}

	skinningPipeline = createReloadablePipeline("skinning_cs");

	skinningMatrices.reserve(MAX_NUM_SKINNING_MATRICES_PER_FRAME);
}

std::tuple<ref<dx_vertex_buffer>, vertex_range, mat4*> skinObject(const ref<dx_vertex_buffer>& vertexBuffer, vertex_range range, uint32 numJoints)
{
	uint32 offset = (uint32)skinningMatrices.size();

	assert(offset + numJoints <= skinningMatrices.capacity());

	skinningMatrices.resize(skinningMatrices.size() + numJoints);

	calls.push_back(
		{
			vertexBuffer,
			range,
			offset,
			numJoints,
			totalNumVertices
		}
	);

	vertex_range resultRange;
	resultRange.numVertices = range.numVertices;
	resultRange.firstVertex = totalNumVertices;

	totalNumVertices += range.numVertices;

	return { skinnedVertexBuffer[currentSkinnedVertexBuffer], resultRange, skinningMatrices.data() + offset };
}

std::tuple<ref<dx_vertex_buffer>, uint32, mat4*> skinObject(const ref<dx_vertex_buffer>& vertexBuffer, uint32 numJoints)
{
	auto [vb, range, mats] = skinObject(vertexBuffer, vertex_range{ 0, vertexBuffer->elementCount }, numJoints);

	return { vb, range.firstVertex, mats };
}

std::tuple<ref<dx_vertex_buffer>, submesh_info, mat4*> skinObject(const ref<dx_vertex_buffer>& vertexBuffer, submesh_info submesh, uint32 numJoints)
{
	auto [vb, range, mats] = skinObject(vertexBuffer, vertex_range{ submesh.baseVertex, submesh.numVertices }, numJoints);

	submesh_info resultInfo;
	resultInfo.firstTriangle = submesh.firstTriangle;
	resultInfo.numTriangles = submesh.numTriangles;
	resultInfo.baseVertex = range.firstVertex;
	resultInfo.numVertices = range.numVertices;

	return { vb, resultInfo, mats };
}

bool performSkinning()
{
	bool result = false;
	if (calls.size() > 0)
	{
		dx_command_list* cl = dxContext.getFreeComputeCommandList(true);

		mat4* mats = (mat4*)mapBuffer(skinningMatricesBuffer[dxContext.bufferedFrameID]);
		memcpy(mats, skinningMatrices.data(), sizeof(mat4) * skinningMatrices.size());
		unmapBuffer(skinningMatricesBuffer[dxContext.bufferedFrameID]);


		cl->setPipelineState(*skinningPipeline.pipeline);
		cl->setComputeRootSignature(*skinningPipeline.rootSignature);

		cl->setDescriptorHeapSRV(SKINNING_RS_SRV_UAV, 0, skinningMatricesBuffer[dxContext.bufferedFrameID]);
		cl->setDescriptorHeapUAV(SKINNING_RS_SRV_UAV, 1, skinnedVertexBuffer[currentSkinnedVertexBuffer]);

		for (const auto& c : calls)
		{
			cl->setRootComputeSRV(SKINNING_RS_INPUT_VERTEX_BUFFER, c.vertexBuffer->gpuVirtualAddress);

			cl->setCompute32BitConstants(SKINNING_RS_CB, skinning_cb{ c.jointOffset, c.numJoints, c.range.firstVertex, c.range.numVertices, c.vertexOffset });

			cl->dispatch(bucketize(c.range.numVertices, 512));
		}

		cl->uavBarrier(skinnedVertexBuffer[currentSkinnedVertexBuffer]);

		dxContext.executeCommandList(cl);

		result = true;
	}

	currentSkinnedVertexBuffer = 1 - currentSkinnedVertexBuffer;
	calls.clear();
	skinningMatrices.clear();
	totalNumVertices = 0;

	return result;
}


