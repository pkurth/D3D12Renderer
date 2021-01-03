#pragma once

#include "math.h"
#include "material.h"

struct pbr_material : material_base
{
	static void initializePipeline();

	pbr_material() = default;
	pbr_material(ref<dx_texture> albedo, ref<dx_texture> normal, ref<dx_texture> roughness, ref<dx_texture> metallic, const vec4& albedoTint, float roughnessOverride, float metallicOverride)
		: albedo(albedo), normal(normal), roughness(roughness), metallic(metallic), albedoTint(albedoTint), roughnessOverride(roughnessOverride), metallicOverride(metallicOverride) {}

	ref<dx_texture> albedo;
	ref<dx_texture> normal;
	ref<dx_texture> roughness;
	ref<dx_texture> metallic;

	vec4 albedoTint;
	float roughnessOverride;
	float metallicOverride;

	void prepareForRendering(struct dx_command_list* cl);
	static void setupPipeline(dx_command_list* cl, const common_material_info& info);
};

struct pbr_environment
{
	ref<dx_texture> sky;
	ref<dx_texture> environment;
	ref<dx_texture> irradiance;
};

ref<pbr_material> createPBRMaterial(const char* albedoTex, const char* normalTex, const char* roughTex, const char* metallicTex, 
	const vec4& albedoTint = vec4(1.f, 1.f, 1.f, 1.f), float roughOverride = 1.f, float metallicOverride = 0.f);

ref<pbr_material> getDefaultPBRMaterial();

ref<pbr_environment> createEnvironment(const char* filename, uint32 skyResolution = 2048, uint32 environmentResolution = 128, uint32 irradianceResolution = 32, bool asyncCompute = false);

