#include "pch.h"
#include "file_registry.h"
#include "file_system.h"
#include "log.h"
#include "hash.h"
#include "random.h"
#include "yaml.h"


static random_number_generator rng = time(0);

struct unique_id
{
	unique_id() : value(rng.randomUint64()) {}
	unique_id(uint64 value) : value(value) {}

	uint64 value;
};

typedef std::unordered_map<fs::path, unique_id> file_registry;

static file_registry registry;
static std::mutex mutex;
static const fs::path registryPath = fs::path("assets/registry/reg.yaml").lexically_normal();


static file_registry loadRegistryFromDisk()
{
	file_registry loadedRegistry;

	std::ifstream stream(registryPath);
	YAML::Node n = YAML::Load(stream);

	for (auto entryNode : n)
	{
		fs::path path = entryNode["Path"].as<fs::path>();
		uint64 id = entryNode["UUID"].as<uint64>();

		loadedRegistry[path] = id;
	}

	return loadedRegistry;
}

static void writeRegistryToDisk(const file_registry& registry)
{
	YAML::Emitter out;
	out << YAML::BeginSeq;

	for (const auto& entry : registry)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Path" << YAML::Value << entry.first;
		out << YAML::Key << "UUID" << YAML::Value << entry.second.value;
		out << YAML::EndMap;
	}

	out << YAML::EndSeq;

	fs::create_directories(registryPath.parent_path());
	std::ofstream fout(registryPath);
	fout << out.c_str();
}

static void readDirectory(const fs::path& path, const file_registry& loadedRegistry)
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

			if (it != loadedRegistry.end())
			{
				registry.insert({ path, it->second });
			}
			else
			{
				registry.insert({ path, unique_id() });
			}
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
				assert(registry.find(e.path) == registry.end());
				registry.insert({ e.path, unique_id() });
			} break;
			case file_system_change_delete:
			{
				LOG_MESSAGE("Asset '%ws' deleted", e.path.c_str());
				assert(registry.find(e.path) != registry.end());
				registry.erase(e.path);
			} break;
			case file_system_change_modify:
			{
				LOG_MESSAGE("Asset '%ws' modified", e.path.c_str());
			} break;
			case file_system_change_rename:
			{
				LOG_MESSAGE("Asset renamed from '%ws' to '%ws'", e.oldPath.c_str(), e.path.c_str());
				assert(registry.find(e.oldPath) != registry.end());
				assert(registry.find(e.path) == registry.end());
				unique_id id = registry[e.oldPath];
				registry.erase(e.oldPath);
				registry.insert({ e.path, id });
			} break;
		}
		mutex.unlock();

		LOG_MESSAGE("Rewriting file registry");
		writeRegistryToDisk(registry);
	}
}

void initializeFileRegistry()
{
	auto loadedRegistry = loadRegistryFromDisk();
	readDirectory("assets", loadedRegistry);
	writeRegistryToDisk(registry);

	observeDirectory("assets", handleAssetChange);
}
