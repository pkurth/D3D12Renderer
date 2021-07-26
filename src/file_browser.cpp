#include "pch.h"
#include "file_browser.h"
#include "imgui.h"
#include "assimp.h"
#include "dx_texture.h"

#include <shellapi.h>
#include <fontawesome/IconsFontAwesome5.h>

file_browser::file_browser()
{
	changeCurrentPath("assets");
}

static const char* getTypeIcon(file_browser::dir_entry_type type)
{
	if (type == file_browser::dir_entry_type_directory) return ICON_FA_FOLDER_OPEN;
	if (type == file_browser::dir_entry_type_empty_directory) return ICON_FA_FOLDER;
	if (type == file_browser::dir_entry_type_mesh) return ICON_FA_CUBE;
	if (type == file_browser::dir_entry_type_image) return ICON_FA_FILE_IMAGE;
	if (type == file_browser::dir_entry_type_font) return ICON_FA_FONT;

	return ICON_FA_FILE;
}

static bool isDirectory(file_browser::dir_entry_type type)
{
	return type == file_browser::dir_entry_type_directory || type == file_browser::dir_entry_type_empty_directory;
}

static bool isFile(file_browser::dir_entry_type type)
{
	return !isDirectory(type);
}

void file_browser::draw(mesh_editor_window& meshEditorWindow)
{
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
				ImGui::PushID(&p);

				if (ImGui::TableNextColumn())
				{
					std::string filename = p.filename.u8string();

					char buffer[256];
					snprintf(buffer, sizeof(buffer), "%s  %s", getTypeIcon(p.type), filename.c_str());
					ImGui::SelectableWrapped(buffer, entryWidth, false);

					// Tooltip and directory navigation.
					if (ImGui::IsItemHovered())
					{
						if (isDirectory(p.type) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
						{
							changeCurrentPath(currentPath / p.filename);
							ImGui::PopID();
							break;
						}
						else if (p.type == dir_entry_type_mesh)
						{
							ImGui::SetTooltip("Drag&drop into scene to instantiate.");
						}
					}

					// Context menu.
					if (ImGui::BeginPopupContextItem(0, ImGuiPopupFlags_MouseButtonRight))
					{
						if (p.type == dir_entry_type_mesh && ImGui::MenuItem("Edit"))
						{
							fs::path fullPath = currentPath / p.filename;
							meshEditorWindow.setAsset(fullPath);
						}

						if (ImGui::MenuItem("Reveal in Windows Explorer"))
						{
							fs::path fullPath = currentPath / p.filename;
							ShellExecuteW(0, 0, L"explorer.exe", (L"/select," + fullPath.wstring()).c_str(), 0, SW_SHOWNORMAL);
						}

						if (isFile(p.type) && ImGui::MenuItem("Open in default program"))
						{
							fs::path fullPath = currentPath / p.filename;
							ShellExecuteW(0, 0, fullPath.c_str(), 0, 0, SW_SHOWNORMAL);
						}
						ImGui::EndPopup();
					}

					// Drag&drop.
					if (p.type == dir_entry_type_mesh)
					{
						if (ImGui::BeginDragDropSource())
						{
							fs::path fullPath = currentPath / p.filename;
							std::string str = fullPath.string();
							ImGui::SetDragDropPayload("content_browser_file", str.c_str(), str.length() + 1, ImGuiCond_Once);
							ImGui::Text("Drop into scene to instantiate.");
							ImGui::EndDragDropSource();
						}
					}
				}

				ImGui::PopID();
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
		dir_entry_type type = dir_entry_type_unknown;

		if (fs::is_directory(p.path()))
		{
			uint32 numChildren = (uint32)(std::distance(std::filesystem::directory_iterator{ p.path() }, std::filesystem::directory_iterator{}));
			type = numChildren ? dir_entry_type_directory : dir_entry_type_empty_directory;
		}
		else
		{
			fs::path extension = p.path().extension();
			if (isMeshExtension(extension))
			{
				type = dir_entry_type_mesh;
			}
			else if (isImageExtension(extension))
			{
				type = dir_entry_type_image;
			}
			else if (extension == ".ttf")
			{
				type = dir_entry_type_font;
			}
		}
		
		currentPathEntries.push_back({ p.path().filename(), type });
	}

	std::sort(currentPathEntries.begin(), currentPathEntries.end(), [](const dir_entry& a, const dir_entry& b)
	{
		if (isDirectory(a.type) == isDirectory(b.type))
		{
			return a.filename < b.filename;
		}
		return isDirectory(a.type);
	});
}

void file_browser::changeCurrentPath(const fs::path& path)
{
	currentPath = path;
	refresh();
}

