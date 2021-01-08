#include "pch.h"
#include "assimp.h"
#include "pbr.h"

#include <assimp/Exporter.hpp>
#include <assimp/postprocess.h>

#include <filesystem>

namespace fs = std::filesystem;

#define CACHE_FORMAT "assbin"

const aiScene* loadAssimpSceneFile(const char* filepathRaw, Assimp::Importer& importer)
{
	fs::path filepath = filepathRaw;
	fs::path extension = filepath.extension();

	fs::path cachedFilename = filepath;
	cachedFilename.replace_extension(".cache." CACHE_FORMAT);

	fs::path cacheFilepath = L"bin_cache" / cachedFilename;

	const aiScene* scene = 0;

	{
		// Look for cached.

		WIN32_FILE_ATTRIBUTE_DATA cachedData;
		if (GetFileAttributesExW(cacheFilepath.c_str(), GetFileExInfoStandard, &cachedData))
		{
			FILETIME cachedFiletime = cachedData.ftLastWriteTime;

			WIN32_FILE_ATTRIBUTE_DATA originalData;
			assert(GetFileAttributesExW(filepath.c_str(), GetFileExInfoStandard, &originalData));
			FILETIME originalFiletime = originalData.ftLastWriteTime;

			if (CompareFileTime(&cachedFiletime, &originalFiletime) >= 0)
			{
				// Cached file is newer than original, so load this.
				scene = importer.ReadFile(cacheFilepath.string(), 0);
			}
		}
	}

	if (!scene)
	{
		importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.f);
		importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
		importer.SetPropertyInteger(AI_CONFIG_PP_SLM_VERTEX_LIMIT, UINT16_MAX); // So that we can use 16 bit indices.

		uint32 importFlags = aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_OptimizeGraph | aiProcess_FlipUVs;
		uint32 exportFlags = 0;

		scene = importer.ReadFile(filepath.string(), importFlags);

		fs::create_directories(cacheFilepath.parent_path());
		Assimp::Exporter exporter;
#if 0
		uint32 exporterCount = exporter.GetExportFormatCount();
		for (uint32 i = 0; i < exporterCount; ++i)
		{
			const aiExportFormatDesc* desc = exporter.GetExportFormatDescription(i);
			int a = 0;
		}
#endif

		exporter.Export(scene, CACHE_FORMAT, cacheFilepath.string(), exportFlags);
	}

	if (scene)
	{
		scene = importer.GetScene();
	}

	return scene;
}

ref<pbr_material> loadAssimpMaterial(const aiMaterial* material)
{
	aiString diffuse, normal, roughness, metallic;
	bool hasDiffuse = material->GetTexture(aiTextureType_DIFFUSE, 0, &diffuse) == aiReturn_SUCCESS;
	bool hasNormal = material->GetTexture(aiTextureType_HEIGHT, 0, &normal) == aiReturn_SUCCESS ||
		material->GetTexture(aiTextureType_NORMALS, 0, &normal) == aiReturn_SUCCESS;
	bool hasRoughness = material->GetTexture(aiTextureType_SHININESS, 0, &roughness) == aiReturn_SUCCESS;
	bool hasMetallic = material->GetTexture(aiTextureType_AMBIENT, 0, &metallic) == aiReturn_SUCCESS;

	const char* albedoName = 0;
	const char* normalName = 0;
	const char* roughnessName = 0;
	const char* metallicName = 0;

	aiColor3D aiColor;
	vec4 albedoTint(1.f);
	if (material->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == aiReturn_SUCCESS)
	{
		albedoTint.x = aiColor.r;
		albedoTint.y = aiColor.g;
		albedoTint.z = aiColor.b;
	}

	float roughnessOverride = 1.f;
	float metallicOverride = 0.f;

	if (hasDiffuse) { albedoName = diffuse.C_Str(); }
	if (hasNormal) { normalName = normal.C_Str(); }
	if (hasRoughness)
	{
		roughnessName = roughness.C_Str();
	}
	else
	{
		float shininess;
		if (material->Get(AI_MATKEY_SHININESS, shininess) != aiReturn_SUCCESS)
		{
			shininess = 80.f; // Default value.
		}
		roughnessOverride = 1.f - sqrt(shininess * 0.01f);
	}

	if (hasMetallic)
	{
		metallicName = metallic.C_Str();
	}
	else
	{
		if (material->Get(AI_MATKEY_REFLECTIVITY, metallicOverride) != aiReturn_SUCCESS)
		{
			metallicOverride = 0.f;
		}
	}

	return createPBRMaterial(albedoName, normalName, roughnessName, metallicName, albedoTint, roughnessOverride, metallicOverride);
}
