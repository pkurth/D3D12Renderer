#pragma once

#include <functional>

enum file_system_change
{
	file_system_change_none,
	file_system_change_add,
	file_system_change_delete,
	file_system_change_modify,
};

typedef std::function<void(file_system_change, const fs::path&)> file_system_observer;

bool observeDirectory(const fs::path& directory, const file_system_observer& callback, bool watchSubDirectories = true);
