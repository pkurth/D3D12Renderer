#pragma once

#include "core/math.h"
#include "material.h"
#include "render_pass.h"

struct dx_command_list;

struct pbr_material
{
	pbr_material() = default;
	pbr_material(ref<dx_texture> albedo, 
		ref<dx_texture> normal, 
		ref<dx_texture> roughness, 
		ref<dx_texture> metallic, 
		const vec4& emission, 
		const vec4& albedoTint, 
		float roughnessOverride, 
		float metallicOverride,
		bool doubleSided)
		: albedo(albedo), 
		normal(normal), 
		roughness(roughness), 
		metallic(metallic), 
		emission(emission), 
		albedoTint(albedoTint), 
		roughnessOverride(roughnessOverride), 
		metallicOverride(metallicOverride),
		doubleSided(doubleSided) {}

	ref<dx_texture> albedo;
	ref<dx_texture> normal;
	ref<dx_texture> roughness;
	ref<dx_texture> metallic;

	vec4 emission;
	vec4 albedoTint;
	float roughnessOverride;
	float metallicOverride;
	bool doubleSided;
};

struct opaque_pbr_pipeline
{
	using material_t = ref<pbr_material>;

	static void initialize();

	PIPELINE_RENDER_DECL;

	struct standard;
	struct double_sided;
};

struct opaque_pbr_pipeline::standard : opaque_pbr_pipeline
{
	PIPELINE_SETUP_DECL;
};

struct opaque_pbr_pipeline::double_sided : opaque_pbr_pipeline
{
	PIPELINE_SETUP_DECL;
};

struct transparent_pbr_pipeline
{
	using material_t = ref<pbr_material>;

	static void initialize();

	PIPELINE_SETUP_DECL;
	PIPELINE_RENDER_DECL;
};





struct pbr_environment
{
	ref<dx_texture> sky;
	ref<dx_texture> environment;
	ref<dx_texture> irradiance;

	std::string name;
};

ref<pbr_material> createPBRMaterial(const std::string& albedoTex, const std::string& normalTex, const std::string& roughTex, const std::string& metallicTex,
	const vec4& emission = vec4(0.f), const vec4& albedoTint = vec4(1.f), float roughOverride = 1.f, float metallicOverride = 0.f, bool doubleSided = false);
ref<pbr_material> getDefaultPBRMaterial();

ref<pbr_environment> createEnvironment(const std::string& filename, uint32 skyResolution = 2048, uint32 environmentResolution = 128, uint32 irradianceResolution = 32, bool asyncCompute = false);

