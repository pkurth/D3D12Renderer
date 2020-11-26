#pragma once

#include "dx_render_primitives.h"
#include "math.h"

struct pbr_material
{
	ref<dx_texture> albedo;
	ref<dx_texture> normal;
	ref<dx_texture> roughness;
	ref<dx_texture> metallic;

	vec4 albedoTint;
	float roughnessOverride;
	float metallicOverride;
};

struct pbr_environment
{
	ref<dx_texture> sky;
	ref<dx_texture> environment;
	ref<dx_texture> irradiance;
};

ref<pbr_material> createMaterial(const char* albedoTex, const char* normalTex, const char* roughTex, const char* metallicTex, 
	const vec4& albedoTint = vec4(1.f, 1.f, 1.f, 1.f), float roughOverride = 1.f, float metallicOverride = 0.f);

ref<pbr_material> getDefaultMaterial();

ref<pbr_environment> createEnvironment(const char* filename, uint32 skyResolution = 2048, uint32 environmentResolution = 128, uint32 irradianceResolution = 32, bool asyncCompute = false);

