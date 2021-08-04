#pragma once

#include "core/camera.h"
#include "light_source.h"
#include "render_pass.h"
#include "camera.hlsli"

void initializeRenderUtils();
void endFrameCommon();

void buildCameraConstantBuffer(const render_camera& camera, float cameraJitterStrength, camera_cb& outCB);
void buildCameraConstantBuffer(const render_camera& camera, vec2 jitter, camera_cb& outCB);

void assignSunShadowMapViewports(const sun_shadow_render_pass* sunShadowRenderPass, directional_light_cb& sun);

void waitForSkinningToFinish();
