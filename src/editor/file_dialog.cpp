#include "pch.h"
#include "file_dialog.h"

#include <commdlg.h>
#include <shlobj_core.h>

static void createFilter(char* filter, const std::string& fileDescription, const std::string& extension)
{
	int offset = sprintf(filter, "%s (*.%s)", fileDescription.c_str(), extension.c_str());
	filter[offset] = 0;
	++offset;
	offset += sprintf(filter + offset, "*.%s", extension.c_str());
}

static bool endsWith(std::string const& value, std::string const& ending)
{
	if (ending.size() > value.size()) return false;
	return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

std::string openFileDialog(const std::string& fileDescription, const std::string& extension)
{
	char filter[128];
	createFilter(filter, fileDescription, extension);

	OPENFILENAMEA ofn = {};
	char szFile[MAX_PATH] = "";

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GetActiveWindow();
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = 1;
	ofn.lpstrInitialDir = ".";
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
	if (GetOpenFileNameA(&ofn) == TRUE)
	{
		std::string result = ofn.lpstrFile;
		if (!endsWith(result, "." + extension))
		{
			result += "." + extension;
		}
		return result;
	}
	return std::string();
}

std::string saveFileDialog(const std::string& fileDescription, const std::string& extension)
{
	char filter[128];
	createFilter(filter, fileDescription, extension);

	OPENFILENAMEA ofn = {};
	char szFile[MAX_PATH] = "";

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GetActiveWindow();
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = 1;
	ofn.lpstrInitialDir = ".";
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
	if (GetSaveFileNameA(&ofn) == TRUE)
	{
		std::string result = ofn.lpstrFile;
		if (!endsWith(result, "." + extension))
		{
			result += "." + extension;
		}
		return result;
	}
	return std::string();
}

#pragma warning( disable : 4244 )

std::string directoryDialog()
{
	IFileDialog* pFileOpen;

	// Create the FileOpenDialog object.
	auto hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
		IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

	pFileOpen->SetOptions(FOS_PICKFOLDERS | FOS_NOCHANGEDIR | FOS_PATHMUSTEXIST);

	std::string result;

	if (SUCCEEDED(hr))
	{
		// Show the Open dialog box.
		hr = pFileOpen->Show(NULL);

		// Get the file name from the dialog box.
		if (SUCCEEDED(hr))
		{
			IShellItem* pItem;
			hr = pFileOpen->GetResult(&pItem);
			if (SUCCEEDED(hr))
			{
				PWSTR pszFilePath;
				hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

				// Display the file name to the user.
				if (SUCCEEDED(hr))
				{
					std::wstring wresult(pszFilePath);
					result = std::string(wresult.begin(), wresult.end());
					CoTaskMemFree(pszFilePath);
				}
				pItem->Release();
			}
		}
		pFileOpen->Release();
	}

	return result;
}
