#pragma once

#include "dx/dx.h"
#include "light_source.h"

struct dx_command_list;
struct dx_texture;
struct dx_buffer;

void initializeTexturePreprocessing();

void generateMipMapsOnGPU(dx_command_list* cl, ref<dx_texture>& texture);
ref<dx_texture> equirectangularToCubemap(dx_command_list* cl, const ref<dx_texture>& equirectangular, uint32 resolution, uint32 numMips = 0, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
ref<dx_texture> cubemapToIrradiance(dx_command_list* cl, const ref<dx_texture>& environment, uint32 resolution = 32, uint32 sourceSlice = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
ref<dx_buffer> cubemapToIrradianceSH(dx_command_list* cl, const ref<dx_texture>& environment);
ref<dx_texture> prefilterEnvironment(dx_command_list* cl, const ref<dx_texture>& environment, uint32 resolution);
ref<dx_texture> integrateBRDF(dx_command_list* cl, uint32 resolution = 512);
