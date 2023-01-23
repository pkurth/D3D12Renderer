#include "pch.h"
#include "depth_prepass.h"
#include "render_utils.h"

#include "dx/dx_command_list.h"

#include "depth_only_rs.hlsli"


static dx_pipeline defaultPipeline;
static dx_pipeline doubleSidedDefaultPipeline;

static dx_pipeline animatedPipeline;
static dx_pipeline doubleSidedAnimatedPipeline;


void initializeDepthPrepassPipelines()
{
	DXGI_FORMAT depthOnlyFormat[] = { screenVelocitiesFormat, objectIDsFormat };

	auto desc = CREATE_GRAPHICS_PIPELINE
		.renderTargets(depthOnlyFormat, arraysize(depthOnlyFormat), depthStencilFormat)
		.inputLayout(inputLayout_position);

	defaultPipeline = createReloadablePipeline(desc, { "depth_only_vs", "depth_only_ps" }, rs_in_vertex_shader);
	animatedPipeline = createReloadablePipeline(desc, { "depth_only_animated_vs", "depth_only_ps" }, rs_in_vertex_shader);

	desc.cullingOff();
	doubleSidedDefaultPipeline = createReloadablePipeline(desc, { "depth_only_vs", "depth_only_ps" }, rs_in_vertex_shader);
	doubleSidedAnimatedPipeline = createReloadablePipeline(desc, { "depth_only_animated_vs", "depth_only_ps" }, rs_in_vertex_shader);
}

static void setupDepthPrepassCommon(dx_command_list* cl, const dx_pipeline& pipeline, vec2 jitter, vec2 prevFrameJitter)
{
	cl->setPipelineState(*pipeline.pipeline);
	cl->setGraphicsRootSignature(*pipeline.rootSignature);

	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	depth_only_camera_jitter_cb jitterCB = { jitter, prevFrameJitter };

	cl->setGraphics32BitConstants(DEPTH_ONLY_RS_CAMERA_JITTER, jitterCB);
}


PIPELINE_SETUP_IMPL(static_depth_prepass_pipeline::single_sided)
{
	setupDepthPrepassCommon(cl, defaultPipeline, common.cameraJitter, common.prevFrameCameraJitter);
}

PIPELINE_SETUP_IMPL(static_depth_prepass_pipeline::double_sided)
{
	setupDepthPrepassCommon(cl, doubleSidedDefaultPipeline, common.cameraJitter, common.prevFrameCameraJitter);
}

PIPELINE_SETUP_IMPL(dynamic_depth_prepass_pipeline::single_sided)
{
	setupDepthPrepassCommon(cl, defaultPipeline, common.cameraJitter, common.prevFrameCameraJitter);
}

PIPELINE_SETUP_IMPL(dynamic_depth_prepass_pipeline::double_sided)
{
	setupDepthPrepassCommon(cl, doubleSidedDefaultPipeline, common.cameraJitter, common.prevFrameCameraJitter);
}

PIPELINE_SETUP_IMPL(animated_depth_prepass_pipeline::single_sided)
{
	setupDepthPrepassCommon(cl, animatedPipeline, common.cameraJitter, common.prevFrameCameraJitter);
}

PIPELINE_SETUP_IMPL(animated_depth_prepass_pipeline::double_sided)
{
	setupDepthPrepassCommon(cl, doubleSidedAnimatedPipeline, common.cameraJitter, common.prevFrameCameraJitter);
}




DEPTH_ONLY_RENDER_IMPL(static_depth_prepass_pipeline)
{
	cl->setGraphics32BitConstants(DEPTH_ONLY_RS_OBJECT_ID, rc.objectID);
	cl->setGraphics32BitConstants(DEPTH_ONLY_RS_MVP, depth_only_transform_cb{ viewProj * rc.data.transform, prevFrameViewProj * rc.data.transform });

	cl->setVertexBuffer(0, rc.data.vertexBuffer);
	cl->setIndexBuffer(rc.data.indexBuffer);
	cl->drawIndexed(rc.data.submesh.numIndices, 1, rc.data.submesh.firstIndex, rc.data.submesh.baseVertex, 0);
}

DEPTH_ONLY_RENDER_IMPL(dynamic_depth_prepass_pipeline)
{
	cl->setGraphics32BitConstants(DEPTH_ONLY_RS_OBJECT_ID, rc.objectID);
	cl->setGraphics32BitConstants(DEPTH_ONLY_RS_MVP, depth_only_transform_cb{ viewProj * rc.data.transform, prevFrameViewProj * rc.data.prevFrameTransform });

	cl->setVertexBuffer(0, rc.data.vertexBuffer);
	cl->setIndexBuffer(rc.data.indexBuffer);
	cl->drawIndexed(rc.data.submesh.numIndices, 1, rc.data.submesh.firstIndex, rc.data.submesh.baseVertex, 0);
}

DEPTH_ONLY_RENDER_IMPL(animated_depth_prepass_pipeline)
{
	cl->setGraphics32BitConstants(DEPTH_ONLY_RS_OBJECT_ID, rc.objectID);
	cl->setGraphics32BitConstants(DEPTH_ONLY_RS_MVP, depth_only_transform_cb{ viewProj * rc.data.transform, prevFrameViewProj * rc.data.prevFrameTransform });
	cl->setRootGraphicsSRV(DEPTH_ONLY_RS_PREV_FRAME_POSITIONS, rc.data.prevFrameVertexBufferAddress + rc.data.submesh.baseVertex * rc.data.vertexBuffer.view.StrideInBytes);

	cl->setVertexBuffer(0, rc.data.vertexBuffer);
	cl->setIndexBuffer(rc.data.indexBuffer);
	cl->drawIndexed(rc.data.submesh.numIndices, 1, rc.data.submesh.firstIndex, rc.data.submesh.baseVertex, 0);
}















