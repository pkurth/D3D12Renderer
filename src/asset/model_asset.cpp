#include "pch.h"
#include "model_asset.h"
#include "core/log.h"

model_asset loadFBX(const fs::path& path, uint32 flags);
model_asset loadOBJ(const fs::path& path, uint32 flags);

model_asset loadBIN(const fs::path& path);
void writeBIN(const model_asset& asset, const fs::path& path);

model_asset load3DModelFromFile(const fs::path& path, uint32 meshFlags)
{
	if (!fs::exists(path))
	{
		LOG_WARNING("Could not find file '%ws'", path.c_str());
		std::cerr << "Could not find file '" << path << "'.\n";
		return {};
	}


	std::string extension = path.extension().string();

	fs::path cachedFilename = path;
	cachedFilename.replace_extension("." + std::to_string(meshFlags) + ".cache.bin");
	fs::path cacheFilepath = L"asset_cache" / cachedFilename;

	if (fs::exists(cacheFilepath))
	{
		auto lastCacheWriteTime = fs::last_write_time(cacheFilepath);
		auto lastOriginalWriteTime = fs::last_write_time(path);

		if (lastCacheWriteTime > lastOriginalWriteTime)
		{
			return loadBIN(cacheFilepath);
		}
	}


	LOG_MESSAGE("Preprocessing asset '%ws' for faster loading next time", path.c_str());
	std::cout << "Preprocessing asset '" << path << "' for faster loading next time.";
#ifdef _DEBUG
	std::cout << " Consider running in a release build the first time.";
#endif
	std::cout << '\n';


	model_asset result;

	std::transform(extension.begin(), extension.end(), extension.begin(),
		[](char c) { return std::tolower(c); });
	if (extension == ".fbx")
	{
		result = loadFBX(path, meshFlags);
	}
	else if (extension == ".obj")
	{
		result = loadOBJ(path, meshFlags);
	}

	fs::create_directories(cacheFilepath.parent_path());
	writeBIN(result, cacheFilepath);

	return result;
}

bool isMeshExtension(const fs::path& extension)
{
	return extension == ".fbx" || extension == ".obj" || extension == ".bin";
}

bool isMeshExtension(const std::string& extension)
{
	return extension == ".fbx" || extension == ".obj" || extension == ".bin";
}
