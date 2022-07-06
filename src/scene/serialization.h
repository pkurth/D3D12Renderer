#pragma once

#include "scene.h"
#include "core/camera.h"
#include "rendering/main_renderer.h"

void serializeSceneToDisk(game_scene& scene, const render_camera& camera, const renderer_settings& rendererSettings);
bool deserializeSceneFromDisk(game_scene& scene, render_camera& camera, renderer_settings& rendererSettings, std::string& environmentName);
