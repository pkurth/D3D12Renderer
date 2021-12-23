#pragma once

#include <functional>

enum file_system_change
{
	file_system_change_none,
	file_system_change_add,
	file_system_change_delete,
	file_system_change_modify,
	file_system_change_rename,
};

struct file_system_event
{
	file_system_change change;
	fs::path path;
	fs::path oldPath; // Only set in case of a rename event.
};

typedef std::function<void(const file_system_event&)> file_system_observer;

bool observeDirectory(const fs::path& directory, const file_system_observer& callback, bool watchSubDirectories = true);
