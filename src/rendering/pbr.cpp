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

#include <unordered_map>
#include <memory>

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
	float uvScale)
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

		if (!albedoTex.empty()) material->albedo = loadTextureFromFile(albedoTex);
		if (!normalTex.empty()) material->normal = loadTextureFromFile(normalTex, image_load_flags_default | image_load_flags_noncolor);
		if (!roughTex.empty()) material->roughness = loadTextureFromFile(roughTex, image_load_flags_default | image_load_flags_noncolor);
		if (!metallicTex.empty()) material->metallic = loadTextureFromFile(metallicTex, image_load_flags_default | image_load_flags_noncolor);
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

ref<pbr_environment> createEnvironment(const fs::path& filename, uint32 skyResolution, uint32 environmentResolution, uint32 irradianceResolution, bool asyncCompute)
{
	static std::unordered_map<fs::path, weakref<pbr_environment>> cache;
	static std::mutex mutex;

	mutex.lock();

	auto sp = cache[filename].lock();
	if (!sp)
	{
		ref<dx_texture> equiSky = loadTextureFromFile(filename,
			image_load_flags_noncolor | image_load_flags_cache_to_dds | image_load_flags_gen_mips_on_cpu);

		if (equiSky)
		{
			ref<pbr_environment> environment = make_ref<pbr_environment>();

			dx_command_list* cl;
			if (asyncCompute)
			{
				dxContext.computeQueue.waitForOtherQueue(dxContext.copyQueue);
				cl = dxContext.getFreeComputeCommandList(true);
			}
			else
			{
				dxContext.renderQueue.waitForOtherQueue(dxContext.copyQueue);
				cl = dxContext.getFreeRenderCommandList();
			}
			//generateMipMapsOnGPU(cl, equiSky);
			environment->sky = equirectangularToCubemap(cl, equiSky, skyResolution, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
			environment->environment = prefilterEnvironment(cl, environment->sky, environmentResolution);
			environment->irradiance = cubemapToIrradiance(cl, environment->sky, irradianceResolution);

			SET_NAME(environment->sky->resource, "Sky");
			SET_NAME(environment->environment->resource, "Environment");
			SET_NAME(environment->irradiance->resource, "Irradiance");

			environment->name = filename;

			dxContext.executeCommandList(cl);

			cache[filename] = sp = environment;
		}
	}

	mutex.unlock();
	return sp;
}

static void setupPBRCommon(dx_command_list* cl, const common_material_info& info)
{
	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	dx_cpu_descriptor_handle nullTexture = render_resources::nullTextureSRV;
	dx_cpu_descriptor_handle nullBuffer = render_resources::nullBufferSRV;

	cl->setGraphics32BitConstants(DEFAULT_PBR_RS_LIGHTING, lighting_cb{ vec2(1.f / info.shadowMap->width, 1.f / info.shadowMap->height), info.environmentIntensity });

	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 0, info.irradiance ? info.irradiance->defaultSRV : nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 1, info.environment ? info.environment->defaultSRV : nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 2, render_resources::brdfTex);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 3, info.tiledCullingGrid ? info.tiledCullingGrid->defaultSRV : nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 4, info.tiledObjectsIndexList ? info.tiledObjectsIndexList->defaultSRV : nullBuffer);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 5, info.pointLightBuffer ? info.pointLightBuffer->defaultSRV : nullBuffer);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 6, info.spotLightBuffer ? info.spotLightBuffer->defaultSRV : nullBuffer);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 7, info.decalBuffer ? info.decalBuffer->defaultSRV : nullBuffer);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 8, info.shadowMap ? info.shadowMap->defaultSRV : nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 9, info.pointLightShadowInfoBuffer ? info.pointLightShadowInfoBuffer->defaultSRV : nullBuffer);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 10, info.spotLightShadowInfoBuffer ? info.spotLightShadowInfoBuffer->defaultSRV : nullBuffer);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 11, info.decalTextureAtlas ? info.decalTextureAtlas->defaultSRV : nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 12, info.aoTexture ? info.aoTexture : render_resources::whiteTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 13, info.sssTexture ? info.sssTexture : render_resources::whiteTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 14, info.ssrTexture ? info.ssrTexture->defaultSRV : nullTexture);

	cl->setGraphicsDynamicConstantBuffer(DEFAULT_PBR_RS_SUN, info.sunCBV);

	cl->setGraphicsDynamicConstantBuffer(DEFAULT_PBR_RS_CAMERA, info.cameraCBV);


	// Default material properties. This is JUST to make the dynamic descriptor heap happy.
	// These textures will NEVER be read.

	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 0, nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 1, nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 2, nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 3, nullTexture);
}

template <typename pipeline_t>
static void renderPBRCommon(dx_command_list* cl, const mat4& viewProj, const default_render_command<pipeline_t>& rc)
{
	const auto& mat = rc.material;

	uint32 flags = 0;

	if (mat->albedo)
	{
		cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 0, mat->albedo);
		flags |= USE_ALBEDO_TEXTURE;
	}
	if (mat->normal)
	{
		cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 1, mat->normal);
		flags |= USE_NORMAL_TEXTURE;
	}
	if (mat->roughness)
	{
		cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 2, mat->roughness);
		flags |= USE_ROUGHNESS_TEXTURE;
	}
	if (mat->metallic)
	{
		cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 3, mat->metallic);
		flags |= USE_METALLIC_TEXTURE;
	}

	cl->setGraphics32BitConstants(DEFAULT_PBR_RS_MATERIAL,
		pbr_material_cb(mat->albedoTint, mat->emission.xyz, mat->roughnessOverride, mat->metallicOverride, flags, 1.f, 0.f, mat->doubleSided, mat->uvScale)
	);


	const mat4& m = rc.transform;
	const submesh_info& submesh = rc.submesh;

	cl->setGraphics32BitConstants(DEFAULT_PBR_RS_MVP, transform_cb{ viewProj * m, m });

	cl->setVertexBuffer(0, rc.vertexBuffer.positions);
	cl->setVertexBuffer(1, rc.vertexBuffer.others);
	cl->setIndexBuffer(rc.indexBuffer);
	cl->drawIndexed(submesh.numIndices, 1, submesh.firstIndex, submesh.baseVertex, 0);
}

void opaque_pbr_pipeline::initialize()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout_position_uv_normal_tangent)
		.renderTargets(opaqueLightPassFormats, arraysize(opaqueLightPassFormats), depthStencilFormat)
		.depthSettings(true, false, D3D12_COMPARISON_FUNC_EQUAL);

	opaquePBRPipeline = createReloadablePipeline(desc, { "default_vs", "default_pbr_ps" });

	desc.cullingOff();
	opaqueDoubleSidedPBRPipeline = createReloadablePipeline(desc, { "default_vs", "default_pbr_ps" });
}

void transparent_pbr_pipeline::initialize()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout_position_uv_normal_tangent)
		.renderTargets(transparentLightPassFormats, arraysize(transparentLightPassFormats), depthStencilFormat)
		.alphaBlending(0);

	transparentPBRPipeline = createReloadablePipeline(desc, { "default_vs", "default_pbr_transparent_ps" });
}




PIPELINE_SETUP_IMPL(opaque_pbr_pipeline::standard)
{
	cl->setPipelineState(*opaquePBRPipeline.pipeline);
	cl->setGraphicsRootSignature(*opaquePBRPipeline.rootSignature);

	setupPBRCommon(cl, materialInfo);
}

PIPELINE_SETUP_IMPL(opaque_pbr_pipeline::double_sided)
{
	cl->setPipelineState(*opaqueDoubleSidedPBRPipeline.pipeline);
	cl->setGraphicsRootSignature(*opaqueDoubleSidedPBRPipeline.rootSignature);

	setupPBRCommon(cl, materialInfo);
}

PIPELINE_RENDER_IMPL(opaque_pbr_pipeline)
{
	renderPBRCommon(cl, viewProj, rc);
}

PIPELINE_SETUP_IMPL(transparent_pbr_pipeline)
{
	cl->setPipelineState(*transparentPBRPipeline.pipeline);
	cl->setGraphicsRootSignature(*transparentPBRPipeline.rootSignature);

	setupPBRCommon(cl, materialInfo);
}

PIPELINE_RENDER_IMPL(transparent_pbr_pipeline)
{
	renderPBRCommon(cl, viewProj, rc);
}
