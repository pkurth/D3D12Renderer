#pragma once

struct file_browser
{
	file_browser();
	void draw();

private:
	struct dir_entry
	{
		fs::path filename;
		bool isDirectory;
		uint32 numChildren;
	};

	fs::path currentPath;
	std::vector<dir_entry> currentPathEntries;

	void changeCurrentPath(const fs::path& path);
	void refresh();
};
