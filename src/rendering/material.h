#pragma once

#include "light_source.h"
#include "dx/dx_buffer.h"
#include "dx/dx_texture.h"

struct dx_command_list;

struct common_material_info
{
	ref<dx_texture> sky;
	ref<dx_texture> irradiance;
	ref<dx_texture> environment;
	ref<dx_texture> brdf;

	ref<dx_texture> tiledCullingGrid;
	ref<dx_buffer> tiledObjectsIndexList;
	ref<dx_buffer> tiledSpotLightIndexList;
	ref<dx_buffer> pointLightBuffer;
	ref<dx_buffer> spotLightBuffer;
	ref<dx_buffer> decalBuffer;
	ref<dx_buffer> pointLightShadowInfoBuffer;
	ref<dx_buffer> spotLightShadowInfoBuffer;

	ref<dx_texture> decalTextureAtlas;

	ref<dx_texture> shadowMap;

	ref<dx_texture> volumetricsTexture;

	// These two are only set, if the material is rendered after the opaque pass.
	ref<dx_texture> opaqueDepth;
	ref<dx_texture> worldNormals;

	dx_dynamic_constant_buffer cameraCBV;
	dx_dynamic_constant_buffer sunCBV;

	float environmentIntensity;
	float skyIntensity;
};


typedef void (*material_setup_function)(dx_command_list*, const common_material_info&);

// All materials must conform to the following standards:
// - Have a static function setupOpaquePipeline and/or setupTransparentPipeline of type material_setup_function, which binds the shader and initializes common stuff.
// - Have a function void prepareForRendering, which sets up uniforms specific to this material instance.
// - Initialize the shader to the main renderer's HDR render target and have the depth test to EQUAL.
// - Currently all materials must use the default_vs vertex shader and have the transform bound to root parameter 0.


struct material_base
{
	virtual void prepareForRendering(dx_command_list* cl) = 0;
};
