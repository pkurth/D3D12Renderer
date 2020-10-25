#include "pch.h"
#include "imgui.h"
#include "threading.h"

#include <filesystem>
#include <iostream>
#include <shellapi.h>

namespace fs = std::filesystem;

static const ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;



struct file_system_entry
{
	fs::path path;
	std::string name;
	std::vector<file_system_entry> children;
	bool isFile;
};

static file_system_entry rootFileEntry;
static thread_mutex fileSystemMutex;

static const char* rootDirName = ".";
static fs::path assetPath = (fs::path(rootDirName) / "assets").lexically_normal();

static void recurseFileSystem(file_system_entry& entry)
{
	for (auto& p : fs::directory_iterator(entry.path))
	{
		file_system_entry newEntry;
		newEntry.path = p.path();
		newEntry.name = newEntry.path.filename().string();
		newEntry.isFile = !p.is_directory();

		if (p.is_directory())
		{
			recurseFileSystem(newEntry);
		}

		entry.children.push_back(std::move(newEntry));

		std::sort(entry.children.begin(), entry.children.end(), [](file_system_entry& a, file_system_entry& b)
		{
			if (a.isFile && !b.isFile) { return false; }
			if (b.isFile && !a.isFile) { return true; }
			return a.name < b.name;
		});
	}
}

static file_system_entry& findFileSystemEntry(file_system_entry& parent, fs::path::iterator it, fs::path::iterator end)
{
	fs::path cur = *it;
	for (auto& c : parent.children)
	{
		if (c.name == cur)
		{
			auto next = ++it;
			if (next == end)
			{
				return c;
			}
			return findFileSystemEntry(c, next, end);
		}
	}
	return rootFileEntry;
}

static DWORD checkForFileChanges(void*)
{
	fileSystemMutex = createMutex();


	rootFileEntry.path = assetPath;
	rootFileEntry.name = rootFileEntry.path.filename().string();
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

	fs::path lastChangedPath = "";
	fs::file_time_type lastChangedPathTimeStamp;
	
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
				auto changedPathWriteTime = fs::last_write_time(changedPath);

				// The filesystem usually sends multiple notifications for changed files, since the file is first written, then metadata is changed etc.
				// This check prevents these notifications if they are too close together in time.
				// This is a pretty crude fix. In this setup files should not change at the same time, since we only ever track one file.
				if (changedPath == lastChangedPath 
					&& std::chrono::duration_cast<std::chrono::milliseconds>(changedPathWriteTime - lastChangedPathTimeStamp).count() < 200)
				{
					lastChangedPath = changedPath;
					lastChangedPathTimeStamp = changedPathWriteTime;
					break;
				}

				auto [rootEnd, nothing] = std::mismatch(assetPath.begin(), assetPath.end(), changedPath.begin());

				if (rootEnd == assetPath.end())
				{
					// This is inside the asset directory.
					bool isFile = !fs::is_directory(changedPath);

					fs::path relativeToAsset = fs::relative(changedPath, assetPath);

					file_system_entry& entryToReload = (relativeToAsset.string() == ".") ? rootFileEntry : findFileSystemEntry(rootFileEntry, relativeToAsset.begin(), relativeToAsset.end());
					assert(entryToReload.isFile == isFile);

					if (!isFile)
					{
						lock(fileSystemMutex);
						entryToReload.children.clear();
						recurseFileSystem(entryToReload);
						unlock(fileSystemMutex);
					}
					else
					{
						std::cout << "File " << changedPath << " changed!" << std::endl;
					}

					lastChangedPath = changedPath;
					lastChangedPathTimeStamp = changedPathWriteTime;
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

static fs::path selectedPath;

static void coloredText(const char* text, float r, float g, float b)
{
	ImGui::TextColored(ImColor(r, g, b, 1.f).Value, text);
}

static bool coloredTreeNode(const char* text, float r, float g, float b)
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImColor(r, g, b, 1.f).Value);
	bool result = ImGui::TreeNodeEx(text, treeFlags);
	ImGui::PopStyleColor();
	return result;
}

static void printCachedFileEntries(file_system_entry& e)
{
	if (e.isFile)
	{
		if (e.path == selectedPath) { coloredText(e.name.c_str(), 0.8f, 0.2f, 0.1f); }
		else { ImGui::Text(e.name.c_str()); }

		if (ImGui::IsItemClicked())
		{
			selectedPath = e.path;
		}
	}
	else
	{
		bool openNode = (e.path == selectedPath) ? coloredTreeNode(e.name.c_str(), 0.8f, 0.2f, 0.1f) : ImGui::TreeNodeEx(e.name.c_str(), treeFlags);

		if (ImGui::IsItemClicked())
		{
			selectedPath = e.path;
		}

		if (openNode)
		{
			for (file_system_entry& c : e.children)
			{
				printCachedFileEntries(c);
			}

			ImGui::TreePop();
		}
	}
}

void drawFileBrowser()
{
	static HANDLE checkForFileChangesThread = CreateThread(0, 0, checkForFileChanges, 0, 0, 0);


	ImGui::Begin("Assets");
	
	if (ImGui::Begin("Directories"))
	{
		lock(fileSystemMutex);
		printCachedFileEntries(rootFileEntry);
		unlock(fileSystemMutex);
	}
	ImGui::End();


	fs::path directoryToShow = selectedPath;
	bool selectedPathIsFile = !fs::is_directory(selectedPath);
	if (selectedPathIsFile)
	{
		directoryToShow = selectedPath.parent_path();
	}
	if (!fs::exists(directoryToShow))
	{
		directoryToShow = assetPath;
	}

	if (!selectedPathIsFile)
	{
		coloredText(directoryToShow.string().c_str(), 0.8f, 0.2f, 0.1f);
	}
	else
	{
		ImGui::Text(directoryToShow.string().c_str());
	}
	ImGui::Separator();

	lock(fileSystemMutex);
	fs::path relativeToAsset = fs::relative(directoryToShow, assetPath);
	file_system_entry& entryToShow = (relativeToAsset.string() == ".") ? rootFileEntry : findFileSystemEntry(rootFileEntry, relativeToAsset.begin(), relativeToAsset.end());

	ImGui::Text("..");
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
	{
		selectedPath = directoryToShow.parent_path();
	}

	float minWidthPerItem = 300.f;
	float widthAvail = ImGui::GetContentRegionAvail().x;
	uint32 columns = max(1, (uint32)floor(widthAvail / minWidthPerItem));


	ImGui::Columns(columns);

	for (uint32 i = 0; i < entryToShow.children.size(); ++i)
	{
		if (i > 0 && i % columns == 0)
		{
			ImGui::Separator();
		}

		auto& c = entryToShow.children[i];

		if (selectedPathIsFile && selectedPath == c.path)
		{
			coloredText(c.name.c_str(), 0.8f, 0.2f, 0.1f);
		}
		else
		{
			if (c.isFile)
			{
				coloredText(c.name.c_str(), 1.f, 1.f, 0.7f);
			}
			else
			{
				ImGui::Text(c.name.c_str());
			}
		}

		if (ImGui::IsItemHovered() && (c.isFile ? ImGui::IsItemClicked() : ImGui::IsMouseDoubleClicked(0)))
		{
			selectedPath = c.path;
		}

		if (c.isFile && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
		{
			ShellExecute(0, 0, c.path.wstring().c_str(), 0, 0, SW_SHOW);
		}

		ImGui::NextColumn();
	}

	ImGui::Columns(1);

	unlock(fileSystemMutex);

	ImGui::End();
}
