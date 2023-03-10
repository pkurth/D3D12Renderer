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
};


struct tree_pipeline
{
	using render_data_t = pbr_render_data;

	PIPELINE_SETUP_DECL;
	PIPELINE_RENDER_DECL;
};



void initializeTreePipelines();

ref<multi_mesh> loadTreeMeshFromFile(const fs::path& sceneFilename);
ref<multi_mesh> loadTreeMeshFromHandle(asset_handle handle);

