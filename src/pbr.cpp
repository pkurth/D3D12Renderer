#include "pch.h"
#include "pbr.h"
#include "texture.h"
#include "texture_preprocessing.h"
#include "dx_context.h"

#include <unordered_map>

struct material_key
{
	std::string albedoTex, normalTex, roughTex, metallicTex;
	vec4 albedoTint;
	float roughnessOverride, metallicOverride;
};

namespace std
{
	// Source: https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
	template <typename T>
	inline void hash_combine(std::size_t& seed, const T& v)
	{
		std::hash<T> hasher;
		seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}
}

namespace std
{
	template<>
	struct hash<vec4>
	{
		size_t operator()(const vec4& x) const
		{
			std::size_t seed = 0;

			hash_combine(seed, x.x);
			hash_combine(seed, x.y);
			hash_combine(seed, x.z);
			hash_combine(seed, x.w);

			return seed;
		}
	};

	template<>
	struct hash<material_key>
	{
		size_t operator()(const material_key& x) const
		{
			std::size_t seed = 0;

			hash_combine(seed, x.albedoTex);
			hash_combine(seed, x.normalTex);
			hash_combine(seed, x.roughTex);
			hash_combine(seed, x.metallicTex);
			hash_combine(seed, x.albedoTint);
			hash_combine(seed, x.roughnessOverride);
			hash_combine(seed, x.metallicOverride);

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
		&& a.albedoTint == b.albedoTint
		&& a.roughnessOverride == b.roughnessOverride
		&& a.metallicOverride == b.metallicOverride;
}

ref<pbr_material> createMaterial(const char* albedoTex, const char* normalTex, const char* roughTex, const char* metallicTex, const vec4& albedoTint, float roughOverride, float metallicOverride)
{
	material_key s =
	{
		albedoTex ? albedoTex : "",
		normalTex ? normalTex : "",
		roughTex ? roughTex : "",
		metallicTex ? metallicTex : "",
		albedoTint,
		roughTex ? 1.f : roughOverride, // If texture is set, override does not matter, so set it to consistent value.
		metallicTex ? 0.f : metallicOverride, // If texture is set, override does not matter, so set it to consistent value.
	};


	static std::unordered_map<material_key, weakref<pbr_material>> cache;
	static thread_mutex mutex = createMutex();

	mutex.lock();

	auto sp = cache[s].lock();
	if (!sp)
	{
		ref<pbr_material> material = make_ref<pbr_material>();

		if (albedoTex) material->albedo = loadTextureFromFile(albedoTex);
		if (normalTex) material->normal = loadTextureFromFile(normalTex, texture_load_flags_default | texture_load_flags_noncolor);
		if (roughTex) material->roughness = loadTextureFromFile(roughTex, texture_load_flags_default | texture_load_flags_noncolor);
		if (metallicTex) material->metallic = loadTextureFromFile(metallicTex, texture_load_flags_default | texture_load_flags_noncolor);
		material->albedoTint = albedoTint;
		material->roughnessOverride = roughOverride;
		material->metallicOverride = metallicOverride;

		cache[s] = sp = material;
	}

	mutex.unlock();
	return sp;
}

ref<pbr_environment> createEnvironment(const char* filename, uint32 skyResolution, uint32 environmentResolution, uint32 irradianceResolution, bool asyncCompute)
{
	static std::unordered_map<std::string, weakref<pbr_environment>> cache;
	static thread_mutex mutex = createMutex();

	mutex.lock();

	std::string s = filename;

	auto sp = cache[s].lock();
	if (!sp)
	{
		ref<pbr_environment> environment = make_ref<pbr_environment>();

		ref<dx_texture> equiSky = loadTextureFromFile(filename,
			texture_load_flags_noncolor | texture_load_flags_cache_to_dds | texture_load_flags_gen_mips_on_cpu);

		dx_command_list* cl;
		if (asyncCompute)
		{
			dxContext.computeQueue.waitForOtherQueue(dxContext.copyQueue);
			cl = dxContext.getFreeComputeCommandList(true);
		}
		else
		{
			dxContext.renderQueue.waitForOtherQueue(dxContext.copyQueue);
			cl = dxContext.getFreeRenderCommandList();
		}
		//generateMipMapsOnGPU(cl, equiSky);
		environment->sky = equirectangularToCubemap(cl, equiSky, skyResolution, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
		environment->environment = prefilterEnvironment(cl, environment->sky, environmentResolution);
		environment->irradiance = cubemapToIrradiance(cl, environment->sky, irradianceResolution);
		dxContext.executeCommandList(cl);

		dxContext.retireObject(equiSky->resource);

		cache[s] = sp = environment;
	}

	mutex.unlock();
	return sp;
}
