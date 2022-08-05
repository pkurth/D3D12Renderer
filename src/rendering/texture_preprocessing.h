#pragma once

#include "dx/dx.h"
#include "light_source.h"

struct dx_command_list;
struct dx_texture;
struct dx_buffer;

void initializeTexturePreprocessing();

void generateMipMapsOnGPU(dx_command_list* cl, ref<dx_texture>& texture);
ref<dx_texture> equirectangularToCubemap(dx_command_list* cl, const ref<dx_texture>& equirectangular, uint32 resolution, uint32 numMips = 0, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);

void texturedSkyToIrradiance(dx_command_list* cl, const ref<dx_texture>& sky, ref<dx_texture>& outIrradiance);
void texturedSkyToPrefilteredRadiance(dx_command_list* cl, const ref<dx_texture>& sky, ref<dx_texture>& outPrefilteredRadiance);

void proceduralSkyToIrradiance(dx_command_list* cl, vec3 sunDirection, ref<dx_texture>& outIrradiance);
//void proceduralSkyToPrefilteredRadiance(dx_command_list* cl, vec3 sunDirection, ref<dx_texture>& outPrefilteredRadiance);

ref<dx_texture> integrateBRDF(dx_command_list* cl, uint32 resolution = 512);

void texturedSkyToIrradianceSH(dx_command_list* cl, const ref<dx_texture>& environment, ref<dx_buffer> shBuffer, uint32 shIndex);
ref<dx_buffer> texturedSkyToIrradianceSH(dx_command_list* cl, const ref<dx_texture>& environment); // Creates buffer with one spherical harmonics set.
