#pragma once

#include "core/math.h"
#include "material.h"

enum pbr_material_shader
{
	pbr_material_shader_default,
	pbr_material_shader_double_sided,
	pbr_material_shader_alpha_cutout,
	pbr_material_shader_transparent,

	pbr_material_shader_count,
};

static const char* pbrMaterialShaderNames[] =
{
	"Default",
	"Double sided",
	"Alpha cutout",
	"Transparent",
};

struct pbr_material_desc
{
	fs::path albedo;
	fs::path normal;
	fs::path roughness;
	fs::path metallic;

	vec4 emission = vec4(0.f);
	vec4 albedoTint = vec4(1.f);
	float roughnessOverride = 1.f;
	float metallicOverride = 0.f;
	pbr_material_shader shader = pbr_material_shader_default;
	float uvScale = 1.f;
	float translucency = 0.f;
};

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
		pbr_material_shader shader,
		float uvScale,
		float translucency)
		: albedo(albedo),
		normal(normal),
		roughness(roughness),
		metallic(metallic),
		emission(emission),
		albedoTint(albedoTint),
		roughnessOverride(roughnessOverride),
		metallicOverride(metallicOverride),
		shader(shader),
		uvScale(uvScale),
		translucency(translucency) {}

	ref<dx_texture> albedo;
	ref<dx_texture> normal;
	ref<dx_texture> roughness;
	ref<dx_texture> metallic;

	vec4 emission;
	vec4 albedoTint;
	float roughnessOverride;
	float metallicOverride;
	pbr_material_shader shader;
	float uvScale;
	float translucency;
};

ref<pbr_material> createPBRMaterial(const fs::path& albedoTex, const fs::path& normalTex, const fs::path& roughTex, const fs::path& metallicTex,
	const vec4& emission = vec4(0.f), const vec4& albedoTint = vec4(1.f), float roughOverride = 1.f, float metallicOverride = 0.f,
	pbr_material_shader shader = pbr_material_shader_default, float uvScale = 1.f, float translucency = 0.f, bool disableTextureCompression = false);
ref<pbr_material> createPBRMaterial(const ref<dx_texture>& albedoTex, const ref<dx_texture>& normalTex, const ref<dx_texture>& roughTex, const ref<dx_texture>& metallicTex,
	const vec4& emission = vec4(0.f), const vec4& albedoTint = vec4(1.f), float roughOverride = 1.f, float metallicOverride = 0.f,
	pbr_material_shader shader = pbr_material_shader_default, float uvScale = 1.f, float translucency = 0.f);
ref<pbr_material> createPBRMaterial(const pbr_material_desc& desc);

ref<pbr_material> getDefaultPBRMaterial();

