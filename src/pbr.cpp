#include "pch.h"
#include "pbr.h"
#include "dx_texture.h"
#include "texture_preprocessing.h"
#include "dx_context.h"
#include "dx_command_list.h"
#include "dx_renderer.h"
#include "geometry.h"
#include "color.h"
#include "hash.h"

#include "default_pbr_rs.hlsli"
#include "material.hlsli"

#include <unordered_map>

static dx_pipeline defaultOpaquePBRPipeline;
static dx_pipeline defaultTransparentPBRPipeline;

struct material_key
{
	std::string albedoTex, normalTex, roughTex, metallicTex;
	vec4 emission;
	vec4 albedoTint;
	float roughnessOverride, metallicOverride;
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
		&& a.metallicOverride == b.metallicOverride;
}

ref<pbr_material> createPBRMaterial(const std::string& albedoTex, const std::string& normalTex, const std::string& roughTex, const std::string& metallicTex, const vec4& emission, const vec4& albedoTint, float roughOverride, float metallicOverride)
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
		!metallicTex.empty() ? 0.f : metallicOverride,	// If texture is set, override does not matter, so set it to consistent value.
	};


	static std::unordered_map<material_key, weakref<pbr_material>> cache;
	static std::mutex mutex;

	mutex.lock();

	auto sp = cache[s].lock();
	if (!sp)
	{
		ref<pbr_material> material = make_ref<pbr_material>();

		if (!albedoTex.empty()) material->albedo = loadTextureFromFile(albedoTex);
		if (!normalTex.empty()) material->normal = loadTextureFromFile(normalTex, texture_load_flags_default | texture_load_flags_noncolor);
		if (!roughTex.empty()) material->roughness = loadTextureFromFile(roughTex, texture_load_flags_default | texture_load_flags_noncolor);
		if (!metallicTex.empty()) material->metallic = loadTextureFromFile(metallicTex, texture_load_flags_default | texture_load_flags_noncolor);
		material->emission = emission;
		material->albedoTint = albedoTint;
		material->roughnessOverride = roughOverride;
		material->metallicOverride = metallicOverride;

		cache[s] = sp = material;
	}

	mutex.unlock();
	return sp;
}

ref<pbr_material> getDefaultPBRMaterial()
{
	static ref<pbr_material> material = make_ref<pbr_material>(nullptr, nullptr, nullptr, nullptr, vec4(0.f), vec4(1.f, 0.f, 1.f, 1.f), 1.f, 0.f);
	return material;
}

ref<pbr_environment> createEnvironment(const std::string& filename, uint32 skyResolution, uint32 environmentResolution, uint32 irradianceResolution, bool asyncCompute)
{
	static std::unordered_map<std::string, weakref<pbr_environment>> cache;
	static std::mutex mutex;

	mutex.lock();

	auto sp = cache[filename].lock();
	if (!sp)
	{
		ref<dx_texture> equiSky = loadTextureFromFile(filename,
			texture_load_flags_noncolor | texture_load_flags_cache_to_dds | texture_load_flags_gen_mips_on_cpu);

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

void pbr_material::prepareForRendering(dx_command_list* cl)
{
	uint32 flags = 0;

	if (albedo)
	{
		cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 0, albedo);
		flags |= USE_ALBEDO_TEXTURE;
	}
	if (normal)
	{
		cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 1, normal);
		flags |= USE_NORMAL_TEXTURE;
	}
	if (roughness)
	{
		cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 2, roughness);
		flags |= USE_ROUGHNESS_TEXTURE;
	}
	if (metallic)
	{
		cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 3, metallic);
		flags |= USE_METALLIC_TEXTURE;
	}

	cl->setGraphics32BitConstants(DEFAULT_PBR_RS_MATERIAL,
		pbr_material_cb(albedoTint, emission.xyz, roughnessOverride, metallicOverride, flags)
	);
}

void pbr_material::setupOpaquePipeline(dx_command_list* cl, const common_material_info& info)
{
	cl->setPipelineState(*defaultOpaquePBRPipeline.pipeline);
	cl->setGraphicsRootSignature(*defaultOpaquePBRPipeline.rootSignature);

	setupCommon(cl, info);
}

void pbr_material::setupTransparentPipeline(dx_command_list* cl, const common_material_info& info)
{
	cl->setPipelineState(*defaultTransparentPBRPipeline.pipeline);
	cl->setGraphicsRootSignature(*defaultTransparentPBRPipeline.rootSignature);

	setupCommon(cl, info);
}

void pbr_material::setupCommon(dx_command_list* cl, const common_material_info& info)
{
	dx_cpu_descriptor_handle nullTexture = dx_renderer::nullTextureSRV;
	dx_cpu_descriptor_handle nullBuffer = dx_renderer::nullBufferSRV;

	cl->setGraphics32BitConstants(DEFAULT_PBR_RS_LIGHTING, lighting_cb{ vec2(1.f / info.shadowMap->width, 1.f / info.shadowMap->height), info.environmentIntensity });

	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 0, info.irradiance);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 1, info.environment);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 2, info.brdf);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 3, info.tiledCullingGrid);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 4, info.tiledObjectsIndexList);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 5, info.pointLightBuffer ? info.pointLightBuffer->defaultSRV : nullBuffer);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 6, info.spotLightBuffer ? info.spotLightBuffer->defaultSRV : nullBuffer);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 7, info.decalBuffer ? info.decalBuffer->defaultSRV : nullBuffer);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 8, info.shadowMap ? info.shadowMap->defaultSRV : nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 9, info.pointLightShadowInfoBuffer ? info.pointLightShadowInfoBuffer->defaultSRV : nullBuffer);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 10, info.spotLightShadowInfoBuffer ? info.spotLightShadowInfoBuffer->defaultSRV : nullBuffer);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_FRAME_CONSTANTS, 11, info.decalTextureAtlas ? info.decalTextureAtlas->defaultSRV : nullTexture);

	cl->setGraphicsDynamicConstantBuffer(DEFAULT_PBR_RS_SUN, info.sunCBV);

	cl->setGraphicsDynamicConstantBuffer(DEFAULT_PBR_RS_CAMERA, info.cameraCBV);


	// Default material properties. This is JUST to make the dynamic descriptor heap happy.
	// These textures will NEVER be read.

	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 0, nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 1, nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 2, nullTexture);
	cl->setDescriptorHeapSRV(DEFAULT_PBR_RS_PBR_TEXTURES, 3, nullTexture);
}

void pbr_material::initializePipeline()
{
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position_uv_normal_tangent)
			.renderTargets(dx_renderer::opaqueLightPassFormats, arraysize(dx_renderer::opaqueLightPassFormats), dx_renderer::hdrDepthStencilFormat)
			.depthSettings(true, false, D3D12_COMPARISON_FUNC_EQUAL);

		defaultOpaquePBRPipeline = createReloadablePipeline(desc, { "default_vs", "default_pbr_ps" });
	}

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position_uv_normal_tangent)
			.renderTargets(dx_renderer::transparentLightPassFormats, arraysize(dx_renderer::transparentLightPassFormats), dx_renderer::hdrDepthStencilFormat)
			.alphaBlending(0);

		defaultTransparentPBRPipeline = createReloadablePipeline(desc, { "default_vs", "default_pbr_transparent_ps" });
	}
}
