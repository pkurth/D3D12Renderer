#include "pch.h"
#include "asset_editor_panel.h"
#include "imgui.h"


void asset_editor_panel::draw()
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


mesh_editor_panel::mesh_editor_panel()
{
	title = "Mesh editor";
}

void mesh_editor_panel::setAsset(const fs::path& path)
{
	open = true;
}

void mesh_editor_panel::edit(uint32 renderWidth, uint32 renderHeight)
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

ref<dx_texture> mesh_editor_panel::getRendering()
{
	return 0;
}

