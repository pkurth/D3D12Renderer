#include "pch.h"
#include "pbr_material.h"



struct material_key
{
	fs::path albedoTex, normalTex, roughTex, metallicTex;
	vec4 emission;
	vec4 albedoTint;
	float roughnessOverride, metallicOverride;
	pbr_material_shader shader;
	float uvScale;
	float translucency;
};

namespace std
{
	template<>
	struct hash<material_key>
	{
		size_t operator()(const material_key& x) const
		{
			size_t seed = 0;

			hash_combine(seed, x.albedoTex);
			hash_combine(seed, x.normalTex);
			hash_combine(seed, x.roughTex);
			hash_combine(seed, x.metallicTex);
			hash_combine(seed, x.emission);
			hash_combine(seed, x.albedoTint);
			hash_combine(seed, x.roughnessOverride);
			hash_combine(seed, x.metallicOverride);
			hash_combine(seed, (uint32)x.shader);
			hash_combine(seed, x.uvScale);
			hash_combine(seed, x.translucency);

			return seed;
		}
	};
}

static bool operator==(const material_key& a, const material_key& b)
{
	return a.albedoTex == b.albedoTex
		&& a.normalTex == b.normalTex
		&& a.roughTex == b.roughTex
		&& a.metallicTex == b.metallicTex
		&& a.emission == b.emission
		&& a.albedoTint == b.albedoTint
		&& a.roughnessOverride == b.roughnessOverride
		&& a.metallicOverride == b.metallicOverride
		&& a.shader == b.shader
		&& a.uvScale == b.uvScale
		&& a.translucency == b.translucency;
}

ref<pbr_material> createPBRMaterial(
	const fs::path& albedoTex,
	const fs::path& normalTex,
	const fs::path& roughTex,
	const fs::path& metallicTex,
	const vec4& emission,
	const vec4& albedoTint,
	float roughOverride,
	float metallicOverride,
	pbr_material_shader shader,
	float uvScale,
	float translucency,
	bool disableTextureCompression)
{
	material_key s =
	{
		!albedoTex.empty() ? albedoTex : "",
		!normalTex.empty() ? normalTex : "",
		!roughTex.empty() ? roughTex : "",
		!metallicTex.empty() ? metallicTex : "",
		emission,
		albedoTint,
		!roughTex.empty() ? 1.f : roughOverride,			// If texture is set, override does not matter, so set it to consistent value.
		!metallicTex.empty() ? 0.f : metallicOverride,		// If texture is set, override does not matter, so set it to consistent value.
		shader,
		uvScale,
		translucency
	};


	static std::unordered_map<material_key, weakref<pbr_material>> cache;
	static std::mutex mutex;

	mutex.lock();

	auto sp = cache[s].lock();
	if (!sp)
	{
		ref<pbr_material> material = make_ref<pbr_material>();

		uint32 removeFlags = 0xFFFFFFFF;
		if (disableTextureCompression)
		{
			removeFlags = ~image_load_flags_compress;
		}

		if (!albedoTex.empty()) material->albedo = loadTextureFromFile(albedoTex, image_load_flags_default & removeFlags);
		if (!normalTex.empty()) material->normal = loadTextureFromFile(normalTex, image_load_flags_default_noncolor & removeFlags);
		if (!roughTex.empty()) material->roughness = loadTextureFromFile(roughTex, image_load_flags_default_noncolor & removeFlags);
		if (!metallicTex.empty()) material->metallic = loadTextureFromFile(metallicTex, image_load_flags_default_noncolor & removeFlags);
		material->emission = emission;
		material->albedoTint = albedoTint;
		material->roughnessOverride = roughOverride;
		material->metallicOverride = metallicOverride;
		material->shader = shader;
		material->uvScale = uvScale;
		material->translucency = translucency;

		cache[s] = sp = material;
	}

	mutex.unlock();
	return sp;
}

ref<pbr_material> createPBRMaterial(
	const ref<dx_texture>& albedoTex,
	const ref<dx_texture>& normalTex,
	const ref<dx_texture>& roughTex,
	const ref<dx_texture>& metallicTex,
	const vec4& emission,
	const vec4& albedoTint,
	float roughOverride,
	float metallicOverride,
	pbr_material_shader shader,
	float uvScale,
	float translucency)
{
	ref<pbr_material> material = make_ref<pbr_material>();

	material->albedo = albedoTex;
	material->normal = normalTex;
	material->roughness = roughTex;
	material->metallic = metallicTex;
	material->emission = emission;
	material->albedoTint = albedoTint;
	material->roughnessOverride = roughOverride;
	material->metallicOverride = metallicOverride;
	material->shader = shader;
	material->uvScale = uvScale;
	material->translucency = translucency;

	return material;
}

ref<pbr_material> createPBRMaterial(const pbr_material_desc& desc)
{
	return createPBRMaterial(desc.albedo, desc.normal, desc.roughness, desc.metallic, desc.emission, desc.albedoTint, desc.roughnessOverride,
		desc.metallicOverride, desc.shader, desc.uvScale, desc.translucency);
}

ref<pbr_material> getDefaultPBRMaterial()
{
	static ref<pbr_material> material = make_ref<pbr_material>(nullptr, nullptr, nullptr, nullptr,
		vec4(0.f), vec4(1.f, 0.f, 1.f, 1.f), 1.f, 0.f, pbr_material_shader_default, 1.f, 0.f);
	return material;
}

