#include "pch.h"
#include "asset_editor_panel.h"
#include "core/imgui.h"
#include "geometry/mesh.h"
#include "animation/animation.h"
#include "editor_icons.h"

void asset_editor_panel::beginFrame()
{
	windowOpen = windowOpenInternal;

	if (windowOpen)
	{
		ImGui::SetNextWindowSize(ImVec2(1280, 800), ImGuiCond_FirstUseEver);
		if (ImGui::Begin(title, &windowOpenInternal, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			if (ImGui::BeginTable("##table", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
			{
				ImGui::TableSetupColumn("ViewportColumn");
				ImGui::TableSetupColumn("SettingsColumn", ImGuiTableColumnFlags_WidthFixed, 300);

				ImVec2 contentSize;

				if (ImGui::TableNextColumn())
				{
					contentSize = ImGui::GetContentRegionAvail();
					ref<dx_texture> rendering = getRendering();
					if (rendering)
					{
						ImVec2 minCorner = ImGui::GetCursorPos();
						ImGui::Image(rendering, contentSize);

						ImGui::SetCursorPos(ImVec2(minCorner.x + 4.5f, minCorner.y + 4.5f));
						ImGui::Dummy(ImVec2(contentSize.x - 9.f, contentSize.y - 9.f));

						if (dragDropTarget && ImGui::BeginDragDropTarget())
						{
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(dragDropTarget)) { setDragDropData(payload->Data, payload->DataSize); }
							ImGui::EndDragDropTarget();
						}
					}
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

void asset_editor_panel::open()
{
	windowOpenInternal = true;
}

void asset_editor_panel::close()
{
	windowOpenInternal = false;
}


mesh_editor_panel::mesh_editor_panel()
{
	title = EDITOR_ICON_MESH "  Mesh editor";
	dragDropTarget = EDITOR_ICON_MESH;

	renderer_spec spec = { false, false, false, false, false, false };

	renderer.initialize(color_depth_8, 1280, 800, spec);

	camera.initializeIngame(vec3(0.f, 0.f, 25.f), quat::identity, deg2rad(70.f), 0.01f);
}

void mesh_editor_panel::edit(uint32 renderWidth, uint32 renderHeight)
{
	renderer.beginFrame(renderWidth, renderHeight);
	camera.setViewport(renderWidth, renderHeight);
	camera.updateMatrices();

	renderPass.reset();


	if (this->mesh)
	{
		const dx_mesh& mesh = this->mesh->mesh;

		for (auto& sm : this->mesh->submeshes)
		{
			submesh_info submesh = sm.info;
			const ref<pbr_material>& material = sm.material;

			//renderPass.renderStaticObject(mat4::identity, mesh.vertexBuffer, mesh.indexBuffer, submesh, material);
		}
	}



	renderer.submitRenderPass(&renderPass);
	renderer.setCamera(camera);


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

void mesh_editor_panel::endFrame()
{
	if (isOpen())
	{
		renderer.endFrame(0);
	}
}

ref<dx_texture> mesh_editor_panel::getRendering()
{
	return renderer.frameResult;
}

void mesh_editor_panel::setDragDropData(void* data, uint32 size)
{
	const char* filename = (const char*)data;

	fs::path path = filename;
	fs::path relative = fs::relative(path, fs::current_path());

	this->mesh = loadMeshFromFile(relative.string());
}

