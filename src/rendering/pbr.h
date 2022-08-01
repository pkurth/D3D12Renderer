#pragma once

#include "core/math.h"
#include "material.h"
#include "render_command.h"

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
		bool doubleSided,
		float uvScale)
		: albedo(albedo), 
		normal(normal), 
		roughness(roughness), 
		metallic(metallic), 
		emission(emission), 
		albedoTint(albedoTint), 
		roughnessOverride(roughnessOverride), 
		metallicOverride(metallicOverride),
		doubleSided(doubleSided),
		uvScale(uvScale) {}

	ref<dx_texture> albedo;
	ref<dx_texture> normal;
	ref<dx_texture> roughness;
	ref<dx_texture> metallic;

	vec4 emission;
	vec4 albedoTint;
	float roughnessOverride;
	float metallicOverride;
	bool doubleSided;
	float uvScale;
};

struct pbr_pipeline
{
	using material_t = ref<pbr_material>;

	static void initialize();

	PIPELINE_RENDER_DECL;

	struct opaque;
	struct opaque_double_sided;
	struct transparent;

protected:
	static void setupPBRCommon(dx_command_list* cl, const common_material_info& info);
};

struct pbr_pipeline::opaque : pbr_pipeline
{
	PIPELINE_SETUP_DECL;
};

struct pbr_pipeline::opaque_double_sided : pbr_pipeline
{
	PIPELINE_SETUP_DECL;
};

struct pbr_pipeline::transparent : pbr_pipeline
{
	PIPELINE_SETUP_DECL;
};



enum pbr_environment_type
{
	pbr_environment_type_uninitialized,
	pbr_environment_type_textured,
	pbr_environment_type_procedural,
};

static const char* pbrEnvironmentTypeNames[] =
{
	"Uninitialized",
	"Textured",
	"Procedural",
};

struct pbr_environment
{
	pbr_environment_type type = pbr_environment_type_uninitialized;
	fs::path name;

	ref<dx_texture> sky;
	ref<dx_texture> irradiance;
	ref<dx_texture> prefilteredRadiance;
};

ref<pbr_material> createPBRMaterial(const fs::path& albedoTex, const fs::path& normalTex, const fs::path& roughTex, const fs::path& metallicTex,
	const vec4& emission = vec4(0.f), const vec4& albedoTint = vec4(1.f), float roughOverride = 1.f, float metallicOverride = 0.f, bool doubleSided = false, float uvScale = 1.f);
ref<pbr_material> getDefaultPBRMaterial();

pbr_environment createPBREnvironment(const fs::path& filename, uint32 skyResolution = 2048, uint32 irradianceResolution = 32, uint32 prefilteredRadianceResolution = 128, bool asyncCompute = false);
pbr_environment createProceduralPBREnvironment(vec3 sunDirection, uint32 irradianceResolution = 32, uint32 prefilteredRadianceResolution = 128, bool asyncCompute = false);
