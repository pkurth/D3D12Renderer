#pragma once

#include "camera.h"
#include "camera.hlsli"

void initializeRenderUtils();
void endFrameCommon();

void buildCameraConstantBuffer(const render_camera& camera, float cameraJitterStrength, camera_cb& outCB);
void buildCameraConstantBuffer(const render_camera& camera, vec2 jitter, camera_cb& outCB);
