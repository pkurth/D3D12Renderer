#pragma once

#include "dx.h"
#include "dx_pipeline.h"



enum stencil_flags
{
	stencil_flag_selected_object = (1 << 0),
};

enum aspect_ratio_mode
{
	aspect_ratio_free,
	aspect_ratio_fix_16_9,
	aspect_ratio_fix_16_10,

	aspect_ratio_mode_count,
};

static const char* aspectRatioNames[] =
{
	"Free",
	"16:9",
	"16:10",
};

enum renderer_mode
{
	renderer_mode_rasterized,
	renderer_mode_pathtraced,

	renderer_mode_count,
};

static const char* rendererModeNames[] =
{
	"Rasterized",
	"Path-traced",
};


struct renderer_base
{
	static void initializeCommon(DXGI_FORMAT outputFormat);



	static dx_pipeline depthOnlyPipeline;
	static dx_pipeline animatedDepthOnlyPipeline;
	static dx_pipeline shadowPipeline;
	static dx_pipeline pointLightShadowPipeline;

	static dx_pipeline textureSkyPipeline;
	static dx_pipeline proceduralSkyPipeline;

	static dx_pipeline outlineMarkerPipeline;
	static dx_pipeline outlineDrawerPipeline;


	static dx_command_signature particleCommandSignature;



	static DXGI_FORMAT outputFormat;

	static constexpr DXGI_FORMAT hdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	static constexpr DXGI_FORMAT worldNormalsFormat = DXGI_FORMAT_R16G16_FLOAT;
	static constexpr DXGI_FORMAT hdrDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	static constexpr DXGI_FORMAT linearDepthFormat = DXGI_FORMAT_R32_FLOAT;
	static constexpr DXGI_FORMAT screenVelocitiesFormat = DXGI_FORMAT_R16G16_FLOAT;
	static constexpr DXGI_FORMAT objectIDsFormat = DXGI_FORMAT_R32_UINT;
	static constexpr DXGI_FORMAT reflectanceFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // Fresnel (xyz), roughness (w).
	static constexpr DXGI_FORMAT reflectionFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	static constexpr DXGI_FORMAT hdrPostProcessFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	static constexpr DXGI_FORMAT ldrPostProcessFormat = DXGI_FORMAT_R11G11B10_FLOAT; // Not really LDR. But I don't like the idea of converting to 8 bit and then to sRGB in separate passes.

	static constexpr DXGI_FORMAT overlayFormat = ldrPostProcessFormat;
	static constexpr DXGI_FORMAT overlayDepthFormat = hdrDepthStencilFormat;

	static constexpr DXGI_FORMAT opaqueLightPassFormats[] = { hdrFormat, worldNormalsFormat, reflectanceFormat };
	static constexpr DXGI_FORMAT transparentLightPassFormats[] = { hdrPostProcessFormat };
	static constexpr DXGI_FORMAT skyPassFormats[] = { hdrFormat, screenVelocitiesFormat, objectIDsFormat };
};

