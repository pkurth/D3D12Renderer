#include "pch.h"
#include "asset_editor_window.h"
#include "imgui.h"


void asset_editor_window::draw()
{
	if (open)
	{
		ImGui::SetNextWindowSize(ImVec2(1280, 800), ImGuiCond_FirstUseEver);
		if (ImGui::Begin(title, &open, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			if (ImGui::BeginTable("##table", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
			{
				ImGui::TableSetupColumn("ViewportColumn");
				ImGui::TableSetupColumn("SettingsColumn", ImGuiTableColumnFlags_WidthFixed, 300);

				ImVec2 contentSize;

				if (ImGui::TableNextColumn())
				{
					contentSize = ImGui::GetContentRegionAvail();
					ImGui::Button("Rendering will go here", contentSize);
					//ImGui::Image(getRendering(), contentSize);
				}

				if (ImGui::TableNextColumn())
				{
					edit((uint32)contentSize.x, (uint32)contentSize.y);
				}

				ImGui::EndTable();
			}
		}
		ImGui::End();
	}
}


mesh_editor_window::mesh_editor_window()
{
	title = "Mesh editor";
}

void mesh_editor_window::setAsset(const fs::path& path)
{
	open = true;
}

void mesh_editor_window::edit(uint32 renderWidth, uint32 renderHeight)
{
	if (ImGui::BeginTabBar("Tabs"))
	{
		if (ImGui::BeginTabItem("Geometry"))
		{
			if (ImGui::BeginChild("GeometrySettings"))
			{
				ImGui::Text("Geometry settings will go here");
			}
			ImGui::EndChild();

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Animations"))
		{
			if (ImGui::BeginChild("AnimationSettings"))
			{
				ImGui::Text("Animation settings will go here");
			}
			ImGui::EndChild();

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}

ref<dx_texture> mesh_editor_window::getRendering()
{
	return 0;
}

