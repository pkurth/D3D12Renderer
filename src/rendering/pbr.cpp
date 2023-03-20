#include "pch.h"
#include "pbr.h"
#include "dx/dx_texture.h"
#include "texture_preprocessing.h"
#include "dx/dx_context.h"
#include "dx/dx_command_list.h"
#include "geometry/mesh_builder.h"
#include "core/color.h"
#include "core/hash.h"
#include "render_resources.h"
#include "render_utils.h"

#include "default_pbr_rs.hlsli"
#include "material.hlsli"
#include "light_source.hlsli"

#include <unordered_map>
#include <memory>

static_assert(sizeof(pbr_material_cb) == 24);

static dx_pipeline opaquePBRPipeline;
static dx_pipeline opaqueDoubleSidedPBRPipeline;
static dx_pipeline transparentPBRPipeline;

void pbr_pipeline::initialize()
{
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position_uv_normal_tangent)
			.renderTargets(opaqueLightPassFormats, OPQAUE_LIGHT_PASS_NO_VELOCITIES_NO_OBJECT_ID, depthStencilFormat)
			.depthSettings(true, false, D3D12_COMPARISON_FUNC_EQUAL);

		opaquePBRPipeline = createReloadablePipeline(desc, { "default_vs", "default_pbr_ps" });

		desc.cullingOff();
		opaqueDoubleSidedPBRPipeline = createReloadablePipeline(desc, { "default_vs", "default_pbr_ps" });
	}

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position_uv_normal_tangent)
			.renderTargets(transparentLightPassFormats, arraysize(transparentLightPassFormats), depthStencilFormat)
			.alphaBlending(0);

		transparentPBRPipeline = createReloadablePipeline(desc, { "default_vs", "default_pbr_transparent_ps" });
	}
}



void pbr_pipeline::setupPBRCommon(dx_command_list* cl, const common_render_data& common)
{
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	dx_cpu_descriptor_handle nullTexture = render_resources::nullTextureSRV;
	dx_cpu_descriptor_handle nullBuffer = render_resources::nullBufferSRV;

	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 0, common.irradiance ? common.irradiance->defaultSRV : nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 1, common.prefilteredRadiance ? common.prefilteredRadiance->defaultSRV : nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 2, render_resources::brdfTex);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 3, common.tiledCullingGrid ? common.tiledCullingGrid->defaultSRV : nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 4, common.tiledObjectsIndexList ? common.tiledObjectsIndexList->defaultSRV : nullBuffer);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 5, common.pointLightBuffer ? common.pointLightBuffer->defaultSRV : nullBuffer);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 6, common.spotLightBuffer ? common.spotLightBuffer->defaultSRV : nullBuffer);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 7, common.decalBuffer ? common.decalBuffer->defaultSRV : nullBuffer);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 8, common.shadowMap ? common.shadowMap->defaultSRV : nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 9, common.pointLightShadowInfoBuffer ? common.pointLightShadowInfoBuffer->defaultSRV : nullBuffer);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 10, common.spotLightShadowInfoBuffer ? common.spotLightShadowInfoBuffer->defaultSRV : nullBuffer);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 11, common.decalTextureAtlas ? common.decalTextureAtlas->defaultSRV : nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 12, common.aoTexture ? common.aoTexture : render_resources::whiteTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 13, common.sssTexture ? common.sssTexture : render_resources::whiteTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 14, common.ssrTexture ? common.ssrTexture->defaultSRV : nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 15, common.lightProbeIrradiance ? common.lightProbeIrradiance->defaultSRV : nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 16, common.lightProbeDepth ? common.lightProbeDepth->defaultSRV : nullTexture);

	cl->setGraphicsDynamicConstantBuffer(DEFAULT_PBR_RS_CAMERA, common.cameraCBV);
	cl->setGraphicsDynamicConstantBuffer(DEFAULT_PBR_RS_LIGHTING, common.lightingCBV);


	// Default material properties. This is JUST to make the dynamic descriptor heap happy.
	// These textures will NEVER be read.

	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 0, nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 1, nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 2, nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 3, nullTexture);
}


PIPELINE_SETUP_IMPL(pbr_pipeline::opaque)
{
	cl->setPipelineState(*opaquePBRPipeline.pipeline);
	cl->setGraphicsRootSignature(*opaquePBRPipeline.rootSignature);

	setupPBRCommon(cl, common);
}

PIPELINE_SETUP_IMPL(pbr_pipeline::opaque_double_sided)
{
	cl->setPipelineState(*opaqueDoubleSidedPBRPipeline.pipeline);
	cl->setGraphicsRootSignature(*opaqueDoubleSidedPBRPipeline.rootSignature);

	setupPBRCommon(cl, common);
}

PIPELINE_SETUP_IMPL(pbr_pipeline::transparent)
{
	cl->setPipelineState(*transparentPBRPipeline.pipeline);
	cl->setGraphicsRootSignature(*transparentPBRPipeline.rootSignature);

	setupPBRCommon(cl, common);
}

PIPELINE_RENDER_IMPL(pbr_pipeline, pbr_render_data)
{
	const auto& mat = data.material;

	uint32 flags = 0;

	if (mat->albedo)
	{
		cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 0, mat->albedo);
		flags |= MATERIAL_USE_ALBEDO_TEXTURE;
	}
	if (mat->normal)
	{
		cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 1, mat->normal);
		flags |= MATERIAL_USE_NORMAL_TEXTURE;
	}
	if (mat->roughness)
	{
		cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 2, mat->roughness);
		flags |= MATERIAL_USE_ROUGHNESS_TEXTURE;
	}
	if (mat->metallic)
	{
		cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 3, mat->metallic);
		flags |= MATERIAL_USE_METALLIC_TEXTURE;
	}
	if (mat->shader != pbr_material_shader_default)
	{
		flags |= MATERIAL_DOUBLE_SIDED;
	}

	cl->setGraphics32BitConstants(DEFAULT_PBR_RS_MATERIAL,
		pbr_material_cb(mat->albedoTint, mat->emission.xyz, mat->roughnessOverride, mat->metallicOverride, flags, 1.f, mat->translucency, mat->uvScale)
	);


	const submesh_info& submesh = data.submesh;

	cl->setRootGraphicsSRV(DEFAULT_PBR_RS_TRANSFORM, data.transformPtr);

	cl->setVertexBuffer(0, data.vertexBuffer.positions);
	cl->setVertexBuffer(1, data.vertexBuffer.others);
	cl->setIndexBuffer(data.indexBuffer);
	cl->drawIndexed(submesh.numIndices, data.numInstances, submesh.firstIndex, submesh.baseVertex, 0);
}

