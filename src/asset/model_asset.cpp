#include "pch.h"
#include "model_asset.h"

model_asset loadFBX(const fs::path& path, uint32 flags);

model_asset load3DModelFromFile(const fs::path& path, uint32 meshFlags)
{
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
		[](char c) { return std::tolower(c); });
	if (extension == ".fbx")
	{
		return loadFBX(path, meshFlags);
	}

	return {};
}
