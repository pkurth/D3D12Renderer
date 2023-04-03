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

	uint32 albedoFlags = image_load_flags_default;
	uint32 normalFlags = image_load_flags_default_noncolor;
	uint32 roughnessFlags = image_load_flags_default_noncolor;
	uint32 metallicFlags = image_load_flags_default_noncolor;

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

ref<pbr_material> createPBRMaterial(const pbr_material_desc& desc);
ref<pbr_material> getDefaultPBRMaterial();

