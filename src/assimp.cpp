#include "pch.h"
#include "assimp.h"
#include "pbr.h"

#include <assimp/Exporter.hpp>
#include <assimp/postprocess.h>
#include <assimp/LogStream.hpp>
#include <assimp/DefaultLogger.hpp>

#include <filesystem>

namespace fs = std::filesystem;

#define CACHE_FORMAT "assbin"

struct assimp_logger : public Assimp::LogStream
{
	virtual void write(const char* message) override
	{
		std::cout << message << '\n';
	}
};

const aiScene* loadAssimpSceneFile(const std::string& filepathRaw, Assimp::Importer& importer)
{
#if 0
	if (Assimp::DefaultLogger::isNullLogger())
	{
		Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
		Assimp::DefaultLogger::get()->attachStream(new assimp_logger, Assimp::Logger::Err | Assimp::Logger::Warn);
	}
#endif

	int removeFlags = importer.GetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS);

	fs::path filepath = filepathRaw;
	fs::path extension = filepath.extension();

	fs::path cachedFilename = filepath;
	cachedFilename.replace_extension(".cache." + std::to_string(removeFlags) + CACHE_FORMAT);

	fs::path cacheFilepath = L"asset_cache" / cachedFilename;

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
		if (!fs::exists(filepathRaw))
		{
			std::cerr << "Could not find file '" << filepathRaw << "'.\n";
			return 0;
		}

		std::cout << "Preprocessing asset '" << filepathRaw << "' for faster loading next time.";
#ifdef _DEBUG
		std::cout << " Consider running in a release build the first time.";
#endif
		std::cout << '\n';

		//importer.SetPropertyInteger(AI_CONFIG_PP_SLM_VERTEX_LIMIT, UINT16_MAX); // So that we can use 16 bit indices.

		uint32 importFlags = aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_OptimizeGraph | aiProcess_FlipUVs;
		uint32 exportFlags = 0;

		if (removeFlags != -1)
		{
			if (removeFlags & aiComponent_TEXCOORDS)
			{
				importFlags &= ~aiProcess_GenUVCoords;
			}
			if (removeFlags & aiComponent_NORMALS)
			{
				importFlags &= ~aiProcess_GenSmoothNormals;
			}
			if (removeFlags & aiComponent_TANGENTS_AND_BITANGENTS)
			{
				importFlags &= ~aiProcess_CalcTangentSpace;
			}
			importFlags |= aiProcess_RemoveComponent;
		}
		else
		{
			importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.f);
			importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
		}

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

static ref<dx_texture> loadAssimpTexture(const aiScene* scene, const std::string& sceneFilepath, const std::string& name, uint32 flags = texture_load_flags_default)
{
	ref<dx_texture> texture = 0;

	if (!name.empty())
	{
		if (name[0] == '*')
		{
			uint32 index = std::stoi(name.c_str() + 1);

			if (index < scene->mNumTextures)
			{
				const aiTexture* assimpTexture = scene->mTextures[index];

				image_format imageFormat;
				if (assimpTexture->CheckFormat("dds"))
				{
					imageFormat = image_format_dds;
				}
				else if (assimpTexture->CheckFormat("hdr"))
				{
					imageFormat = image_format_hdr;
				}
				else if (assimpTexture->CheckFormat("tga"))
				{
					imageFormat = image_format_tga;
				}
				else if (assimpTexture->CheckFormat("png") || assimpTexture->CheckFormat("jpg"))
				{
					imageFormat = image_format_wic;
				}
				else
				{
					std::cerr << "Unknown format hint '" << assimpTexture->achFormatHint << "' in Assimp texture.\n";
					return 0;
				}

				std::string cacheFilepath = sceneFilepath + "_texture" + std::to_string(index);

				texture = loadTextureFromMemory(assimpTexture->pcData, assimpTexture->mWidth * ((assimpTexture->mHeight == 0) ? 1 : assimpTexture->mHeight), imageFormat, cacheFilepath, flags);
			}
			else
			{
				std::cerr << "Cannot load texture " << index << " from Assimp scene.\n";
			}
		}
		else
		{
			fs::path path = sceneFilepath;
			path = path.parent_path();
			path /= name;
			texture = loadTextureFromFile(path.string(), flags);
		}
	}

	return texture;
}

ref<pbr_material> loadAssimpMaterial(const aiScene* scene, const std::string& sceneFilepath, const aiMaterial* material)
{
	const char* albedoName = "";
	const char* normalName = "";
	const char* roughnessName = "";
	const char* metallicName = "";

	vec4 albedoTint(1.f);
	vec4 emission(0.f, 0.f, 0.f, 1.f);
	float roughnessOverride = 1.f;
	float metallicOverride = 0.f;

	{
		aiString diffuse, normal, roughness, metallic;
		bool hasDiffuse = material->GetTexture(aiTextureType_DIFFUSE, 0, &diffuse) == aiReturn_SUCCESS;
		bool hasNormal = material->GetTexture(aiTextureType_HEIGHT, 0, &normal) == aiReturn_SUCCESS ||
			material->GetTexture(aiTextureType_NORMALS, 0, &normal) == aiReturn_SUCCESS;
		bool hasRoughness = material->GetTexture(aiTextureType_SHININESS, 0, &roughness) == aiReturn_SUCCESS;
		bool hasMetallic = material->GetTexture(aiTextureType_AMBIENT, 0, &metallic) == aiReturn_SUCCESS ||
			material->GetTexture(aiTextureType_METALNESS, 0, &metallic) == aiReturn_SUCCESS;


		aiColor3D aiColor;
		if (material->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == aiReturn_SUCCESS)
		{
			albedoTint.x = aiColor.r;
			albedoTint.y = aiColor.g;
			albedoTint.z = aiColor.b;
		}

		if (material->Get(AI_MATKEY_COLOR_EMISSIVE, aiColor) == aiReturn_SUCCESS)
		{
			emission.x = aiColor.r;
			emission.y = aiColor.g;
			emission.z = aiColor.b;
		}

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
	}

	// TODO: This circumvents the material caching. Do we need the caching?

	ref<dx_texture> albedo = loadAssimpTexture(scene, sceneFilepath, albedoName);
	ref<dx_texture> normal = loadAssimpTexture(scene, sceneFilepath, normalName, texture_load_flags_default | texture_load_flags_noncolor);
	ref<dx_texture> roughness = loadAssimpTexture(scene, sceneFilepath, roughnessName, texture_load_flags_default | texture_load_flags_noncolor);
	ref<dx_texture> metallic = loadAssimpTexture(scene, sceneFilepath, metallicName, texture_load_flags_default | texture_load_flags_noncolor);

	ref<pbr_material> result = make_ref<pbr_material>();

	result->albedo = albedo;
	result->normal = normal;
	result->roughness = roughness;
	result->metallic = metallic;
	result->emission = emission;
	result->albedoTint = albedoTint;
	result->roughnessOverride = roughnessOverride;
	result->metallicOverride = metallicOverride;

	return result;
}
