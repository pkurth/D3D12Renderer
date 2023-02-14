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
#include "lighting.hlsli"

#include <unordered_map>
#include <memory>

static_assert(sizeof(pbr_material_cb) == 24);

static dx_pipeline opaquePBRPipeline;
static dx_pipeline opaqueDoubleSidedPBRPipeline;
static dx_pipeline transparentPBRPipeline;

struct material_key
{
	fs::path albedoTex, normalTex, roughTex, metallicTex;
	vec4 emission;
	vec4 albedoTint;
	float roughnessOverride, metallicOverride;
	bool doubleSided;
	float uvScale;
};

namespace std
{
	template<>
	struct hash<material_key>
	{
		size_t operator()(const material_key& x) const
		{
			size_t seed = 0;

			hash_combine(seed, x.albedoTex);
			hash_combine(seed, x.normalTex);
			hash_combine(seed, x.roughTex);
			hash_combine(seed, x.metallicTex);
			hash_combine(seed, x.emission);
			hash_combine(seed, x.albedoTint);
			hash_combine(seed, x.roughnessOverride);
			hash_combine(seed, x.metallicOverride);
			hash_combine(seed, x.doubleSided);
			hash_combine(seed, x.uvScale);

			return seed;
		}
	};
}

static bool operator==(const material_key& a, const material_key& b)
{
	return a.albedoTex == b.albedoTex
		&& a.normalTex == b.normalTex
		&& a.roughTex == b.roughTex
		&& a.metallicTex == b.metallicTex
		&& a.emission == b.emission
		&& a.albedoTint == b.albedoTint
		&& a.roughnessOverride == b.roughnessOverride
		&& a.metallicOverride == b.metallicOverride
		&& a.doubleSided == b.doubleSided
		&& a.uvScale == b.uvScale;
}

ref<pbr_material> createPBRMaterial(
	const fs::path& albedoTex, 
	const fs::path& normalTex, 
	const fs::path& roughTex, 
	const fs::path& metallicTex, 
	const vec4& emission, 
	const vec4& albedoTint, 
	float roughOverride, 
	float metallicOverride, 
	bool doubleSided,
	float uvScale, 
	bool disableTextureCompression)
{
	material_key s =
	{
		!albedoTex.empty()	? albedoTex		: "",
		!normalTex.empty() ? normalTex		: "",
		!roughTex.empty() ? roughTex		: "",
		!metallicTex.empty() ? metallicTex	: "",
		emission,
		albedoTint,
		!roughTex.empty() ? 1.f : roughOverride,			// If texture is set, override does not matter, so set it to consistent value.
		!metallicTex.empty() ? 0.f : metallicOverride,		// If texture is set, override does not matter, so set it to consistent value.
		doubleSided,
		uvScale,
	};


	static std::unordered_map<material_key, weakref<pbr_material>> cache;
	static std::mutex mutex;

	mutex.lock();

	auto sp = cache[s].lock();
	if (!sp)
	{
		ref<pbr_material> material = make_ref<pbr_material>();

		uint32 removeFlags = 0xFFFFFFFF;
		if (disableTextureCompression)
		{
			removeFlags = ~image_load_flags_compress;
		}

		if (!albedoTex.empty()) material->albedo = loadTextureFromFile(albedoTex, image_load_flags_default & removeFlags);
		if (!normalTex.empty()) material->normal = loadTextureFromFile(normalTex, image_load_flags_default_noncolor & removeFlags);
		if (!roughTex.empty()) material->roughness = loadTextureFromFile(roughTex, image_load_flags_default_noncolor & removeFlags);
		if (!metallicTex.empty()) material->metallic = loadTextureFromFile(metallicTex, image_load_flags_default_noncolor & removeFlags);
		material->emission = emission;
		material->albedoTint = albedoTint;
		material->roughnessOverride = roughOverride;
		material->metallicOverride = metallicOverride;
		material->doubleSided = doubleSided;
		material->uvScale = uvScale;

		cache[s] = sp = material;
	}

	mutex.unlock();
	return sp;
}

ref<pbr_material> getDefaultPBRMaterial()
{
	static ref<pbr_material> material = make_ref<pbr_material>(nullptr, nullptr, nullptr, nullptr, vec4(0.f), vec4(1.f, 0.f, 1.f, 1.f), 1.f, 0.f, false, 1.f);
	return material;
}

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

PIPELINE_RENDER_IMPL(pbr_pipeline)
{
	const auto& mat = rc.data.material;

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
	if (mat->doubleSided)
	{
		flags |= MATERIAL_DOUBLE_SIDED;
	}

	cl->setGraphics32BitConstants(DEFAULT_PBR_RS_MATERIAL,
		pbr_material_cb(mat->albedoTint, mat->emission.xyz, mat->roughnessOverride, mat->metallicOverride, flags, 1.f, 0.f, mat->uvScale)
	);


	const mat4& m = rc.data.transform;
	const submesh_info& submesh = rc.data.submesh;

	cl->setGraphics32BitConstants(DEFAULT_PBR_RS_MVP, transform_cb{ viewProj * m, m });

	cl->setVertexBuffer(0, rc.data.vertexBuffer.positions);
	cl->setVertexBuffer(1, rc.data.vertexBuffer.others);
	cl->setIndexBuffer(rc.data.indexBuffer);
	cl->drawIndexed(submesh.numIndices, 1, submesh.firstIndex, submesh.baseVertex, 0);
}
