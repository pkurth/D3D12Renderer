#pragma once

#include "dx_render_primitives.h"

struct dx_command_list;

void initializeTexturePreprocessing();

void generateMipMapsOnGPU(dx_command_list* cl, dx_texture& texture);
dx_texture equirectangularToCubemap(dx_command_list* cl, dx_texture& equirectangular, uint32 resolution, uint32 numMips = 0, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
dx_texture cubemapToIrradiance(dx_command_list* cl, dx_texture& environment, uint32 resolution = 32, uint32 sourceSlice = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, float uvzScale = 1.f);
dx_texture prefilterEnvironment(dx_command_list* cl, dx_texture& environment, uint32 resolution);
dx_texture integrateBRDF(dx_command_list* cl, uint32 resolution = 512);
