#include "pch.h"
#include "file_registry.h"
#include "file_system.h"
#include "log.h"
#include "yaml.h"



typedef std::unordered_map<fs::path, asset_handle> path_to_handle;
typedef std::unordered_map<asset_handle, fs::path> handle_to_path;

static path_to_handle pathToHandle;
static handle_to_path handleToPath;

static std::mutex mutex;
static const fs::path registryPath = fs::path(L"assets/registry/reg.yaml").lexically_normal();


static path_to_handle loadRegistryFromDisk()
{
	path_to_handle loadedRegistry;

	std::ifstream stream(registryPath);
	YAML::Node n = YAML::Load(stream);

	for (auto entryNode : n)
	{
		asset_handle handle = 0;
		fs::path path;

		YAML_LOAD(entryNode, handle, "Handle");
		YAML_LOAD(entryNode, path, "Path");

		if (handle)
		{
			loadedRegistry[path] = handle;
		}
	}

	return loadedRegistry;
}

static void writeRegistryToDisk()
{
	YAML::Emitter out;
	out << YAML::BeginSeq;

	for (const auto& [path, handle] : pathToHandle)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Handle" << YAML::Value << handle;
		out << YAML::Key << "Path" << YAML::Value << path;
		out << YAML::EndMap;
	}

	out << YAML::EndSeq;

	fs::create_directories(registryPath.parent_path());
	std::ofstream fout(registryPath);
	fout << out.c_str();
}

static void readDirectory(const fs::path& path, const path_to_handle& loadedRegistry)
{
	for (const auto& dirEntry : fs::directory_iterator(path))
	{
		const auto& path = dirEntry.path();
		if (dirEntry.is_directory())
		{
			readDirectory(path, loadedRegistry);
		}
		else
		{
			auto it = loadedRegistry.find(path);

			// If already known, use the handle, otherwise generate one.
			asset_handle handle = (it != loadedRegistry.end()) ? it->second : asset_handle::generate();

			pathToHandle.insert({ path, handle });
			handleToPath.insert({ handle, path });
		}
	}
}

static void handleAssetChange(const file_system_event& e)
{
	if (!fs::is_directory(e.path) && e.path != registryPath)
	{
		mutex.lock();
		switch (e.change)
		{
			case file_system_change_add:
			{
				LOG_MESSAGE("Asset '%ws' added", e.path.c_str());

				assert(pathToHandle.find(e.path) == pathToHandle.end());

				asset_handle handle = asset_handle::generate();
				pathToHandle.insert({ e.path, handle });
				handleToPath.insert({ handle, e.path });
			} break;

			case file_system_change_delete:
			{
				LOG_MESSAGE("Asset '%ws' deleted", e.path.c_str());

				auto it = pathToHandle.find(e.path);

				assert(it != pathToHandle.end());

				asset_handle handle = it->second;
				pathToHandle.erase(it);
				handleToPath.erase(handle);
			} break;

			case file_system_change_modify:
			{
				LOG_MESSAGE("Asset '%ws' modified", e.path.c_str());
			} break;

			case file_system_change_rename:
			{
				LOG_MESSAGE("Asset renamed from '%ws' to '%ws'", e.oldPath.c_str(), e.path.c_str());

				auto oldIt = pathToHandle.find(e.oldPath);

				assert(oldIt != pathToHandle.end()); // Old path exists.
				assert(pathToHandle.find(e.path) == pathToHandle.end()); // New path does not exist.

				asset_handle handle = oldIt->second;
				pathToHandle.erase(oldIt);
				pathToHandle.insert({ e.path, handle });
				handleToPath[handle] = e.path; // Replace.
			} break;
		}
		mutex.unlock();

		// During runtime the registry is only written to in this function, so no need to protect the read with mutex.
		LOG_MESSAGE("Rewriting file registry");
		writeRegistryToDisk();
	}
}

asset_handle getAssetHandleFromPath(const fs::path& path)
{
	const std::lock_guard<std::mutex> lock(mutex);

	auto it = pathToHandle.find(path);
	if (it == pathToHandle.end())
	{
		return {};
	}
	return it->second;
}

fs::path getPathFromAssetHandle(asset_handle handle)
{
	const std::lock_guard<std::mutex> lock(mutex);

	auto it = handleToPath.find(handle);
	if (it == handleToPath.end())
	{
		return {};
	}
	return it->second;
}

void initializeFileRegistry()
{
	auto loadedRegistry = loadRegistryFromDisk();
	readDirectory(L"assets", loadedRegistry);
	writeRegistryToDisk();

	observeDirectory(L"assets", handleAssetChange);
}
