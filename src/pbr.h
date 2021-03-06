#pragma once

#include "math.h"
#include "material.h"

struct pbr_material : material_base
{
	static void initializePipeline();

	pbr_material() = default;
	pbr_material(ref<dx_texture> albedo, ref<dx_texture> normal, ref<dx_texture> roughness, ref<dx_texture> metallic, const vec4& emission, const vec4& albedoTint, float roughnessOverride, float metallicOverride)
		: albedo(albedo), normal(normal), roughness(roughness), metallic(metallic), emission(emission), albedoTint(albedoTint), roughnessOverride(roughnessOverride), metallicOverride(metallicOverride) {}

	ref<dx_texture> albedo;
	ref<dx_texture> normal;
	ref<dx_texture> roughness;
	ref<dx_texture> metallic;

	vec4 emission;
	vec4 albedoTint;
	float roughnessOverride;
	float metallicOverride;

	void prepareForRendering(struct dx_command_list* cl);
	static void setupOpaquePipeline(dx_command_list* cl, const common_material_info& info);
	static void setupTransparentPipeline(dx_command_list* cl, const common_material_info& info);

private:
	static void setupCommon(dx_command_list* cl, const common_material_info& info);
};

struct pbr_environment
{
	ref<dx_texture> sky;
	ref<dx_texture> environment;
	ref<dx_texture> irradiance;

	std::string name;
};

ref<pbr_material> createPBRMaterial(const std::string& albedoTex, const std::string& normalTex, const std::string& roughTex, const std::string& metallicTex,
	const vec4& emission = vec4(0.f), const vec4& albedoTint = vec4(1.f), float roughOverride = 1.f, float metallicOverride = 0.f);

ref<pbr_material> getDefaultPBRMaterial();

ref<pbr_environment> createEnvironment(const std::string& filename, uint32 skyResolution = 2048, uint32 environmentResolution = 128, uint32 irradianceResolution = 32, bool asyncCompute = false);

