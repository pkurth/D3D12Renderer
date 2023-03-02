#pragma once

#include "scene.h"
#include "core/camera.h"
#include "rendering/main_renderer.h"

void serializeSceneToYAMLFile(editor_scene& scene, const renderer_settings& rendererSettings);
bool deserializeSceneFromYAMLFile(editor_scene& scene, renderer_settings& rendererSettings, std::string& environmentName);
