#pragma once

#include "dx/dx_pipeline.h"
#include "core/camera.h"
#include "light_source.h"
#include "render_pass.h"
#include "camera.hlsli"




void initializeRenderUtils();
void endFrameCommon();

void buildCameraConstantBuffer(const render_camera& camera, float cameraJitterStrength, camera_cb& outCB);
void buildCameraConstantBuffer(const render_camera& camera, vec2 jitter, camera_cb& outCB);

void waitForSkinningToFinish();



static constexpr DXGI_FORMAT hdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
static constexpr DXGI_FORMAT ldrFormat = DXGI_FORMAT_R10G10B10A2_UNORM; // Not really LDR. But I don't like the idea of converting to 8 bit and then to sRGB in separate passes.

static constexpr DXGI_FORMAT worldNormalsFormat = DXGI_FORMAT_R16G16_FLOAT;
static constexpr DXGI_FORMAT screenVelocitiesFormat = DXGI_FORMAT_R16G16_FLOAT;
static constexpr DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
static constexpr DXGI_FORMAT linearDepthFormat = DXGI_FORMAT_R32_FLOAT;
static constexpr DXGI_FORMAT objectIDsFormat = DXGI_FORMAT_R32_UINT;
static constexpr DXGI_FORMAT reflectanceFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // Fresnel (xyz), roughness (w).

static constexpr DXGI_FORMAT aoFormat = DXGI_FORMAT_R8_UNORM;
static constexpr DXGI_FORMAT sssFormat = DXGI_FORMAT_R8_UNORM;

static constexpr DXGI_FORMAT shadowDepthFormat = DXGI_FORMAT_D16_UNORM;


static constexpr DXGI_FORMAT opaqueLightPassFormats[] = { hdrFormat, worldNormalsFormat, reflectanceFormat };
static constexpr DXGI_FORMAT transparentLightPassFormats[] = { hdrFormat };
static constexpr DXGI_FORMAT skyPassFormats[] = { hdrFormat, screenVelocitiesFormat, objectIDsFormat };

enum color_depth
{
	color_depth_8,
	color_depth_10,
};

static constexpr DXGI_FORMAT colorDepthToFormat(color_depth colorDepth)
{
	return (colorDepth == color_depth_8) ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R10G10B10A2_UNORM;
}


