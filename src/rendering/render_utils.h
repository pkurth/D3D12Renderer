#pragma once

#include "dx/dx_pipeline.h"
#include "core/camera.h"
#include "light_source.h"
#include "render_pass.h"
#include "camera.hlsli"




enum stencil_flags
{
	stencil_flag_selected_object = (1 << 0),
};


void initializeRenderUtils();
void endFrameCommon();

void buildCameraConstantBuffer(const render_camera& camera, float cameraJitterStrength, camera_cb& outCB);
void buildCameraConstantBuffer(const render_camera& camera, vec2 jitter, camera_cb& outCB);

void assignSunShadowMapViewports(const sun_shadow_render_pass* sunShadowRenderPass, directional_light_cb& sun);

void waitForSkinningToFinish();




extern dx_pipeline depthOnlyPipeline;
extern dx_pipeline animatedDepthOnlyPipeline;
extern dx_pipeline shadowPipeline;
extern dx_pipeline pointLightShadowPipeline;

extern dx_pipeline textureSkyPipeline;
extern dx_pipeline proceduralSkyPipeline;
extern dx_pipeline preethamSkyPipeline;
extern dx_pipeline sphericalHarmonicsSkyPipeline;

extern dx_pipeline outlineMarkerPipeline;
extern dx_pipeline outlineDrawerPipeline;


extern dx_command_signature particleCommandSignature;



extern constexpr DXGI_FORMAT hdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
extern constexpr DXGI_FORMAT worldNormalsFormat = DXGI_FORMAT_R16G16_FLOAT;
extern constexpr DXGI_FORMAT hdrDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
extern constexpr DXGI_FORMAT linearDepthFormat = DXGI_FORMAT_R32_FLOAT;
extern constexpr DXGI_FORMAT screenVelocitiesFormat = DXGI_FORMAT_R16G16_FLOAT;
extern constexpr DXGI_FORMAT objectIDsFormat = DXGI_FORMAT_R32_UINT;
extern constexpr DXGI_FORMAT reflectanceFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // Fresnel (xyz), roughness (w).
extern constexpr DXGI_FORMAT reflectionFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

extern constexpr DXGI_FORMAT hdrPostProcessFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
extern constexpr DXGI_FORMAT ldrPostProcessFormat = DXGI_FORMAT_R11G11B10_FLOAT; // Not really LDR. But I don't like the idea of converting to 8 bit and then to sRGB in separate passes.

extern constexpr DXGI_FORMAT overlayFormat = ldrPostProcessFormat;
extern constexpr DXGI_FORMAT overlayDepthFormat = hdrDepthStencilFormat;

extern constexpr DXGI_FORMAT opaqueLightPassFormats[] = { hdrFormat, worldNormalsFormat, reflectanceFormat };
extern constexpr DXGI_FORMAT transparentLightPassFormats[] = { hdrPostProcessFormat };
extern constexpr DXGI_FORMAT skyPassFormats[] = { hdrFormat, screenVelocitiesFormat, objectIDsFormat };




