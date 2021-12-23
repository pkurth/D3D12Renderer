#include "pch.h"
#include "file_system.h"

struct observe_params
{
	fs::path directory;
	file_system_observer callback;
	bool watchSubDirectories;
};

static DWORD observeDirectoryThread(void* inParams)
{
	observe_params* params = (observe_params*)inParams;

	uint8 buffer[1024] = {};

	HANDLE directoryHandle = CreateFileW(
		params->directory.c_str(),
		GENERIC_READ | FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
		NULL);

	if (directoryHandle == INVALID_HANDLE_VALUE)
	{
		std::cerr << "Monitor directory failed.\n";
		return 1;
	}

	OVERLAPPED overlapped;
	overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	ResetEvent(overlapped.hEvent);

	DWORD eventName = 
		FILE_NOTIFY_CHANGE_LAST_WRITE |
		FILE_NOTIFY_CHANGE_FILE_NAME | 
		FILE_NOTIFY_CHANGE_DIR_NAME | 
		FILE_NOTIFY_CHANGE_SIZE;

	DWORD bytesReturned;

	fs::path lastChangedPath = "";
	fs::file_time_type lastChangedPathTimeStamp;

	while (true)
	{
		DWORD result = ReadDirectoryChangesW(directoryHandle,
			buffer, sizeof(buffer), params->watchSubDirectories,
			eventName,
			&bytesReturned, &overlapped, NULL);

		if (!result)
		{
			std::cerr << "Read directory changes failed\n";
			break;
		}

		WaitForSingleObject(overlapped.hEvent, INFINITE);



		DWORD dw;
		if (!GetOverlappedResult(directoryHandle, &overlapped, &dw, FALSE) || dw == 0)
		{
			std::cerr << "Get overlapped result failed.\n";
			break;
		}

		FILE_NOTIFY_INFORMATION* filenotify;
		DWORD offset = 0;

		fs::path oldPath;

		do
		{
			filenotify = (FILE_NOTIFY_INFORMATION*)(&buffer[offset]);

			file_system_change change = file_system_change_none;

			switch (filenotify->Action)
			{
				case FILE_ACTION_ADDED: { change = file_system_change_add; } break;
				case FILE_ACTION_REMOVED: { change = file_system_change_delete; } break;
				case FILE_ACTION_MODIFIED: { change = file_system_change_modify; } break;
				case FILE_ACTION_RENAMED_OLD_NAME: 
				{
					uint32 filenameLength = filenotify->FileNameLength / sizeof(WCHAR);
					oldPath = (params->directory / std::wstring(filenotify->FileName, filenameLength)).lexically_normal();
				} break;
				case FILE_ACTION_RENAMED_NEW_NAME: { change = file_system_change_rename; } break;
			}

			if (change != file_system_change_none)
			{
				uint32 filenameLength = filenotify->FileNameLength / sizeof(WCHAR);
				fs::path path = (params->directory / std::wstring(filenotify->FileName, filenameLength)).lexically_normal();

				if (change == file_system_change_modify)
				{
					auto writeTime = fs::last_write_time(path);

					// The filesystem usually sends multiple notifications for changed files, since the file is first written, then metadata is changed etc.
					// This check prevents these notifications if they are too close together in time.
					// This is a pretty crude fix. In this setup files should not change at the same time, since we only ever track one file.
					if (path == lastChangedPath
						&& std::chrono::duration_cast<std::chrono::milliseconds>(writeTime - lastChangedPathTimeStamp).count() < 200)
					{
						lastChangedPath = path;
						lastChangedPathTimeStamp = writeTime;
						break;
					}

					lastChangedPath = path;
					lastChangedPathTimeStamp = writeTime;
				}

				file_system_event e;
				e.change = change;
				e.path = std::move(path);
				if (change == file_system_change_rename)
				{
					e.oldPath = std::move(oldPath);
				}

				params->callback(e);
			}

			offset += filenotify->NextEntryOffset;

		} while (filenotify->NextEntryOffset != 0);


		if (!ResetEvent(overlapped.hEvent))
		{
			std::cerr << "Reset event failed.\n";
		}
	}

	CloseHandle(directoryHandle);

	delete params;

	return 0;
}

bool observeDirectory(const fs::path& directory, const file_system_observer& callback, bool watchSubDirectories)
{
	observe_params* params = new observe_params;
	params->directory = directory;
	params->callback = callback;
	params->watchSubDirectories = watchSubDirectories;

	HANDLE handle = CreateThread(0, 0, observeDirectoryThread, params, 0, 0);
	bool result = handle != INVALID_HANDLE_VALUE;
	CloseHandle(handle);

	return result;
}
