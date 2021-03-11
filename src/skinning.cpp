#include "pch.h"
#include "skinning.h"
#include "dx_command_list.h"
#include "dx_barrier_batcher.h"

#include "skinning_rs.hlsli"

#define MAX_NUM_SKINNING_MATRICES_PER_FRAME 4096
#define MAX_NUM_SKINNED_VERTICES_PER_FRAME (1024 * 256)

static ref<dx_buffer> skinningMatricesBuffer; // Buffered frames are in a single dx_buffer.

static uint32 currentSkinnedVertexBuffer;
static vertex_buffer_group skinnedVertexBuffer[2]; // We have two of these, so that we can compute screen space velocities.

static dx_pipeline skinningPipeline;


struct skinning_call
{
	vertex_buffer_group vertexBuffer;
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
	skinningMatricesBuffer = createUploadBuffer(sizeof(mat4), MAX_NUM_SKINNING_MATRICES_PER_FRAME * NUM_BUFFERED_FRAMES, 0);

	for (uint32 i = 0; i < 2; ++i)
	{
		skinnedVertexBuffer[i].positions = createVertexBuffer(sizeof(vec3), MAX_NUM_SKINNED_VERTICES_PER_FRAME, 0, true);
		skinnedVertexBuffer[i].others = createVertexBuffer(getVertexSize(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents), MAX_NUM_SKINNED_VERTICES_PER_FRAME, 0, true);
	}

	skinningPipeline = createReloadablePipeline("skinning_cs");

	skinningMatrices.reserve(MAX_NUM_SKINNING_MATRICES_PER_FRAME);
}

std::tuple<vertex_buffer_group, vertex_range, mat4*> skinObject(const vertex_buffer_group& vertexBuffer, vertex_range range, uint32 numJoints)
{
	uint32 offset = (uint32)skinningMatrices.size();

	assert(offset + numJoints <= MAX_NUM_SKINNING_MATRICES_PER_FRAME);

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

	assert(totalNumVertices <= MAX_NUM_SKINNED_VERTICES_PER_FRAME);

	return { skinnedVertexBuffer[currentSkinnedVertexBuffer], resultRange, skinningMatrices.data() + offset };
}

std::tuple<vertex_buffer_group, uint32, mat4*> skinObject(const vertex_buffer_group& vertexBuffer, uint32 numJoints)
{
	auto [vb, range, mats] = skinObject(vertexBuffer, vertex_range{ 0, vertexBuffer.positions->elementCount }, numJoints);

	return { vb, range.firstVertex, mats };
}

std::tuple<vertex_buffer_group, submesh_info, mat4*> skinObject(const vertex_buffer_group& vertexBuffer, submesh_info submesh, uint32 numJoints)
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

		uint32 matrixOffset = dxContext.bufferedFrameID * MAX_NUM_SKINNING_MATRICES_PER_FRAME;

		mat4* mats = (mat4*)mapBuffer(skinningMatricesBuffer, false);
		memcpy(mats + matrixOffset, skinningMatrices.data(), sizeof(mat4) * skinningMatrices.size());
		unmapBuffer(skinningMatricesBuffer, true, map_range{ matrixOffset, (uint32)skinningMatrices.size() });


		cl->setPipelineState(*skinningPipeline.pipeline);
		cl->setComputeRootSignature(*skinningPipeline.rootSignature);

		cl->setRootComputeSRV(SKINNING_RS_MATRICES, skinningMatricesBuffer->gpuVirtualAddress + sizeof(mat4) * matrixOffset);
		cl->setRootComputeUAV(SKINNING_RS_OUTPUT0, skinnedVertexBuffer[currentSkinnedVertexBuffer].positions);
		cl->setRootComputeUAV(SKINNING_RS_OUTPUT1, skinnedVertexBuffer[currentSkinnedVertexBuffer].others);

		for (const auto& c : calls)
		{
			cl->setRootComputeSRV(SKINNING_RS_INPUT_VERTEX_BUFFER0, c.vertexBuffer.positions->gpuVirtualAddress);
			cl->setRootComputeSRV(SKINNING_RS_INPUT_VERTEX_BUFFER1, c.vertexBuffer.others->gpuVirtualAddress);
			cl->setCompute32BitConstants(SKINNING_RS_CB, skinning_cb{ c.jointOffset, c.numJoints, c.range.firstVertex, c.range.numVertices, c.vertexOffset });
			cl->dispatch(bucketize(c.range.numVertices, 512));
		}

		// Not necessary, since the command list ends here.
		barrier_batcher(cl)
			.uav(skinnedVertexBuffer[currentSkinnedVertexBuffer].positions)
			.uav(skinnedVertexBuffer[currentSkinnedVertexBuffer].others);

		dxContext.executeCommandList(cl);

		result = true;
	}

	currentSkinnedVertexBuffer = 1 - currentSkinnedVertexBuffer;
	calls.clear();
	skinningMatrices.clear();
	totalNumVertices = 0;

	return result;
}


