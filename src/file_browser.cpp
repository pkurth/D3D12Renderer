#include "pch.h"
#include "file_browser.h"
#include "imgui.h"
#include "assimp.h"

#include <shellapi.h>
#include <fontawesome/IconsFontAwesome5.h>

file_browser::file_browser()
{
	changeCurrentPath("assets");
}

void file_browser::draw()
{
	const char* directoryString = ICON_FA_FOLDER_OPEN "  %s";
	const char* emptyDirectoryString = ICON_FA_FOLDER "  %s";
	const char* fileString = ICON_FA_FILE "  %s";

	if (ImGui::Begin("Assets"))
	{
		if (ImGui::DisableableButton(ICON_FA_LEVEL_UP_ALT, currentPath.has_parent_path())) { changeCurrentPath(currentPath.parent_path()); }
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Up"); }
		ImGui::SameLine();

		if (ImGui::Button(ICON_FA_REDO_ALT)) { refresh(); }
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Refresh"); }
		ImGui::SameLine();
			
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::Text("Current path: ");
		fs::path accPath;
		for (auto p : currentPath)
		{
			accPath = accPath.empty() ? p : (accPath / p);
			ImGui::SameLine(0, 0);
			if (ImGui::SmallButton(p.u8string().c_str()))
			{
				changeCurrentPath(accPath);
				break;
			}
			if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Change to '%s'", accPath.u8string().c_str()); }
			ImGui::SameLine(0, 0);
			ImGui::Text("/");
		}
		ImGui::PopStyleColor();

		ImGui::Separator();

		int width = (int)ImGui::GetContentRegionAvail().x;
		int entryWidth = 256;
		int numColumns = max(1, width / entryWidth);

		if (ImGui::BeginTable("#direntries", numColumns))
		{
			for (auto& p : currentPathEntries)
			{
				if (ImGui::TableNextColumn())
				{
					std::string filename = p.filename.u8string();

					char buffer[256];

					snprintf(buffer, sizeof(buffer), !p.isDirectory ? fileString : p.numChildren ? directoryString : emptyDirectoryString, filename.c_str());

					ImGui::SelectableWrapped(buffer, entryWidth, false);

					if (ImGui::IsItemHovered())
					{
						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
						{
							fs::path fullPath = currentPath / p.filename;
							if (p.isDirectory)
							{
								changeCurrentPath(fullPath);
							}
							else
							{
								ShellExecuteW(0, 0, fullPath.c_str(), 0, 0, SW_SHOWNORMAL);
							}
						}
						else if (isMeshExtension(p.filename))
						{
							ImGui::SetTooltip("Drag&drop into scene to instantiate.");

							if (ImGui::BeginDragDropSource())
							{
								fs::path fullPath = currentPath / p.filename;
								std::string str = fullPath.string();
								ImGui::SetDragDropPayload("content_browser_file", str.c_str(), str.length() + 1);
								ImGui::Text("Drop into scene to instantiate.");
								ImGui::EndDragDropSource();
							}
						}
					}
				}
			}

			ImGui::EndTable();
		}
	}
	ImGui::End();
}

void file_browser::refresh()
{
	currentPathEntries.clear();
	for (auto p : fs::directory_iterator(currentPath))
	{
		bool isDirectory = fs::is_directory(p.path());
		uint32 numChildren = isDirectory ? (uint32)(std::distance(std::filesystem::directory_iterator{ p.path() }, std::filesystem::directory_iterator{})) : 0;
		currentPathEntries.push_back({ p.path().filename(), isDirectory, numChildren });
	}
}

void file_browser::changeCurrentPath(const fs::path& path)
{
	currentPath = path;
	refresh();
}

