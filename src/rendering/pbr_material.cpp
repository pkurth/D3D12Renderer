#include "pch.h"
#include "pbr_material.h"


namespace std
{
	template<>
	struct hash<pbr_material_desc>
	{
		size_t operator()(const pbr_material_desc& x) const
		{
			size_t seed = 0;

			hash_combine(seed, x.albedo);
			hash_combine(seed, x.normal);
			hash_combine(seed, x.roughness);
			hash_combine(seed, x.metallic);
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

static bool operator==(const pbr_material_desc& a, const pbr_material_desc& b)
{
	return a.albedo == b.albedo
		&& a.normal == b.normal
		&& a.roughness == b.roughness
		&& a.metallic == b.metallic
		&& a.emission == b.emission
		&& a.albedoTint == b.albedoTint
		&& a.roughnessOverride == b.roughnessOverride
		&& a.metallicOverride == b.metallicOverride
		&& a.shader == b.shader
		&& a.uvScale == b.uvScale
		&& a.translucency == b.translucency;
}

static std::unordered_map<pbr_material_desc, weakref<pbr_material>> materialCache;
static std::mutex mutex;

ref<pbr_material> createPBRMaterial(const pbr_material_desc& desc)
{
	mutex.lock();

	auto sp = materialCache[desc].lock();
	if (!sp)
	{
		ref<pbr_material> material = make_ref<pbr_material>();

		if (!desc.albedo.empty()) material->albedo = loadTextureFromFile(desc.albedo, desc.albedoFlags);
		if (!desc.normal.empty()) material->normal = loadTextureFromFile(desc.normal, desc.normalFlags);
		if (!desc.roughness.empty()) material->roughness = loadTextureFromFile(desc.roughness, desc.roughnessFlags);
		if (!desc.metallic.empty()) material->metallic = loadTextureFromFile(desc.metallic, desc.metallicFlags);
		material->emission = desc.emission;
		material->albedoTint = desc.albedoTint;
		material->roughnessOverride = desc.roughnessOverride;
		material->metallicOverride = desc.metallicOverride;
		material->shader = desc.shader;
		material->uvScale = desc.uvScale;
		material->translucency = desc.translucency;

		materialCache[desc] = sp = material;
	}

	mutex.unlock();
	return sp;
}

ref<pbr_material> createPBRMaterialAsync(const pbr_material_desc& desc, job_handle parentJob)
{
	mutex.lock();

	auto sp = materialCache[desc].lock();
	if (!sp)
	{
		ref<pbr_material> material = make_ref<pbr_material>();

		if (!desc.albedo.empty()) material->albedo = loadTextureFromFileAsync(desc.albedo, desc.albedoFlags, parentJob);
		if (!desc.normal.empty()) material->normal = loadTextureFromFileAsync(desc.normal, desc.normalFlags, parentJob);
		if (!desc.roughness.empty()) material->roughness = loadTextureFromFileAsync(desc.roughness, desc.roughnessFlags, parentJob);
		if (!desc.metallic.empty()) material->metallic = loadTextureFromFileAsync(desc.metallic, desc.metallicFlags, parentJob);
		material->emission = desc.emission;
		material->albedoTint = desc.albedoTint;
		material->roughnessOverride = desc.roughnessOverride;
		material->metallicOverride = desc.metallicOverride;
		material->shader = desc.shader;
		material->uvScale = desc.uvScale;
		material->translucency = desc.translucency;

		materialCache[desc] = sp = material;
	}

	mutex.unlock();
	return sp;
}

ref<pbr_material> getDefaultPBRMaterial()
{
	static ref<pbr_material> material = createPBRMaterial({});
	return material;
}

