#pragma once

#include "dx.h"

struct dx_command_list;
struct dx_texture;

void initializeTexturePreprocessing();

void generateMipMapsOnGPU(dx_command_list* cl, ref<dx_texture>& texture);
ref<dx_texture> equirectangularToCubemap(dx_command_list* cl, const ref<dx_texture>& equirectangular, uint32 resolution, uint32 numMips = 0, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
ref<dx_texture> cubemapToIrradiance(dx_command_list* cl, const ref<dx_texture>& environment, uint32 resolution = 32, uint32 sourceSlice = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, float uvzScale = 1.f);
ref<dx_texture> prefilterEnvironment(dx_command_list* cl, const ref<dx_texture>& environment, uint32 resolution);
ref<dx_texture> integrateBRDF(dx_command_list* cl, uint32 resolution = 512);
void gaussianBlur(dx_command_list* cl, ref<dx_texture> tex, ref<dx_texture> tmpTex, uint32 numIterations = 1);
