#pragma once

#include "geometry/mesh.h"
#include "rendering/material.h"
#include "rendering/pbr.h"

struct tree_settings
{
	float bendStrength;
};

struct tree_component
{
	tree_settings settings;

	void render(struct opaque_render_pass* renderPass, const trs& transform, const ref<multi_mesh>& mesh, float dt);
};



void initializeTreePipelines();

ref<multi_mesh> loadTreeMeshFromFile(const fs::path& sceneFilename);
ref<multi_mesh> loadTreeMeshFromHandle(asset_handle handle);

