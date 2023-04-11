#include "pch.h"
#include "water.h"

#include "dx/dx_buffer.h"
#include "dx/dx_pipeline.h"
#include "dx/dx_profiling.h"

#include "rendering/render_utils.h"
#include "rendering/render_pass.h"
#include "rendering/render_resources.h"

#include "geometry/mesh_builder.h"

#include "water_rs.hlsli"
#include "transform.hlsli"


static dx_pipeline waterPipeline;

static ref<dx_texture> normalmap1;
static ref<dx_texture> normalmap2;
static ref<dx_texture> foamTexture;
static ref<dx_texture> noiseTexture;

void initializeWaterPipelines()
{
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.cullingOff()
			.renderTargets(transparentLightPassFormats, arraysize(transparentLightPassFormats), depthStencilFormat);

		waterPipeline = createReloadablePipeline(desc, { "water_vs", "water_ps" });
	}

	normalmap1 = loadTextureFromFileAsync("assets/water/waterNM1.png", image_load_flags_noncolor | image_load_flags_gen_mips_on_cpu | image_load_flags_cache_to_dds);
	normalmap2 = loadTextureFromFileAsync("assets/water/waterNM2.png", image_load_flags_noncolor | image_load_flags_gen_mips_on_cpu | image_load_flags_cache_to_dds);
	foamTexture = loadTextureFromFileAsync("assets/water/waterFoam.dds", image_load_flags_noncolor | image_load_flags_gen_mips_on_cpu | image_load_flags_cache_to_dds);
	noiseTexture = loadTextureFromFileAsync("assets/water/waterNoise.dds", image_load_flags_noncolor | image_load_flags_gen_mips_on_cpu | image_load_flags_cache_to_dds);
}


struct water_render_data
{
	mat4 m;

	water_settings settings;
	float time;
};

struct water_pipeline
{
	PIPELINE_SETUP_DECL;
	PIPELINE_RENDER_DECL(water_render_data);
};

PIPELINE_SETUP_IMPL(water_pipeline)
{
	cl->setPipelineState(*waterPipeline.pipeline);
	cl->setGraphicsRootSignature(*waterPipeline.rootSignature);

	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	
	cl->setGraphicsDynamicConstantBuffer(WATER_RS_CAMERA, common.cameraCBV);
	cl->setGraphicsDynamicConstantBuffer(WATER_RS_LIGHTING, common.lightingCBV);
	cl->setDescriptorHeapSRV(WATER_RS_TEXTURES, 0, common.opaqueColor);
	cl->setDescriptorHeapSRV(WATER_RS_TEXTURES, 1, common.opaqueDepth);
	cl->setDescriptorHeapSRV(WATER_RS_TEXTURES, 2, normalmap1 ? normalmap1 : render_resources::defaultNormalMap);
	cl->setDescriptorHeapSRV(WATER_RS_TEXTURES, 3, normalmap2 ? normalmap2 : render_resources::defaultNormalMap);
	cl->setDescriptorHeapSRV(WATER_RS_TEXTURES, 4, foamTexture ? foamTexture : render_resources::blackTexture);
	cl->setDescriptorHeapSRV(WATER_RS_TEXTURES, 5, noiseTexture ? noiseTexture : render_resources::blackTexture);

	dx_cpu_descriptor_handle nullTexture = render_resources::nullTextureSRV;

	cl->setDescriptorHeapSRV(WATER_RS_FRAME_CONSTANTS, 0, common.irradiance);
	cl->setDescriptorHeapSRV(WATER_RS_FRAME_CONSTANTS, 1, common.prefilteredRadiance);
	cl->setDescriptorHeapSRV(WATER_RS_FRAME_CONSTANTS, 2, render_resources::brdfTex);
	cl->setDescriptorHeapSRV(WATER_RS_FRAME_CONSTANTS, 3, common.shadowMap);
	cl->setDescriptorHeapSRV(WATER_RS_FRAME_CONSTANTS, 4, common.aoTexture ? common.aoTexture : render_resources::whiteTexture);
	cl->setDescriptorHeapSRV(WATER_RS_FRAME_CONSTANTS, 5, common.sssTexture ? common.sssTexture : render_resources::whiteTexture);
	cl->setDescriptorHeapSRV(WATER_RS_FRAME_CONSTANTS, 6, common.ssrTexture ? common.ssrTexture->defaultSRV : nullTexture);
}

PIPELINE_RENDER_IMPL(water_pipeline, water_render_data)
{
	PROFILE_ALL(cl, "Water");

	water_cb cb;
	cb.deepColor = data.settings.deepWaterColor;
	cb.shallowColor = data.settings.shallowWaterColor;
	cb.shallowDepth = data.settings.shallowDepth;
	cb.transitionStrength = data.settings.transitionStrength;
	cb.uvOffset = normalize(vec2(1.f, 1.f)) * data.time * 0.05f;
	cb.uvScale = data.settings.uvScale;
	cb.normalmapStrength = data.settings.normalStrength;

	cl->setGraphics32BitConstants(WATER_RS_TRANSFORM, transform_cb{ viewProj * data.m, data.m });
	cl->setGraphics32BitConstants(WATER_RS_SETTINGS, cb);
	cl->draw(4, 1, 0, 0);
}

void water_component::render(const render_camera& camera, transparent_render_pass* renderPass, vec3 positionOffset, vec2 scale, float dt, uint32 entityID)
{
	time += dt;
	renderPass->renderObject<water_pipeline, water_render_data>({ createModelMatrix(positionOffset, quat::identity, vec3(scale.x, 1.f, scale.y)), settings, time });
}
