#pragma once

#include "geometry/mesh.h"
#include "rendering/material.h"
#include "rendering/pbr.h"
#include "core/job_system.h"

struct tree_settings
{
	float bendStrength;
};

struct tree_component
{
	tree_settings settings;
};

void renderTree(struct opaque_render_pass* renderPass, D3D12_GPU_VIRTUAL_ADDRESS transforms, uint32 numInstances, const multi_mesh* mesh, float dt);


void initializeTreePipelines();

ref<multi_mesh> loadTreeMeshFromFile(const fs::path& sceneFilename);
ref<multi_mesh> loadTreeMeshFromHandle(asset_handle handle);

ref<multi_mesh> loadTreeMeshFromFileAsync(const fs::path& sceneFilename, job_handle parentJob = {});
ref<multi_mesh> loadTreeMeshFromHandleAsync(asset_handle handle, job_handle parentJob = {});

