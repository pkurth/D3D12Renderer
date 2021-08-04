#include "pch.h"
#include "asset_editor_panel.h"
#include "core/imgui.h"
#include "geometry/mesh.h"


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
					ref<dx_texture> rendering = getRendering();
					if (rendering)
					{
						ImGui::Image(rendering, contentSize);
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


mesh_editor_panel::mesh_editor_panel()
{
#if 0
	title = "Mesh editor";
	renderer.initialize(1280, 800);

	camera.initializeIngame(vec3(0.f, 0.f, 25.f), quat::identity, deg2rad(70.f), 0.01f);

	environment = createEnvironment("assets/sky/sunset_in_the_chalk_quarry_4k.hdr");

	sun.direction = normalize(vec3(-0.6f, -1.f, -0.3f));
	sun.color = vec3(1.f, 0.93f, 0.76f);
	sun.intensity = 50.f;
	sun.numShadowCascades = 0;
#endif
}

void mesh_editor_panel::setAsset(const fs::path& path)
{
#if 0
	open = true;
	asset = loadMeshFromFile(path.u8string(), false, false);
#endif
}

void mesh_editor_panel::edit(uint32 renderWidth, uint32 renderHeight)
{
#if 0
	renderer.beginFrame(renderWidth, renderHeight);
	camera.setViewport(renderWidth, renderHeight);
	camera.updateMatrices();

	opaqueRenderPass.reset();

	if (asset)
	{
		const dx_mesh& mesh = asset->mesh;

		for (auto& sm : asset->submeshes)
		{
			submesh_info submesh = sm.info;
			const ref<pbr_material>& material = sm.material;

			opaqueRenderPass.renderStaticObject(mesh.vertexBuffer, mesh.indexBuffer, submesh, material, mat4::identity, 0, false);
		}
	}

	renderer.submitRenderPass(&opaqueRenderPass);
	renderer.setCamera(camera);
	renderer.setEnvironment(environment);
	renderer.setSun(sun);

	renderer.endFrame();

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
#endif
}

ref<dx_texture> mesh_editor_panel::getRendering()
{
	//return renderer.frameResult;
	return 0;
}

