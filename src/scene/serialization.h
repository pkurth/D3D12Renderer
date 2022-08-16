#pragma once

#include "scene.h"
#include "core/camera.h"
#include "rendering/main_renderer.h"

void serializeSceneToDisk(editor_scene& scene, const renderer_settings& rendererSettings);
bool deserializeSceneFromDisk(editor_scene& scene, renderer_settings& rendererSettings, std::string& environmentName);
