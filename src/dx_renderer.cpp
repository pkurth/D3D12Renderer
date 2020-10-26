#include "pch.h"
#include "dx_renderer.h"
#include "dx_context.h"
#include "dx_command_list.h"
#include "dx_pipeline.h"
#include "camera.h"
#include "geometry.h"
#include "imgui.h"

#include "model_rs.hlsl"
#include "present_rs.hlsl"

#include <iostream>

dx_cbv_srv_uav_descriptor_heap dx_renderer::globalDescriptorHeap;

dx_descriptor_handle dx_renderer::frameResultSRV;
dx_texture dx_renderer::frameResult;
dx_texture dx_renderer::depthBuffer;

dx_render_target dx_renderer::renderTarget;

uint32 dx_renderer::renderWidth;
uint32 dx_renderer::renderHeight;



static render_camera camera;
static dx_mesh mesh;
static submesh_info submesh;

static tonemap_cb tonemap = defaultTonemapParameters();


static dx_pipeline presentPipeline;
static dx_pipeline modelPipeline;


void dx_renderer::initialize(uint32 width, uint32 height)
{
	renderWidth = width;
	renderHeight = height;

	globalDescriptorHeap = createDescriptorHeap(&dxContext, 2048);
	
	frameResult = createTexture(&dxContext, 0, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, true);
	SET_NAME(frameResult.resource, "Frame result");

	depthBuffer = createDepthTexture(&dxContext, width, height, DXGI_FORMAT_D32_FLOAT);
	SET_NAME(depthBuffer.resource, "Frame depth buffer");

	frameResultSRV = globalDescriptorHeap.push2DTextureSRV(frameResult);

	renderTarget.pushColorAttachment(frameResult);
	renderTarget.pushDepthStencilAttachment(depthBuffer);


	camera.position = vec3(0.f, 0.f, 4.f);
	camera.rotation = quat::identity;
	camera.verticalFOV = deg2rad(70.f);
	camera.nearPlane = 0.1f;

	cpu_mesh cpuMesh(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals);
	submesh = cpuMesh.pushSphere(15, 15, 1.f);
	mesh = cpuMesh.createDXMesh(&dxContext);


	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(renderTarget.renderTargetFormat)
			.depthSettings(false, false)
			.cullingOff();

		presentPipeline = createReloadablePipeline(desc, { "fullscreen_triangle_vs", "present_ps" }, "present_ps");
	}

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position_uv_normal)
			.rasterizeCounterClockwise()
			.renderTargets(renderTarget.renderTargetFormat, renderTarget.depthStencilFormat);

		modelPipeline = createReloadablePipeline(desc, { "model_vs", "model_ps" }, "model_vs");
	}

	createAllReloadablePipelines();
}

void dx_renderer::beginFrame(uint32 width, uint32 height)
{
	if (renderWidth != width || renderHeight != height)
	{
		renderWidth = width;
		renderHeight = height;

		resizeTexture(&dxContext, frameResult, width, height);
		resizeTexture(&dxContext, depthBuffer, width, height);

		globalDescriptorHeap.create2DTextureSRV(frameResult, frameResultSRV);

		renderTarget.notifyOnTextureResize(width, height);
	}

	checkForChangedPipelines();

	camera.recalculateMatrices(width, height);
}

void dx_renderer::dummyRender(float dt)
{
	dx_command_list* cl = dxContext.getFreeRenderCommandList();

	CD3DX12_RECT scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);


	cl->setScissor(scissorRect);
	cl->setViewport(renderTarget.viewport);

	barrier_batcher(cl)
		.transition(frameResult.resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);

	ImGui::Begin("Settings");
	ImGui::Text("%f ms, %u FPS", dt, (uint32)(1.f / dt));
	ImGui::SliderFloat("Exposure", &tonemap.exposure, -1.f, 2.f);
	ImGui::End();


	cl->clearDepth(renderTarget.dsvHandle);
	cl->setRenderTarget(renderTarget);


	cl->setPipelineState(*presentPipeline.pipeline);
	cl->setGraphicsRootSignature(*presentPipeline.rootSignature);
	cl->setGraphics32BitConstants(0, tonemap);
	cl->drawFullscreenTriangle();


	cl->setPipelineState(*modelPipeline.pipeline);
	cl->setGraphicsRootSignature(*modelPipeline.rootSignature);
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cl->setVertexBuffer(0, mesh.vertexBuffer);
	cl->setIndexBuffer(mesh.indexBuffer);
	cl->setGraphics32BitConstants(0, transform_cb{ camera.viewProj, mat4::identity });
	cl->drawIndexed(submesh.numTriangles * 3, 1, submesh.firstTriangle * 3, submesh.baseVertex, 0);


	barrier_batcher(cl)
		.transition(frameResult.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);

	dxContext.executeCommandList(cl);
}
