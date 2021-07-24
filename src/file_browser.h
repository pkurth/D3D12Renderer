#pragma once

struct file_browser
{
	file_browser();
	void draw();

	enum dir_entry_type
	{
		dir_entry_type_unknown,

		dir_entry_type_directory,
		dir_entry_type_empty_directory,
		dir_entry_type_mesh,
		dir_entry_type_image,
		dir_entry_type_font,
	};

private:
	struct dir_entry
	{
		fs::path filename;
		dir_entry_type type;
	};

	fs::path currentPath;
	std::vector<dir_entry> currentPathEntries;

	void changeCurrentPath(const fs::path& path);
	void refresh();
};
