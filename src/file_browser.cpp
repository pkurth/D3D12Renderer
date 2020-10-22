#include "pch.h"
#include "imgui.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

static const ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;



struct file_system_entry
{
	fs::path path;
	std::vector<file_system_entry> children;
	bool isFile;
};

static file_system_entry rootFileEntry;

static void recurseFileSystem(file_system_entry& entry)
{
	for (auto& p : fs::directory_iterator(entry.path))
	{
		file_system_entry newEntry;
		newEntry.path = p.path();
		newEntry.isFile = !p.is_directory();

		if (p.is_directory())
		{
			recurseFileSystem(newEntry);
		}

		entry.children.push_back(std::move(newEntry));
	}
}

static DWORD checkForFileChanges(void* data)
{
	const char* rootDirName = ".";
	fs::path assetPath = (fs::path(rootDirName) / "assets").lexically_normal();

	rootFileEntry.path = assetPath;
	rootFileEntry.isFile = false;
	recurseFileSystem(rootFileEntry);

	uint32 numDirectories = 0;

	enum event_type
	{
		event_file_or_dir_modified,

		event_count,
	};

	DWORD eventName[] =
	{
		FILE_NOTIFY_CHANGE_LAST_WRITE,
	};


	HANDLE directoryHandles[event_count] = {};
	OVERLAPPED overlapped[event_count] = {};
	HANDLE eventsToWaitOn[event_count] = {};

	uint8 buffer[1024] = {};

	for (uint32 i = 0; i < event_count; ++i)
	{
		directoryHandles[i] = CreateFileA(
			rootDirName,
			FILE_LIST_DIRECTORY,
			FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			NULL);

		if (directoryHandles[i] == INVALID_HANDLE_VALUE)
		{
			fprintf(stderr, "Monitor directory failed.\n");
			return 1;
		}

		overlapped[i].hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		ResetEvent(overlapped[i].hEvent);

		DWORD error = ReadDirectoryChangesW(directoryHandles[i],
			buffer, sizeof(buffer), TRUE,
			eventName[i],
			NULL, &overlapped[i], NULL);

		eventsToWaitOn[i] = overlapped[i].hEvent;
		
	}
		

	while (true)
	{
		DWORD result = WaitForMultipleObjects(event_count, eventsToWaitOn, FALSE, INFINITE);

		event_type eventType = (event_type)(result - WAIT_OBJECT_0);

		DWORD dw;
		if (!GetOverlappedResult(directoryHandles[eventType], &overlapped[eventType], &dw, FALSE) || dw == 0)
		{
			fprintf(stderr, "Get overlapped result failed.\n");
			return 1;
		}

		FILE_NOTIFY_INFORMATION* filenotify;

		DWORD offset = 0;

		do
		{
			filenotify = (FILE_NOTIFY_INFORMATION*)(&buffer[offset]);

			if (filenotify->Action == FILE_ACTION_MODIFIED)
			{
				char filename[MAX_PATH];
				int ret = WideCharToMultiByte(CP_ACP, 0, filenotify->FileName,
					filenotify->FileNameLength / sizeof(WCHAR),
					filename, MAX_PATH, NULL, NULL);

				filename[filenotify->FileNameLength / sizeof(WCHAR)] = 0;

				fs::path changedPath = (rootDirName / fs::path(filename)).lexically_normal();

				auto [rootEnd, nothing] = std::mismatch(assetPath.begin(), assetPath.end(), changedPath.begin());

				if (rootEnd == assetPath.end())
				{
					// This is inside the asset directory.
					std::cout << (fs::is_directory(changedPath) ? "Directory " : "File ") << changedPath << " was modified" << std::endl;

					/*file_system_entry* entry = &rootFileEntry;
					auto it = changedPath.begin();
					
					while (*it == entry->path)
					{

					}
					
					++it;*/



				}

			}

			offset += filenotify->NextEntryOffset;

		} while (filenotify->NextEntryOffset != 0);


		if (!ResetEvent(overlapped[eventType].hEvent))
		{
			fprintf(stderr, "Reset event failed.\n");
		}

		DWORD error = ReadDirectoryChangesW(directoryHandles[eventType],
			buffer, sizeof(buffer), TRUE,
			eventName[eventType],
			NULL, &overlapped[eventType], NULL);

		if (error == 0)
		{
			fprintf(stderr, "Read directory failed.\n");
		}
		
	}

	return 0;
}


static void recurseCachedFileEntries(file_system_entry& e)
{
	if (e.isFile)
	{
		ImGui::Text(e.path.stem().string().c_str());
	}
	else
	{
		if (ImGui::TreeNodeEx(e.path.stem().string().c_str(), treeFlags))
		{
			for (file_system_entry& c : e.children)
			{
				recurseCachedFileEntries(c);
			}

			ImGui::TreePop();
		}
	}
}

void drawFileBrowser()
{
	static HANDLE checkForFileChangesThread = CreateThread(0, 0, checkForFileChanges, 0, 0, 0);


	ImGui::Begin("Assets");

	ImGui::Begin("Directories");

	recurseCachedFileEntries(rootFileEntry);


	ImGui::End();
	ImGui::End();
}
