#pragma once

#include "scene.h"
#include "core/camera.h"
#include "rendering/main_renderer.h"
#include "rendering/light_source.h"

void serializeSceneToDisk(scene& appScene, const render_camera& camera, const directional_light& sun, const renderer_settings& rendererSettings, const ref<pbr_environment>& environment);
bool deserializeSceneFromDisk(scene& appScene, render_camera& camera, directional_light& sun, renderer_settings& rendererSettings, std::string& environmentName);
