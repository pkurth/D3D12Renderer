#include "pch.h"
#include "editor.h"
#include "editor_icons.h"
#include "core/cpu_profiling.h"
#include "core/log.h"
#include "core/file_registry.h"
#include "core/imgui.h"
#include "dx/dx_profiling.h"
#include "scene/components.h"
#include "animation/animation.h"
#include "geometry/mesh.h"
#include "physics/ragdoll.h"
#include "physics/vehicle.h"
#include "scene/serialization_yaml.h"
#include "scene/serialization_binary.h"
#include "audio/audio.h"
#include "rendering/debug_visualization.h"
#include "terrain/terrain.h"
#include "terrain/proc_placement.h"
#include "terrain/grass.h"
#include "terrain/water.h"

#include <fontawesome/list.h>
#include <sstream>



static vec3 getEuler(quat q)
{
	vec3 euler = quatToEuler(q);
	euler.x = rad2deg(angleToZeroToTwoPi(euler.x));
	euler.y = rad2deg(angleToZeroToTwoPi(euler.y));
	euler.z = rad2deg(angleToZeroToTwoPi(euler.z));
	return euler;
}

static quat getQuat(vec3 euler)
{
	euler.x = deg2rad(euler.x);
	euler.y = deg2rad(euler.y);
	euler.z = deg2rad(euler.z);
	return eulerToQuat(euler);
}



template <typename component_t, typename member_t>
struct component_member_undo
{
	component_member_undo(member_t& member, member_t before, scene_entity entity, void (*callback)(component_t&, member_t, void*) = 0, void* userData = 0)
	{
		this->entity = entity;
		this->byteOffset = (uint8*)&member - (uint8*)&entity.getComponent<component_t>();
		this->before = before;
		this->callback = callback;
		this->userData = userData;
	}

	void toggle()
	{
		if (entity.valid())
		{
			component_t& comp = entity.getComponent<component_t>();
			auto& member = *(member_t*)((uint8*)&comp + byteOffset);
			std::swap(member, before);

			if (callback)
			{
				callback(comp, member, userData);
			}
		}
	}

private:
	scene_entity entity;
	uint64 byteOffset;

	member_t before;

	void (*callback)(component_t&, member_t, void*) = 0;
	void* userData = 0;
};

template <typename... component_t>
struct component_undo
{
	component_undo(scene_entity entity, component_t... before)
	{
		this->entity = entity;
		this->before = std::make_tuple(before...);
	}

	void toggle()
	{
		if (entity.valid())
		{
			std::apply([this](auto&... c) { (set(c), ...); }, before);
		}
	}

private:
	template <typename T>
	void set(T& c)
	{
		std::swap(entity.getComponent<T>(), c);
	}

	scene_entity entity;

	std::tuple<component_t...> before;
};

template <typename value_t>
struct settings_undo
{
	settings_undo(value_t& value, value_t before)
		: value(value), before(before) {}

	void toggle() { std::swap(value, before); }

private:
	value_t& value;

	value_t before;
};

struct entity_existence_undo
{
	entity_existence_undo(game_scene& scene, scene_entity entity)
		: scene(scene), entity(entity)
	{
		size = serializeEntityToMemory(entity, buffer, sizeof(buffer));
	}

	void toggle()
	{
		if (entity.valid()) { deleteEntity(); }
		else { restoreEntity(); }
	}

private:
	void deleteEntity()
	{
		size = serializeEntityToMemory(entity, buffer, sizeof(buffer));
		scene.deleteEntity(entity);
	}

	void restoreEntity()
	{
		entity_handle place = entity.handle;
		entity = scene.tryCreateEntityInPlace(entity, "");
		assert(entity.handle == place);

		bool success = deserializeEntityFromMemory(entity, buffer, size);
		assert(success);
	}

	game_scene& scene;
	scene_entity entity;
	uint8 buffer[1024];
	uint64 size;
};



template <typename value_t, typename action_t, typename... args_t>
void scene_editor::undoable(const char* undoLabel, value_t before, value_t& value, 
	args_t... args)
{
	if (ImGui::IsItemActive() && !ImGui::IsItemActiveLastFrame())
	{
		currentUndoBuffer->as<value_t>() = before;
	}

	bool changed = ImGui::IsItemDeactivatedAfterEdit();
	if constexpr (std::is_enum_v<value_t> || std::is_integral_v<value_t>)
	{
		changed |= (!ImGui::IsItemActive() && (before != value));
	}

	if (changed)
	{
		std::stringstream s;
		s << std::setprecision(4) << std::boolalpha;
		s << undoLabel << " from " << currentUndoBuffer->as<value_t>() << " to " << value;
		action_t action(value, currentUndoBuffer->as<value_t>(), std::forward<args_t>(args)...);
		currentUndoStack->pushAction(s.str().c_str(), action);
	}
}

#define UNDOABLE_COMPONENT_SETTING(undoLabel, val, command, ...)					\
	{																				\
		using value_t = std::decay_t<decltype(val)>;								\
		value_t before = val;														\
		command;																	\
		undoable<value_t, component_member_undo<component_t, value_t>>(undoLabel,	\
			before, val, selectedEntity, __VA_ARGS__);								\
	}

#define UNDOABLE_SETTING(undoLabel, val, command)									\
	{																				\
		using value_t = std::decay_t<decltype(val)>;								\
		value_t before = val;														\
		command;																	\
		undoable<value_t, settings_undo<value_t>>(undoLabel,						\
			before, val);															\
	}


void scene_editor::updateSelectedEntityUIRotation()
{
	if (selectedEntity)
	{
		quat rotation = quat::identity;

		if (transform_component* transform = selectedEntity.getComponentIfExists<transform_component>())
		{
			rotation = transform->rotation;
		}
		else if (position_rotation_component* prc = selectedEntity.getComponentIfExists<position_rotation_component>())
		{
			rotation = prc->rotation;
		}

		selectedEntityEulerRotation = getEuler(rotation);
	}
}

void scene_editor::setSelectedEntity(scene_entity entity)
{
	selectedEntity = entity;
	updateSelectedEntityUIRotation();
	selectedColliderEntity = {};
	selectedConstraintEntity = {};
}

void scene_editor::initialize(editor_scene* scene, main_renderer* renderer, editor_panels* editorPanels)
{
	this->scene = scene;
	this->renderer = renderer;
	this->editorPanels = editorPanels;
	cameraController.initialize(&scene->camera);

	systemInfo = getSystemInfo();
}

bool scene_editor::update(const user_input& input, ldr_render_pass* ldrRenderPass, float dt)
{
	CPU_PROFILE_BLOCK("Update editor");

	currentUndoStack = &undoStacks[this->scene->mode > 0];
	currentUndoBuffer = &undoBuffers[this->scene->mode > 0];

	auto selectedEntityBefore = selectedEntity;

	auto& scene = this->scene->getCurrentScene();

	// Clear selected entity, if it became invalid (e.g. if it was deleted).
	if (selectedEntity && !scene.isEntityValid(selectedEntity))
	{
		setSelectedEntity({});
	}

	bool objectChanged = false;
	objectChanged |= handleUserInput(input, ldrRenderPass, dt);
	objectChanged |= drawSceneHierarchy();
	objectChanged |= drawMainMenuBar();
	drawSettings(dt);

	if (objectChanged)
	{
		onObjectMoved();
	}

	// This is triggered on undo.
	if (selectedEntity != selectedEntityBefore)
	{
		setSelectedEntity(selectedEntity);
	}


	if (selectedConstraintEntity)
	{
		if (auto* ref = selectedConstraintEntity.getComponentIfExists<constraint_entity_reference_component>())
		{
			scene_entity entityA = { ref->entityA, scene };
			scene_entity entityB = { ref->entityB, scene };

			const trs& transformA = entityA.getComponent<transform_component>();
			const trs& transformB = entityB.getComponent<transform_component>();

			const vec4 constraintColor(1.f, 1.f, 0.f, 1.f);

			if (distance_constraint* c = selectedConstraintEntity.getComponentIfExists<distance_constraint>())
			{
				vec3 a = transformPosition(transformA, c->localAnchorA);
				vec3 b = transformPosition(transformB, c->localAnchorB);
				vec3 center = 0.5f * (a + b);
				vec3 d = normalize(b - a);
				a -= d * (c->globalLength * 0.5f);
				b += d * (c->globalLength * 0.5f);

				renderLine(a, b, constraintColor, ldrRenderPass, true);
			}
			else if (ball_constraint* c = selectedConstraintEntity.getComponentIfExists<ball_constraint>())
			{

			}
			else if (fixed_constraint* c = selectedConstraintEntity.getComponentIfExists<fixed_constraint>())
			{

			}
			else if (hinge_constraint* c = selectedConstraintEntity.getComponentIfExists<hinge_constraint>())
			{
				vec3 pos = transformPosition(transformA, c->localAnchorA);
				vec3 hingeAxis = transformDirection(transformA, c->localHingeAxisA);
				vec3 zeroDegAxis = transformDirection(transformB, c->localHingeTangentB);
				vec3 localHingeCompareA = conjugate(transformA.rotation) * (transformB.rotation * c->localHingeTangentB);
				float curAngle = atan2(dot(localHingeCompareA, c->localHingeBitangentA), dot(localHingeCompareA, c->localHingeTangentA));
				float minAngle = c->minRotationLimit - curAngle;
				float maxAngle = c->maxRotationLimit - curAngle;

				renderAngleRing(pos, hingeAxis, 0.2f, 0.17f, zeroDegAxis, minAngle, maxAngle, constraintColor, ldrRenderPass, true);
				renderLine(pos, pos + hingeAxis * 0.2f, constraintColor, ldrRenderPass, true);
			}
			else if (cone_twist_constraint* c = selectedConstraintEntity.getComponentIfExists<cone_twist_constraint>())
			{

			}
			else if (slider_constraint* c = selectedConstraintEntity.getComponentIfExists<slider_constraint>())
			{

			}
		}
	}

	return objectChanged;
}

static void drawIconsWindow(bool& open)
{
	if (open)
	{
		if (ImGui::Begin(ICON_FA_ICONS "  Icons", &open))
		{
			static ImGuiTextFilter filter;
			filter.Draw();

			if (ImGui::BeginChild("Icons List"))
			{
				for (uint32 i = 0; i < arraysize(awesomeIcons); ++i)
				{
					ImGui::PushID(i);
					if (filter.PassFilter(awesomeIconNames[i]))
					{
						ImGui::Text("%s: %s", awesomeIconNames[i], awesomeIcons[i]);
						ImGui::SameLine();
						if (ImGui::Button("Copy to clipboard"))
						{
							ImGui::SetClipboardText(awesomeIconNames[i]);
						}
					}
					ImGui::PopID();
				}
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}
}

bool scene_editor::drawMainMenuBar()
{
	static bool iconsWindowOpen = false;
	static bool demoWindowOpen = false;
	static bool undoWindowOpen = false;
	static bool soundEditorWindowOpen = false;

	bool objectPotentiallyMoved = false;

	bool controlsClicked = false;
	bool aboutClicked = false;
	bool systemClicked = false;

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu(ICON_FA_FILE "  File"))
		{
			char textBuffer[128];
			auto [undoPossible, undoName] = currentUndoStack->undoPossible();
			snprintf(textBuffer, sizeof(textBuffer), ICON_FA_UNDO " Undo %s", undoPossible ? undoName : "");
			if (ImGui::MenuItem(textBuffer, "Ctrl+Z", false, undoPossible))
			{
				currentUndoStack->undo();
				objectPotentiallyMoved = true;
			}

			auto [redoPossible, redoName] = currentUndoStack->redoPossible();
			snprintf(textBuffer, sizeof(textBuffer), ICON_FA_REDO " Redo %s", redoPossible ? redoName : "");
			if (ImGui::MenuItem(textBuffer, "Ctrl+Y", false, redoPossible))
			{
				currentUndoStack->redo();
				objectPotentiallyMoved = true;
			}

			if (ImGui::MenuItem(undoWindowOpen ? (ICON_FA_HISTORY "  Hide undo history") : (ICON_FA_HISTORY "  Show undo history")))
			{
				undoWindowOpen = !undoWindowOpen;
			}
			ImGui::Separator();

			if (ImGui::MenuItem(ICON_FA_SAVE "  Save scene", "Ctrl+S"))
			{
				serializeToFile();
			}

			if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  Load scene", "Ctrl+O"))
			{
				deserializeFromFile();
			}

			ImGui::Separator();
			if (ImGui::MenuItem(ICON_FA_TIMES "  Exit", "Esc"))
			{
				PostQuitMessage(0);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(ICON_FA_TOOLS "  Developer"))
		{
			if (ImGui::MenuItem(iconsWindowOpen ? (ICON_FA_ICONS "  Hide available icons") : (ICON_FA_ICONS "  Show available icons")))
			{
				iconsWindowOpen = !iconsWindowOpen;
			}

			if (ImGui::MenuItem(demoWindowOpen ? (ICON_FA_PUZZLE_PIECE "  Hide demo window") : (ICON_FA_PUZZLE_PIECE "  Show demo window")))
			{
				demoWindowOpen = !demoWindowOpen;
			}

			ImGui::Separator();

			if (ImGui::MenuItem(dxProfilerWindowOpen ? (ICON_FA_CHART_BAR "  Hide GPU profiler") : (ICON_FA_CHART_BAR "  Show GPU profiler"), nullptr, nullptr, ENABLE_DX_PROFILING))
			{
				dxProfilerWindowOpen = !dxProfilerWindowOpen;
			}

			if (ImGui::MenuItem(cpuProfilerWindowOpen ? (ICON_FA_CHART_LINE "  Hide CPU profiler") : (ICON_FA_CHART_LINE "  Show CPU profiler"), nullptr, nullptr, ENABLE_CPU_PROFILING))
			{
				cpuProfilerWindowOpen = !cpuProfilerWindowOpen;
			}

			ImGui::Separator();

			if (ImGui::MenuItem(logWindowOpen ? (ICON_FA_CLIPBOARD_LIST "  Hide message log") : (ICON_FA_CLIPBOARD_LIST "  Show message log"), "Ctrl+L", nullptr, ENABLE_MESSAGE_LOG))
			{
				logWindowOpen = !logWindowOpen;
			}

			ImGui::Separator();

			if (ImGui::MenuItem(soundEditorWindowOpen ? (EDITOR_ICON_AUDIO "  Hide sound editor") : (EDITOR_ICON_AUDIO "  Show sound editor")))
			{
				soundEditorWindowOpen = !soundEditorWindowOpen;
			}

			if (ImGui::MenuItem(editorPanels->meshEditor.isOpen() ? (EDITOR_ICON_MESH "  Hide mesh editor") : (EDITOR_ICON_MESH "  Show mesh editor")))
			{
				editorPanels->meshEditor.isOpen() ? editorPanels->meshEditor.close() : editorPanels->meshEditor.open();
			}

			ImGui::Separator();

			if (ImGui::MenuItem(ICON_FA_DESKTOP "  System"))
			{
				systemClicked = true;
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(ICON_FA_CHILD "  Help"))
		{
			if (ImGui::MenuItem(ICON_FA_COMPASS "  Controls"))
			{
				controlsClicked = true;
			}

			if (ImGui::MenuItem(ICON_FA_QUESTION "  About"))
			{
				aboutClicked = true;
			}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();

	if (systemClicked)
	{
		ImGui::OpenPopup(ICON_FA_DESKTOP "  System");
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal(ICON_FA_DESKTOP "  System", 0, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Value("CPU", systemInfo.cpuName.c_str());
		ImGui::Value("GPU", systemInfo.gpuName.c_str());

		ImGui::Separator();

		ImGui::PopupOkButton();
		ImGui::EndPopup();
	}

	if (controlsClicked)
	{
		ImGui::OpenPopup(ICON_FA_COMPASS "  Controls");
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal(ICON_FA_COMPASS "  Controls", 0, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("The camera can be controlled in two modes:");
		ImGui::BulletText(
			"Free flying: Hold the right mouse button and move the mouse to turn.\n"
			"Move with WASD (while holding right mouse). Q & E let you move down and up.\n"
			"Holding Shift will make you fly faster, Ctrl will make you slower."
		);
		ImGui::BulletText(
			"Orbit: While holding Alt, press and hold the left mouse button to\n"
			"orbit around a point in front of the camera. Hold the middle mouse button \n"
			"to pan."
		);
		ImGui::Separator();
		ImGui::Text(
			"Left-click on objects to select them. Toggle through gizmos using\n"
			"Q (no gizmo), W (translate), E (rotate), R (scale).\n"
			"Press G to toggle between global and local coordinate system.\n"
			"You can also change the object's transform in the Scene Hierarchy window."
		);
		ImGui::Separator();
		ImGui::Text(
			"Press F to focus the camera on the selected object. This automatically\n"
			"sets the orbit distance such that you now orbit around this object (with alt, see above)."
		);
		ImGui::Separator();
		ImGui::Text(
			"Press V to toggle Vsync on or off."
		);
		ImGui::Separator();
		ImGui::Text(
			"You can drag and drop meshes from the asset window at the bottom into the scene\n"
			"window to add it to the scene."
		);
		ImGui::Separator();
		ImGui::Text(
			"Press the Print key to capture a screenshot of the scene viewport or Ctrl+Print to capture\n"
			"one including the editor UI."
		);
		ImGui::Separator();

		ImGui::PopupOkButton();
		ImGui::EndPopup();
	}

	if (aboutClicked)
	{
		ImGui::OpenPopup(ICON_FA_QUESTION "  About");
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal(ICON_FA_QUESTION "  About", 0, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Dummy(ImVec2(300.f, 1.f));
		ImGui::CenteredText("Direct3D renderer");
		ImGui::CenteredText("written from scratch by Philipp Kurth");

		ImGui::Separator();

		ImGui::PopupOkButton();
		ImGui::EndPopup();
	}

	if (demoWindowOpen)
	{
		ImGui::ShowDemoWindow(&demoWindowOpen);
	}

	drawIconsWindow(iconsWindowOpen);
	drawSoundEditor(soundEditorWindowOpen);
	objectPotentiallyMoved |= currentUndoStack->showHistory(undoWindowOpen);

	return objectPotentiallyMoved;
}

template<typename component_t, typename ui_func>
static void drawComponent(scene_entity entity, const char* componentName, ui_func func)
{
	const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;
	if (auto* component = entity.getComponentIfExists<component_t>())
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
		bool open = ImGui::TreeNodeEx(componentName, treeNodeFlags, componentName);
		ImGui::PopStyleVar();

		if (open)
		{
			func(*component);
			ImGui::TreePop();
		}
	}
}

static bounding_box getObjectBoundingBox(scene_entity entity, bool applyPosition)
{
	bounding_box aabb = entity.hasComponent<mesh_component>() ? entity.getComponent<mesh_component>().mesh->aabb : bounding_box::fromCenterRadius(0.f, 1.f);

	if (transform_component* transform = entity.getComponentIfExists<transform_component>())
	{
		aabb.minCorner *= transform->scale;
		aabb.maxCorner *= transform->scale;

		if (applyPosition)
		{
			aabb.minCorner += transform->position;
			aabb.maxCorner += transform->position;
		}
	}
	else if (position_component* transform = entity.getComponentIfExists<position_component>())
	{
		if (applyPosition)
		{
			aabb.minCorner += transform->position;
			aabb.maxCorner += transform->position;
		}
	}
	else if (position_rotation_component* transform = entity.getComponentIfExists<position_rotation_component>())
	{
		if (applyPosition)
		{
			aabb.minCorner += transform->position;
			aabb.maxCorner += transform->position;
		}
	}

	return aabb;
}

static void editTexture(const char* name, ref<dx_texture>& tex, uint32 loadFlags)
{
	asset_handle asset = {};
	if (tex)
	{
		asset = tex->handle;
	}
	if (ImGui::PropertyAssetHandle(name, EDITOR_ICON_IMAGE, asset))
	{
		fs::path path = getPathFromAssetHandle(asset);
		fs::path relative = fs::relative(path, fs::current_path());
		if (auto newTex = loadTextureFromFile(relative.string(), loadFlags))
		{
			tex = newTex;
		}
	}
}

static void editMesh(const char* name, ref<multi_mesh>& mesh, uint32 loadFlags)
{
	asset_handle asset = {};
	if (mesh)
	{
		asset = mesh->handle;
	}
	if (ImGui::PropertyAssetHandle(name, EDITOR_ICON_MESH, asset))
	{
		fs::path path = getPathFromAssetHandle(asset);
		fs::path relative = fs::relative(path, fs::current_path());
		if (auto newMesh = loadMeshFromFile(relative.string(), loadFlags))
		{
			mesh = newMesh;
		}
	}
}

static void editMaterial(const ref<pbr_material>& material)
{
	if (ImGui::BeginProperties())
	{
		asset_handle dummy = {};

		editTexture("Albedo", material->albedo, image_load_flags_default);
		editTexture("Normal", material->normal, image_load_flags_default_noncolor);
		editTexture("Roughness", material->roughness, image_load_flags_default_noncolor);
		editTexture("Metallic", material->metallic, image_load_flags_default_noncolor);

		ImGui::PropertyColor("Emission", material->emission);
		ImGui::PropertyColor("Albedo tint", material->albedoTint);
		ImGui::PropertyDropdown("Shader", pbrMaterialShaderNames, pbr_material_shader_count, (uint32&)material->shader);
		ImGui::PropertySlider("UV scale", material->uvScale);

		if (!material->roughness)
		{
			ImGui::PropertySlider("Roughness override", material->roughnessOverride);
		}
		if (!material->metallic)
		{
			ImGui::PropertySlider("Metallic override", material->metallicOverride);
		}

		ImGui::EndProperties();
	}
}

bool scene_editor::drawSceneHierarchy()
{
	game_scene& scene = this->scene->getCurrentScene();

	bool objectMovedByWidget = false;

	if (ImGui::Begin("Scene Hierarchy"))
	{
		if (ImGui::BeginChild("Outliner", ImVec2(0, 250)))
		{
			scene.view<tag_component>()
				.each([this, &scene](auto entityHandle, tag_component& tag)
			{
				ImGui::PushID((uint32)entityHandle);
				const char* name = tag.name;
				scene_entity entity = { entityHandle, scene };

				if (ImGui::Selectable(name, entity == selectedEntity))
				{
					setSelectedEntity(entity);
				}

				bool entityDeleted = false;
				if (ImGui::BeginPopupContextItem(name))
				{
					if (ImGui::MenuItem("Delete"))
					{
						entityDeleted = true;
					}

					ImGui::EndPopup();
				}

				if (entityDeleted)
				{
					if (entity == selectedEntity)
					{
						setSelectedEntity({});
					}
					currentUndoStack->pushAction("entity deletion", entity_existence_undo(scene, entity));
					scene.deleteEntity(entity);
				}
				ImGui::PopID();
			});
		}
		ImGui::EndChild();
		ImGui::Separator();

		if (selectedEntity)
		{
			ImGui::Dummy(ImVec2(0, 20));
			ImGui::Separator();
			ImGui::Separator();
			ImGui::Separator();

			if (ImGui::BeginChild("Components"))
			{
				ImGui::AlignTextToFramePadding();

				ImGui::PushID((uint32)selectedEntity);

				{
					tag_component& tag = selectedEntity.getComponent<tag_component>();
					tag_component tagBefore = tag;
					ImGui::InputText("Name", tag.name, sizeof(tag_component::name));

					if (ImGui::IsItemActive() && !ImGui::IsItemActiveLastFrame())
					{
						currentUndoBuffer->as<tag_component>() = tagBefore;
					}
					if (ImGui::IsItemDeactivatedAfterEdit())
					{
						currentUndoStack->pushAction("entity name", component_undo<tag_component>(selectedEntity, currentUndoBuffer->as<tag_component>()));
					}
				}

				ImGui::PopID();



				ImGui::SameLine();
				if (ImGui::Button(ICON_FA_TRASH_ALT))
				{
					currentUndoStack->pushAction("entity deletion", entity_existence_undo(scene, selectedEntity));
					scene.deleteEntity(selectedEntity);
					setSelectedEntity({});
					objectMovedByWidget = true;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Delete entity");
				}
				if (selectedEntity)
				{
					drawComponent<transform_component>(selectedEntity, "TRANSFORM", [this, &objectMovedByWidget](transform_component& transform)
					{
						using component_t = transform_component;

						UNDOABLE_COMPONENT_SETTING("entity position", transform.position,
							objectMovedByWidget |= ImGui::Drag("Position", transform.position, 0.1f));
						UNDOABLE_COMPONENT_SETTING("entity rotation", transform.rotation,
							if (ImGui::Drag("Rotation", selectedEntityEulerRotation, 0.1f))
							{
								transform.rotation = getQuat(selectedEntityEulerRotation);
								objectMovedByWidget = true;
							}, [](transform_component&, quat rot, void* userData) { *(vec3*)userData = getEuler(rot); }, &selectedEntityEulerRotation);
						UNDOABLE_COMPONENT_SETTING("entity scale", transform.scale,
							objectMovedByWidget |= ImGui::Drag("Scale", transform.scale, 0.1f));
					});

					drawComponent<position_component>(selectedEntity, "TRANSFORM", [this, &objectMovedByWidget](position_component& transform)
					{
						using component_t = position_component;

						UNDOABLE_COMPONENT_SETTING("entity position", transform.position,
							objectMovedByWidget |= ImGui::Drag("Position", transform.position, 0.1f));
					});

					drawComponent<position_rotation_component>(selectedEntity, "TRANSFORM", [this, &objectMovedByWidget](position_rotation_component& transform)
					{
						using component_t = position_rotation_component;

						UNDOABLE_COMPONENT_SETTING("entity position", transform.position,
							objectMovedByWidget |= ImGui::Drag("Position", transform.position, 0.1f));
						UNDOABLE_COMPONENT_SETTING("entity rotation", transform.rotation,
							if (ImGui::Drag("Rotation", selectedEntityEulerRotation, 0.1f))
							{
								transform.rotation = getQuat(selectedEntityEulerRotation);
								objectMovedByWidget = true;
							}, [](position_rotation_component&, quat rot, void* userData) { *(vec3*)userData = getEuler(rot); }, & selectedEntityEulerRotation);
					});

					drawComponent<position_scale_component>(selectedEntity, "TRANSFORM", [this, &objectMovedByWidget](position_scale_component& transform)
					{
						using component_t = position_scale_component;

						UNDOABLE_COMPONENT_SETTING("entity position", transform.position,
							objectMovedByWidget |= ImGui::Drag("Position", transform.position, 0.1f));
						UNDOABLE_COMPONENT_SETTING("entity scale", transform.scale,
							objectMovedByWidget |= ImGui::Drag("Scale", transform.scale, 0.1f));
					});

					drawComponent<dynamic_transform_component>(selectedEntity, "DYNAMIC", [](dynamic_transform_component& dynamic)
					{
						ImGui::Text("Dynamic");
					});

					drawComponent<mesh_component>(selectedEntity, "MESH", [this](mesh_component& raster)
					{
						using component_t = mesh_component;

						if (ImGui::BeginProperties())
						{
							editMesh("Mesh", raster.mesh, mesh_creation_flags_default);
							ImGui::EndProperties();
						}
						if (ImGui::BeginTree("Submeshes"))
						{
							for (const auto& sub : raster.mesh->submeshes)
							{
								ImGui::PushID(&sub);
								if (ImGui::BeginTree(sub.name.c_str()))
								{
									editMaterial(sub.material);

									ImGui::EndTree();
								}
								ImGui::PopID();
							}

							ImGui::EndTree();
						}
					});

					drawComponent<terrain_component>(selectedEntity, "TERRAIN", [this, &objectMovedByWidget](terrain_component& terrain)
					{
						using component_t = terrain_component;

						if (ImGui::BeginProperties())
						{
							UNDOABLE_COMPONENT_SETTING("terrain amplitude scale", terrain.amplitudeScale,
								objectMovedByWidget |= ImGui::PropertyDrag("Amplitude scale", terrain.amplitudeScale, 0.1f, 0.f));

							auto& settings = terrain.genSettings;
							UNDOABLE_COMPONENT_SETTING("terrain noise scale", settings.scale,
								objectMovedByWidget |= ImGui::PropertyDrag("Noise scale", settings.scale, 0.001f));
							UNDOABLE_COMPONENT_SETTING("terrain domain warp strength", settings.domainWarpStrength,
								objectMovedByWidget |= ImGui::PropertyDrag("Domain warp strength", settings.domainWarpStrength, 0.05f, 0.f));
							UNDOABLE_COMPONENT_SETTING("terrain domain warp octaves", settings.domainWarpOctaves,
								objectMovedByWidget |= ImGui::PropertySlider("Domain warp octaves", settings.domainWarpOctaves, 1, 16));
							UNDOABLE_COMPONENT_SETTING("terrain noise octaves", settings.noiseOctaves,
								objectMovedByWidget |= ImGui::PropertySlider("Noise octaves", settings.noiseOctaves, 1, 32));

							ImGui::EndProperties();
						}

						if (ImGui::BeginTree("Ground"))
						{
							editMaterial(terrain.groundMaterial);
							ImGui::EndTree();
						}

						if (ImGui::BeginTree("Rock"))
						{
							editMaterial(terrain.rockMaterial);
							ImGui::EndTree();
						}
					});

					drawComponent<proc_placement_component>(selectedEntity, "PROCEDURAL PLACEMENT", [this](proc_placement_component& placement)
					{
						using component_t = proc_placement_component;

						for (auto& layer : placement.layers)
						{
							if (ImGui::BeginTree(layer.name))
							{
								if (ImGui::BeginProperties())
								{
									UNDOABLE_COMPONENT_SETTING("proc placement layer footprint", layer.footprint,
										ImGui::PropertyDrag("Footprint", layer.footprint, 0.05f, 0.01f));
									for (uint32 i = 0; i < layer.numMeshes; ++i)
									{
										ImGui::PushID(i);
										UNDOABLE_COMPONENT_SETTING("proc placement layer density", layer.densities[i],
											ImGui::PropertySlider("Density", layer.densities[i]));
										ImGui::PopID();
									}
									ImGui::EndProperties();
								}
								ImGui::EndTree();
							}
						}
					});

					drawComponent<grass_component>(selectedEntity, "GRASS", [this](grass_component& grass)
					{
						using component_t = grass_component;

						if (ImGui::BeginProperties())
						{
							UNDOABLE_SETTING("grass depth prepass", grass_settings::depthPrepass,
								ImGui::PropertyCheckbox("Depth prepass", grass_settings::depthPrepass));

							grass_settings& settings = grass.settings;

							UNDOABLE_COMPONENT_SETTING("grass blades per chunk dim", settings.numGrassBladesPerChunkDim,
								ImGui::PropertyDrag("Blades per chunk dim", settings.numGrassBladesPerChunkDim, 2));
							UNDOABLE_COMPONENT_SETTING("grass blade height", settings.bladeHeight,
								ImGui::PropertySlider("Blade height", settings.bladeHeight, 0.f, 2.f));
							UNDOABLE_COMPONENT_SETTING("grass blade width", settings.bladeWidth,
								ImGui::PropertySlider("Blade width", settings.bladeWidth, 0.f, 1.f));

							UNDOABLE_COMPONENT_SETTING("grass LOD change start distance", settings.lodChangeStartDistance,
								ImGui::PropertyDrag("LOD change start distance", settings.lodChangeStartDistance, 0.5f, 0.f));
							UNDOABLE_COMPONENT_SETTING("grass LOD change transition distance", settings.lodChangeTransitionDistance,
								ImGui::PropertyDrag("LOD change transition distance", settings.lodChangeTransitionDistance, 0.5f, 0.f));

							UNDOABLE_COMPONENT_SETTING("grass cull start distance", settings.cullStartDistance,
								ImGui::PropertyDrag("Cull start distance", settings.cullStartDistance, 0.5f, 0.f));
							UNDOABLE_COMPONENT_SETTING("grass cull transition distance", settings.cullTransitionDistance,
								ImGui::PropertyDrag("Cull transition distance", settings.cullTransitionDistance, 0.5f, 0.f));

							ImGui::EndProperties();
						}
					});

					drawComponent<water_component>(selectedEntity, "WATER", [this](water_component& water)
					{
						using component_t = water_component;

						if (ImGui::BeginProperties())
						{
							water_settings& settings = water.settings;

							UNDOABLE_COMPONENT_SETTING("deep water color", settings.deepWaterColor,
								ImGui::PropertyColor("Deep color", settings.deepWaterColor));
							UNDOABLE_COMPONENT_SETTING("shallow water color", settings.shallowWaterColor,
								ImGui::PropertyColor("Shallow color", settings.shallowWaterColor));

							UNDOABLE_COMPONENT_SETTING("shallow water depth", settings.shallowDepth,
								ImGui::PropertyDrag("Shallow depth", settings.shallowDepth, 0.05f, 0.f));
							UNDOABLE_COMPONENT_SETTING("water transition strength", settings.transitionStrength,
								ImGui::PropertySlider("Transition strength", settings.transitionStrength, 0.f, 2.f));
							UNDOABLE_COMPONENT_SETTING("water normal map strength", settings.normalStrength,
								ImGui::PropertySlider("Normal map strength", settings.normalStrength));
							UNDOABLE_COMPONENT_SETTING("water UV scale", settings.uvScale,
								ImGui::PropertyDrag("UV scale", settings.uvScale, 0.05f, 0.f));

							ImGui::EndProperties();
						}
					});

					drawComponent<animation_component>(selectedEntity, "ANIMATION", [this](animation_component& anim)
					{
						if (mesh_component* mesh = selectedEntity.getComponentIfExists<mesh_component>())
						{
							if (ImGui::BeginProperties())
							{
								uint32 animationIndex = anim.animation.clip ? (uint32)(anim.animation.clip - mesh->mesh->skeleton.clips.data()) : -1;

								bool animationChanged = ImGui::PropertyDropdown("Currently playing", [](uint32 index, void* data)
								{
									if (index == -1) { return "---"; }

									animation_skeleton& skeleton = *(animation_skeleton*)data;
									const char* result = 0;
									if (index < (uint32)skeleton.clips.size())
									{
										result = skeleton.clips[index].name.c_str();
									}
									return result;
								}, animationIndex, & mesh->mesh->skeleton);

								if (animationChanged)
								{
									anim.animation.set(&mesh->mesh->skeleton.clips[animationIndex]);
								}

								ImGui::EndProperties();
							}

							animation_skeleton& skeleton = mesh->mesh->skeleton;
							if (skeleton.joints.size() > 0)
							{
								if (ImGui::BeginTree("Skeleton"))
								{
									if (ImGui::BeginTree("Joints"))
									{
										for (uint32 i = 0; i < (uint32)skeleton.joints.size(); ++i)
										{
											const skeleton_joint& j = skeleton.joints[i];
											vec3 c = limbTypeColors[j.limbType];
											ImGui::TextColored(ImVec4(c.x, c.y, c.z, 1.f), j.name.c_str());
										}

										ImGui::EndTree();
									}

									if (ImGui::BeginTree("Limbs"))
									{
										for (uint32 i = 0; i < limb_type_count; ++i)
										{
											if (i != limb_type_unknown)
											{
												skeleton_limb& l = skeleton.limbs[i];
												vec3 c = limbTypeColors[i];
												if (ImGui::BeginTreeColoredText(limbTypeNames[i], c))
												{
													if (ImGui::BeginProperties())
													{
														limb_dimensions& d = l.dimensions;
														ImGui::PropertyDrag("Min Y", d.minY, 0.01f);
														ImGui::PropertyDrag("Max Y", d.maxY, 0.01f);
														ImGui::PropertyDrag("Radius", d.radius, 0.01f);

														ImGui::PropertyDrag("Offset X", d.xOffset, 0.01f);
														ImGui::PropertyDrag("Offset Z", d.zOffset, 0.01f);

														ImGui::EndProperties();
													}

													ImGui::EndTree();
												}
											}
										}

										ImGui::EndTree();
									}

									ImGui::EndTree();
								}
							}
						}
					});

					drawComponent<rigid_body_component>(selectedEntity, "RIGID BODY", [this, &scene](rigid_body_component& rb)
					{
						using component_t = rigid_body_component;

						if (ImGui::BeginProperties())
						{
							if (rb.invMass != 0)
							{
								ImGui::PropertyValue("Mass", 1.f / rb.invMass, "%.3fkg");
							}
							UNDOABLE_COMPONENT_SETTING("rigid body linear velocity damping", rb.linearDamping,
								ImGui::PropertySlider("Linear velocity damping", rb.linearDamping));
							UNDOABLE_COMPONENT_SETTING("rigid body angular velocity damping", rb.angularDamping,
								ImGui::PropertySlider("Angular velocity damping", rb.angularDamping));
							UNDOABLE_COMPONENT_SETTING("rigid body gravity factor", rb.gravityFactor,
								ImGui::PropertySlider("Gravity factor", rb.gravityFactor));

							//ImGui::PropertyValue("Linear velocity", rb.linearVelocity);
							//ImGui::PropertyValue("Angular velocity", rb.angularVelocity);

							ImGui::EndProperties();
						}
					});

					drawComponent<physics_reference_component>(selectedEntity, "COLLIDERS", [this, &scene](physics_reference_component& reference)
					{
						// TODO UNDO
						bool dirty = false;

						for (scene_entity colliderEntity : collider_entity_iterator(selectedEntity))
						{
							ImGui::PushID((int)colliderEntity.handle);

							drawComponent<collider_component>(colliderEntity, "Collider", [&colliderEntity, &dirty, this](collider_component& collider)
							{
								switch (collider.type)
								{
									case collider_type_sphere:
									{
										if (ImGui::BeginTree("Shape: Sphere"))
										{
											if (ImGui::BeginProperties())
											{
												dirty |= ImGui::PropertyDrag("Local center", collider.sphere.center, 0.05f);
												dirty |= ImGui::PropertyDrag("Radius", collider.sphere.radius, 0.05f, 0.f);
												ImGui::EndProperties();
											}
											ImGui::EndTree();
										}
									} break;
									case collider_type_capsule:
									{
										if (ImGui::BeginTree("Shape: Capsule"))
										{
											if (ImGui::BeginProperties())
											{
												dirty |= ImGui::PropertyDrag("Local point A", collider.capsule.positionA, 0.05f);
												dirty |= ImGui::PropertyDrag("Local point B", collider.capsule.positionB, 0.05f);
												dirty |= ImGui::PropertyDrag("Radius", collider.capsule.radius, 0.05f, 0.f);
												ImGui::EndProperties();
											}
											ImGui::EndTree();
										}
									} break;
									case collider_type_cylinder:
									{
										if (ImGui::BeginTree("Shape: Cylinder"))
										{
											if (ImGui::BeginProperties())
											{
												dirty |= ImGui::PropertyDrag("Local point A", collider.cylinder.positionA, 0.05f);
												dirty |= ImGui::PropertyDrag("Local point B", collider.cylinder.positionB, 0.05f);
												dirty |= ImGui::PropertyDrag("Radius", collider.cylinder.radius, 0.05f, 0.f);
												ImGui::EndProperties();
											}
											ImGui::EndTree();
										}
									} break;
									case collider_type_aabb:
									{
										if (ImGui::BeginTree("Shape: AABB"))
										{
											if (ImGui::BeginProperties())
											{
												dirty |= ImGui::PropertyDrag("Local min", collider.aabb.minCorner, 0.05f);
												dirty |= ImGui::PropertyDrag("Local max", collider.aabb.maxCorner, 0.05f);
												ImGui::EndProperties();
											}
											ImGui::EndTree();
										}
									} break;
									case collider_type_obb:
									{
										if (ImGui::BeginTree("Shape: OBB"))
										{
											if (ImGui::BeginProperties())
											{
												ImGui::EndProperties();
											}
											ImGui::EndTree();
										}
									} break;
									case collider_type_hull:
									{
										if (ImGui::BeginTree("Shape: Hull"))
										{
											if (ImGui::BeginProperties())
											{
												ImGui::EndProperties();
											}
											ImGui::EndTree();
										}
									} break;
								}

								if (ImGui::BeginProperties())
								{
									ImGui::PropertySlider("Restitution", collider.material.restitution);
									ImGui::PropertySlider("Friction", collider.material.friction);
									dirty |= ImGui::PropertyDrag("Density", collider.material.density, 0.05f, 0.f);

									bool editCollider = selectedColliderEntity == colliderEntity;
									if (ImGui::PropertyCheckbox("Edit", editCollider))
									{
										selectedColliderEntity = editCollider ? colliderEntity : scene_entity{};
									}

									ImGui::EndProperties();
								}
							});

							ImGui::PopID();
						}

						if (dirty)
						{
							if (rigid_body_component* rb = selectedEntity.getComponentIfExists<rigid_body_component>())
							{
								rb->recalculateProperties(&scene.registry, reference);
							}
						}
					});

					drawComponent<physics_reference_component>(selectedEntity, "CONSTRAINTS", [this](physics_reference_component& reference)
					{
						// TODO UNDO
						for (auto [constraintEntity, constraintType] : constraint_entity_iterator(selectedEntity))
						{
							ImGui::PushID((int)constraintEntity.handle);

							switch (constraintType)
							{
								case constraint_type_distance:
								{
									drawComponent<distance_constraint>(constraintEntity, "Distance constraint", [this, constraintEntity = constraintEntity](distance_constraint& constraint)
									{
										if (ImGui::BeginProperties())
										{
											scene_entity otherEntity = getOtherEntity(constraintEntity.getComponent<constraint_entity_reference_component>(), selectedEntity);
											if (ImGui::PropertyButton("Connected entity", ICON_FA_CUBE, otherEntity.getComponent<tag_component>().name))
											{
												setSelectedEntity(otherEntity);
											}

											bool visConstraint = selectedConstraintEntity == constraintEntity;
											if (ImGui::PropertyCheckbox("Visualize", visConstraint))
											{
												selectedConstraintEntity = visConstraint ? constraintEntity : scene_entity{};
											}

											ImGui::PropertySlider("Length", constraint.globalLength);
											ImGui::EndProperties();
										}
									});
								} break;

								case constraint_type_ball:
								{
									drawComponent<ball_constraint>(constraintEntity, "Ball constraint", [this, constraintEntity = constraintEntity](ball_constraint& constraint)
									{
										if (ImGui::BeginProperties())
										{
											scene_entity otherEntity = getOtherEntity(constraintEntity.getComponent<constraint_entity_reference_component>(), selectedEntity);
											if (ImGui::PropertyButton("Connected entity", ICON_FA_CUBE, otherEntity.getComponent<tag_component>().name))
											{
												setSelectedEntity(otherEntity);
											}

											bool visConstraint = selectedConstraintEntity == constraintEntity;
											if (ImGui::PropertyCheckbox("Visualize", visConstraint))
											{
												selectedConstraintEntity = visConstraint ? constraintEntity : scene_entity{};
											}

											ImGui::EndProperties();
										}
									});
								} break;

								case constraint_type_fixed:
								{
									drawComponent<fixed_constraint>(constraintEntity, "Fixed constraint", [this, constraintEntity = constraintEntity](fixed_constraint& constraint)
									{
										if (ImGui::BeginProperties())
										{
											scene_entity otherEntity = getOtherEntity(constraintEntity.getComponent<constraint_entity_reference_component>(), selectedEntity);
											if (ImGui::PropertyButton("Connected entity", ICON_FA_CUBE, otherEntity.getComponent<tag_component>().name))
											{
												setSelectedEntity(otherEntity);
											}

											bool visConstraint = selectedConstraintEntity == constraintEntity;
											if (ImGui::PropertyCheckbox("Visualize", visConstraint))
											{
												selectedConstraintEntity = visConstraint ? constraintEntity : scene_entity{};
											}

											ImGui::EndProperties();
										}
									});
								} break;

								case constraint_type_hinge:
								{
									drawComponent<hinge_constraint>(constraintEntity, "Hinge constraint", [this, constraintEntity = constraintEntity](hinge_constraint& constraint)
									{
										if (ImGui::BeginProperties())
										{
											scene_entity otherEntity = getOtherEntity(constraintEntity.getComponent<constraint_entity_reference_component>(), selectedEntity);
											if (ImGui::PropertyButton("Connected entity", ICON_FA_CUBE, otherEntity.getComponent<tag_component>().name))
											{
												setSelectedEntity(otherEntity);
											}

											bool minLimitActive = constraint.minRotationLimit <= 0.f;
											if (ImGui::PropertyCheckbox("Lower limit active", minLimitActive))
											{
												constraint.minRotationLimit = -constraint.minRotationLimit;
											}
											if (minLimitActive)
											{
												float minLimit = -constraint.minRotationLimit;
												ImGui::PropertySliderAngle("Lower limit", minLimit, 0.f, 180.f, "-%.0f deg");
												constraint.minRotationLimit = -minLimit;
											}

											bool maxLimitActive = constraint.maxRotationLimit >= 0.f;
											if (ImGui::PropertyCheckbox("Upper limit active", maxLimitActive))
											{
												constraint.maxRotationLimit = -constraint.maxRotationLimit;
											}
											if (maxLimitActive)
											{
												ImGui::PropertySliderAngle("Upper limit", constraint.maxRotationLimit, 0.f, 180.f);
											}

											bool motorActive = constraint.maxMotorTorque > 0.f;
											if (ImGui::PropertyCheckbox("Motor active", motorActive))
											{
												constraint.maxMotorTorque = -constraint.maxMotorTorque;
											}
											if (motorActive)
											{
												ImGui::PropertyDropdown("Motor type", constraintMotorTypeNames, arraysize(constraintMotorTypeNames), (uint32&)constraint.motorType);

												if (constraint.motorType == constraint_velocity_motor)
												{
													ImGui::PropertySliderAngle("Motor velocity", constraint.motorVelocity, -1000.f, 1000.f);
												}
												else
												{
													float lo = minLimitActive ? constraint.minRotationLimit : -M_PI;
													float hi = maxLimitActive ? constraint.maxRotationLimit : M_PI;
													ImGui::PropertySliderAngle("Motor target angle", constraint.motorTargetAngle, rad2deg(lo), rad2deg(hi));
												}

												ImGui::PropertySlider("Max motor torque", constraint.maxMotorTorque, 0.001f, 10000.f);
											}

											bool visConstraint = selectedConstraintEntity == constraintEntity;
											if (ImGui::PropertyCheckbox("Visualize", visConstraint))
											{
												selectedConstraintEntity = visConstraint ? constraintEntity : scene_entity{};
											}
											ImGui::EndProperties();
										}
									});
								} break;

								case constraint_type_cone_twist:
								{
									drawComponent<cone_twist_constraint>(constraintEntity, "Cone twist constraint", [this, constraintEntity = constraintEntity](cone_twist_constraint& constraint)
									{
										if (ImGui::BeginProperties())
										{
											scene_entity otherEntity = getOtherEntity(constraintEntity.getComponent<constraint_entity_reference_component>(), selectedEntity);
											if (ImGui::PropertyButton("Connected entity", ICON_FA_CUBE, otherEntity.getComponent<tag_component>().name))
											{
												setSelectedEntity(otherEntity);
											}

											bool swingLimitActive = constraint.swingLimit >= 0.f;
											if (ImGui::PropertyCheckbox("Swing limit active", swingLimitActive))
											{
												constraint.swingLimit = -constraint.swingLimit;
											}
											if (swingLimitActive)
											{
												ImGui::PropertySliderAngle("Swing limit", constraint.swingLimit, 0.f, 180.f);
											}

											bool twistLimitActive = constraint.twistLimit >= 0.f;
											if (ImGui::PropertyCheckbox("Twist limit active", twistLimitActive))
											{
												constraint.twistLimit = -constraint.twistLimit;
											}
											if (twistLimitActive)
											{
												ImGui::PropertySliderAngle("Twist limit", constraint.twistLimit, 0.f, 180.f);
											}

											bool twistMotorActive = constraint.maxTwistMotorTorque > 0.f;
											if (ImGui::PropertyCheckbox("Twist motor active", twistMotorActive))
											{
												constraint.maxTwistMotorTorque = -constraint.maxTwistMotorTorque;
											}
											if (twistMotorActive)
											{
												ImGui::PropertyDropdown("Twist motor type", constraintMotorTypeNames, arraysize(constraintMotorTypeNames), (uint32&)constraint.twistMotorType);

												if (constraint.twistMotorType == constraint_velocity_motor)
												{
													ImGui::PropertySliderAngle("Twist motor velocity", constraint.twistMotorVelocity, -360.f, 360.f);
												}
												else
												{
													float li = twistLimitActive ? constraint.twistLimit : -M_PI;
													ImGui::PropertySliderAngle("Twist motor target angle", constraint.twistMotorTargetAngle, rad2deg(-li), rad2deg(li));
												}

												ImGui::PropertySlider("Max twist motor torque", constraint.maxTwistMotorTorque, 0.001f, 1000.f);
											}

											bool swingMotorActive = constraint.maxSwingMotorTorque > 0.f;
											if (ImGui::PropertyCheckbox("Swing motor active", swingMotorActive))
											{
												constraint.maxSwingMotorTorque = -constraint.maxSwingMotorTorque;
											}
											if (swingMotorActive)
											{
												ImGui::PropertyDropdown("Swing motor type", constraintMotorTypeNames, arraysize(constraintMotorTypeNames), (uint32&)constraint.swingMotorType);

												if (constraint.swingMotorType == constraint_velocity_motor)
												{
													ImGui::PropertySliderAngle("Swing motor velocity", constraint.swingMotorVelocity, -360.f, 360.f);
												}
												else
												{
													float li = swingLimitActive ? constraint.swingLimit : -M_PI;
													ImGui::PropertySliderAngle("Swing motor target angle", constraint.swingMotorTargetAngle, rad2deg(-li), rad2deg(li));
												}

												ImGui::PropertySliderAngle("Swing motor axis angle", constraint.swingMotorAxis, -180.f, 180.f);
												ImGui::PropertySlider("Max swing motor torque", constraint.maxSwingMotorTorque, 0.001f, 1000.f);
											}

											bool visConstraint = selectedConstraintEntity == constraintEntity;
											if (ImGui::PropertyCheckbox("Visualize", visConstraint))
											{
												selectedConstraintEntity = visConstraint ? constraintEntity : scene_entity{};
											}
											ImGui::EndProperties();
										}
									});
								} break;

								case constraint_type_slider:
								{
									drawComponent<slider_constraint>(constraintEntity, "Slider constraint", [this, constraintEntity = constraintEntity](slider_constraint& constraint)
									{
										if (ImGui::BeginProperties())
										{
											scene_entity otherEntity = getOtherEntity(constraintEntity.getComponent<constraint_entity_reference_component>(), selectedEntity);
											if (ImGui::PropertyButton("Connected entity", ICON_FA_CUBE, otherEntity.getComponent<tag_component>().name))
											{
												setSelectedEntity(otherEntity);
											}

											bool minLimitActive = constraint.negDistanceLimit <= 0.f;
											if (ImGui::PropertyCheckbox("Lower limit active", minLimitActive))
											{
												constraint.negDistanceLimit = -constraint.negDistanceLimit;
											}
											if (minLimitActive)
											{
												float minLimit = -constraint.negDistanceLimit;
												ImGui::PropertySlider("Lower limit", minLimit, 0.f, 1000.f, "-%.3f");
												constraint.negDistanceLimit = -minLimit;
											}

											bool maxLimitActive = constraint.posDistanceLimit >= 0.f;
											if (ImGui::PropertyCheckbox("Upper limit active", maxLimitActive))
											{
												constraint.posDistanceLimit = -constraint.posDistanceLimit;
											}
											if (maxLimitActive)
											{
												ImGui::PropertySlider("Upper limit", constraint.posDistanceLimit, 0.f, 1000.f);
											}

											bool motorActive = constraint.maxMotorForce > 0.f;
											if (ImGui::PropertyCheckbox("Motor active", motorActive))
											{
												constraint.maxMotorForce = -constraint.maxMotorForce;
											}
											if (motorActive)
											{
												ImGui::PropertyDropdown("Motor type", constraintMotorTypeNames, arraysize(constraintMotorTypeNames), (uint32&)constraint.motorType);

												if (constraint.motorType == constraint_velocity_motor)
												{
													ImGui::PropertySlider("Motor velocity", constraint.motorVelocity, -10.f, 10.f);
												}
												else
												{
													float lo = minLimitActive ? constraint.negDistanceLimit : -100.f;
													float hi = maxLimitActive ? constraint.posDistanceLimit : 100.f;
													ImGui::PropertySlider("Motor target distance", constraint.motorTargetDistance, lo, hi);
												}

												ImGui::PropertySlider("Max motor force", constraint.maxMotorForce, 0.001f, 1000.f);
											}

											bool visConstraint = selectedConstraintEntity == constraintEntity;
											if (ImGui::PropertyCheckbox("Visualize", visConstraint))
											{
												selectedConstraintEntity = visConstraint ? constraintEntity : scene_entity{};
											}
											ImGui::EndProperties();
										}
									});
								} break;
							}

							ImGui::PopID();
						}
					});

					drawComponent<cloth_component>(selectedEntity, "CLOTH", [this](cloth_component& cloth)
					{
						using component_t = cloth_component;

						if (ImGui::BeginProperties())
						{
							UNDOABLE_COMPONENT_SETTING("cloth total mass", cloth.totalMass,
								ImGui::PropertyDrag("Total mass", cloth.totalMass, 0.1f, 0.f));
							UNDOABLE_COMPONENT_SETTING("cloth stiffness", cloth.stiffness,
								ImGui::PropertySlider("Stiffness", cloth.stiffness, 0.01f, 0.7f));
							UNDOABLE_COMPONENT_SETTING("cloth velocity damping", cloth.damping,
								ImGui::PropertySlider("Velocity damping", cloth.damping, 0.f, 1.f));
							UNDOABLE_COMPONENT_SETTING("cloth gravity factor", cloth.gravityFactor,
								ImGui::PropertySlider("Gravity factor", cloth.gravityFactor, 0.f, 1.f));

							ImGui::EndProperties();
						}
					});

					drawComponent<point_light_component>(selectedEntity, "POINT LIGHT", [this](point_light_component& pl)
					{
						using component_t = point_light_component;

						if (ImGui::BeginProperties())
						{
							UNDOABLE_COMPONENT_SETTING("point light color", pl.color,
								ImGui::PropertyColorWheel("Color", pl.color));
							UNDOABLE_COMPONENT_SETTING("point light intensity", pl.intensity,
								ImGui::PropertySlider("Intensity", pl.intensity, 0.f, 10.f));
							UNDOABLE_COMPONENT_SETTING("point light radius", pl.radius,
								ImGui::PropertySlider("Radius", pl.radius, 0.f, 100.f));
							UNDOABLE_COMPONENT_SETTING("point light casts shadow", pl.castsShadow,
								ImGui::PropertyCheckbox("Casts shadow", pl.castsShadow));
							if (pl.castsShadow)
							{
								UNDOABLE_COMPONENT_SETTING("point light shadow resolution", pl.shadowMapResolution,
									ImGui::PropertyDropdownPowerOfTwo("Shadow resolution", 128, 2048, pl.shadowMapResolution));
							}

							ImGui::EndProperties();
						}
					});

					drawComponent<spot_light_component>(selectedEntity, "SPOT LIGHT", [this](spot_light_component& sl)
					{
						using component_t = spot_light_component;

						if (ImGui::BeginProperties())
						{
							UNDOABLE_COMPONENT_SETTING("spot light color", sl.color,
								ImGui::PropertyColorWheel("Color", sl.color));
							UNDOABLE_COMPONENT_SETTING("spot light intensity", sl.intensity,
								ImGui::PropertySlider("Intensity", sl.intensity, 0.f, 10.f));
							UNDOABLE_COMPONENT_SETTING("spot light distance", sl.distance,
								ImGui::PropertySlider("Distance", sl.distance, 0.f, 100.f));
							UNDOABLE_COMPONENT_SETTING("spot light inner angle", sl.innerAngle,
								ImGui::PropertySliderAngle("Inner angle", sl.innerAngle, 0.1f, 80.f));
							UNDOABLE_COMPONENT_SETTING("spot light outer angle", sl.outerAngle,
								ImGui::PropertySliderAngle("Outer angle", sl.outerAngle, 0.2f, 85.f));
							UNDOABLE_COMPONENT_SETTING("spot light casts shadow", sl.castsShadow,
								ImGui::PropertyCheckbox("Casts shadow", sl.castsShadow));
							if (sl.castsShadow)
							{
								UNDOABLE_COMPONENT_SETTING("spot light shadow resolution", sl.shadowMapResolution,
									ImGui::PropertyDropdownPowerOfTwo("Shadow resolution", 128, 2048, sl.shadowMapResolution));
							}

							ImGui::EndProperties();
						}
					});
				}


				if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered())
				{
					ImGui::OpenPopup("CreateComponentPopup");
				}

				if (ImGui::BeginPopup("CreateComponentPopup"))
				{
					ImGui::Text("Create component");
					ImGui::Separator();

					if (ImGui::MenuItem("Rigid body"))
					{
						selectedEntity.addComponent<rigid_body_component>(false);
					}
					if (ImGui::BeginMenu("Collider"))
					{
						bounding_box aabb = getObjectBoundingBox(selectedEntity, false);
						physics_material material = { physics_material_type_wood, 0.1f, 0.5f, 1.f };

						if (ImGui::MenuItem("Sphere"))
						{
							selectedEntity.addComponent<collider_component>(collider_component::asSphere({ aabb.getCenter(), maxElement(aabb.getRadius()) }, material));
						}
						if (ImGui::MenuItem("Capsule"))
						{
							vec3 r = aabb.getRadius();
							uint32 axis = maxElementIndex(r);
							uint32 axisA = (axis + 1) % 3;
							uint32 axisB = (axis + 2) % 3;
							float cRadius = max(r.data[axisA], r.data[axisB]);
							vec3 a = aabb.minCorner; a.data[axisA] += r.data[axisA]; a.data[axisB] += r.data[axisB]; a.data[axis] += cRadius;
							vec3 b = aabb.maxCorner; b.data[axisA] -= r.data[axisA]; b.data[axisB] -= r.data[axisB]; b.data[axis] -= cRadius;
							selectedEntity.addComponent<collider_component>(collider_component::asCapsule({ a, b, cRadius }, material));
						}
						if (ImGui::MenuItem("Cylinder"))
						{
							vec3 r = aabb.getRadius();
							uint32 axis = maxElementIndex(r);
							uint32 axisA = (axis + 1) % 3;
							uint32 axisB = (axis + 2) % 3;
							float cRadius = max(r.data[axisA], r.data[axisB]);
							vec3 a = aabb.minCorner; a.data[axisA] += r.data[axisA]; a.data[axisB] += r.data[axisB];
							vec3 b = aabb.maxCorner; b.data[axisA] -= r.data[axisA]; b.data[axisB] -= r.data[axisB];
							selectedEntity.addComponent<collider_component>(collider_component::asCylinder({ a, b, cRadius }, material));
						}
						if (ImGui::MenuItem("AABB"))
						{
							selectedEntity.addComponent<collider_component>(collider_component::asAABB(aabb, material));
						}
						if (ImGui::MenuItem("OBB"))
						{
							selectedEntity.addComponent<collider_component>(collider_component::asOBB({ quat::identity, aabb.getCenter(), aabb.getRadius() }, material));
						}

						ImGui::EndMenu();
					}

					ImGui::EndPopup();
				}


			}
			ImGui::EndChild();
		}
	}
	ImGui::End();

	return objectMovedByWidget;
}

void scene_editor::onObjectMoved()
{
	if (selectedEntity)
	{
		if (cloth_component* cloth = selectedEntity.getComponentIfExists<cloth_component>())
		{
			cloth->setWorldPositionOfFixedVertices(selectedEntity.getComponent<transform_component>(), true);
		}
	}
}

bool scene_editor::handleUserInput(const user_input& input, ldr_render_pass* ldrRenderPass, float dt)
{
	game_scene* scene = &this->scene->getCurrentScene();


	// Returns true, if the user dragged an object using a gizmo.

	if (input.keyboard['F'].pressEvent && selectedEntity)
	{
		bounding_box aabb = getObjectBoundingBox(selectedEntity, true);
		cameraController.centerCameraOnObject(aabb);
	}

	bool inputCaptured = cameraController.update(input, renderer->renderWidth, renderer->renderHeight, dt);

	if (inputCaptured)
	{
		renderer->pathTracer.resetRendering();
	}

	bool objectMovedByGizmo = false;


	render_camera& camera = this->scene->camera;

	bool gizmoDrawn = false;


	if (!inputCaptured && !ImGui::IsAnyItemActive() && ImGui::IsKeyDown(key_shift) && ImGui::IsKeyPressed('A'))
	{
		ImGui::OpenPopup("CreateEntityPopup");
		inputCaptured = true;
	}

	inputCaptured |= drawEntityCreationPopup();

	if (ImGui::BeginControlsWindow("##EntityControls", ImVec2(0.f, 0.f), ImVec2(20.f, 90.f)))
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		if (ImGui::Button(ICON_FA_PLUS)) { ImGui::OpenPopup("CreateEntityPopup"); }
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Create entity (Shift+A)"); }
		inputCaptured |= drawEntityCreationPopup();

		ImGui::PopStyleColor();
	}
	ImGui::End();



	collider_component* selectedCollider = selectedColliderEntity ? selectedColliderEntity.getComponentIfExists<collider_component>() : 0;

	bool draggingBefore = gizmo.dragging;

	if (selectedCollider)
	{
		const trs& transform = selectedEntity.hasComponent<transform_component>() ? selectedEntity.getComponent<transform_component>() : trs::identity;
		auto& c = *selectedCollider;
		const vec4 volumeColor(1.f, 1.f, 0.f, 1.f);
		if (c.type == collider_type_sphere)
		{
			if (gizmo.manipulateBoundingSphere(c.sphere, transform, camera, input, !inputCaptured, ldrRenderPass))
			{
				inputCaptured = true;
				objectMovedByGizmo = true;
			}
#if 0
			else if (draggingBefore)
			{
				const trs& origWorldTransform = gizmo.originalTransform;
				bounding_sphere before =
				{
					conjugate(transform.rotation) * (origWorldTransform.position - transform.position),
					origWorldTransform.scale.x
				};
				undoStack.pushAction("sphere collider", component_member_undo<collider_component, bounding_sphere>(selectedColliderEntity, c.sphere, before));
			}
#endif
			gizmoDrawn = true;
			renderWireSphere(transform.rotation * c.sphere.center + transform.position, c.sphere.radius, volumeColor, ldrRenderPass, true);
		}
		else if (c.type == collider_type_capsule)
		{
			if (gizmo.manipulateBoundingCapsule(c.capsule, transform, camera, input, !inputCaptured, ldrRenderPass))
			{
				inputCaptured = true;
				objectMovedByGizmo = true;
			}
#if 0
			else if (draggingBefore)
			{
				const trs& origWorldTransform = gizmo.originalTransform;
				vec3 a = transform.position + transform.rotation * c.capsule.positionA;
				vec3 b = transform.position + transform.rotation * c.capsule.positionB;
				bounding_capsule before =
				{
					conjugate(transform.rotation) * (transformPosition(origWorldTransform, a) - transform.position),
					conjugate(transform.rotation) * (transformPosition(origWorldTransform, b) - transform.position),
					origWorldTransform.scale.x
				};
				undoStack.pushAction("capsule collider", component_member_undo<collider_component, bounding_capsule>(selectedColliderEntity, c.capsule, before));
			}
#endif
			gizmoDrawn = true;
			renderWireCapsule(transform.rotation * c.capsule.positionA + transform.position, transform.rotation * c.capsule.positionB + transform.position,
				c.capsule.radius, volumeColor, ldrRenderPass, true);
		}
		else if (c.type == collider_type_cylinder)
		{
			if (gizmo.manipulateBoundingCylinder(c.cylinder, transform, camera, input, !inputCaptured, ldrRenderPass))
			{
				inputCaptured = true;
				objectMovedByGizmo = true;
			}
			gizmoDrawn = true;
			renderWireCylinder(transform.rotation * c.cylinder.positionA + transform.position, transform.rotation * c.cylinder.positionB + transform.position,
				c.cylinder.radius, volumeColor, ldrRenderPass, true);
		}
		else if (c.type == collider_type_aabb)
		{
			if (gizmo.manipulateBoundingBox(c.aabb, transform, camera, input, !inputCaptured, ldrRenderPass))
			{
				inputCaptured = true;
				objectMovedByGizmo = true;
			}
			gizmoDrawn = true;
			renderWireBox(transform.rotation * c.aabb.getCenter() + transform.position, c.aabb.getRadius(), transform.rotation, volumeColor, ldrRenderPass, true);
		}
		else if (c.type == collider_type_obb)
		{
			if (gizmo.manipulateOrientedBoundingBox(c.obb, transform, camera, input, !inputCaptured, ldrRenderPass))
			{
				inputCaptured = true;
				objectMovedByGizmo = true;
			}
			gizmoDrawn = true;
			renderWireBox(transform.rotation * c.obb.center + transform.position, c.obb.radius, transform.rotation * c.obb.rotation, volumeColor, ldrRenderPass, true);
		}
		if (!gizmoDrawn)
		{
			gizmo.manipulateNothing(camera, input, !inputCaptured, ldrRenderPass);
		}
	}
	else if (selectedEntity)
	{
		if (physics_transform1_component* transform = selectedEntity.getComponentIfExists<physics_transform1_component>())
		{
			// Saved rigid-body properties. When an RB is dragged, we make it kinematic.
			static bool saved = false;
			static float invMass;

			if (gizmo.manipulateTransformation(*transform, camera, input, !inputCaptured, ldrRenderPass))
			{
				updateSelectedEntityUIRotation();
				inputCaptured = true;
				objectMovedByGizmo = true;

				if (!saved && selectedEntity.hasComponent<rigid_body_component>())
				{
					rigid_body_component& rb = selectedEntity.getComponent<rigid_body_component>();
					invMass = rb.invMass;

					rb.invMass = 0.f;
					rb.linearVelocity = 0.f;

					saved = true;
				}

				selectedEntity.getComponent<physics_transform0_component>() = *transform;
				selectedEntity.getComponent<transform_component>() = *transform;
			}
			else
			{
				if (draggingBefore)
				{
					currentUndoStack->pushAction("object transform",
						component_undo<transform_component, physics_transform0_component, physics_transform1_component>(selectedEntity,
							gizmo.originalTransform,
							gizmo.originalTransform,
							gizmo.originalTransform
							));
				}

				if (saved)
				{
					assert(selectedEntity.hasComponent<rigid_body_component>());
					rigid_body_component& rb = selectedEntity.getComponent<rigid_body_component>();

					rb.invMass = invMass;
					saved = false;
				}
			}
		}
		else if (transform_component* transform = selectedEntity.getComponentIfExists<transform_component>())
		{
			if (gizmo.manipulateTransformation(*transform, camera, input, !inputCaptured, ldrRenderPass))
			{
				updateSelectedEntityUIRotation();
				inputCaptured = true;
				objectMovedByGizmo = true;
			}
			else if (draggingBefore)
			{
				currentUndoStack->pushAction("object transform",
					component_undo<transform_component>(selectedEntity, transform_component(gizmo.originalTransform)));
			}
		}
		else if (position_component* pc = selectedEntity.getComponentIfExists<position_component>())
		{
			if (gizmo.manipulatePosition(pc->position, camera, input, !inputCaptured, ldrRenderPass))
			{
				inputCaptured = true;
				objectMovedByGizmo = true;
			}
			else if (draggingBefore)
			{
				currentUndoStack->pushAction("object transform",
					component_undo<position_component>(selectedEntity, position_component{ gizmo.originalTransform.position }));
			}
		}
		else if (position_rotation_component* prc = selectedEntity.getComponentIfExists<position_rotation_component>())
		{
			if (gizmo.manipulatePositionRotation(prc->position, prc->rotation, camera, input, !inputCaptured, ldrRenderPass))
			{
				updateSelectedEntityUIRotation();
				inputCaptured = true;
				objectMovedByGizmo = true;
			}
			else if (draggingBefore)
			{
				currentUndoStack->pushAction("object transform",
					component_undo<position_rotation_component>(selectedEntity, position_rotation_component{ gizmo.originalTransform.position, gizmo.originalTransform.rotation }));
			}
		}
		else if (position_scale_component* psc = selectedEntity.getComponentIfExists<position_scale_component>())
		{
			if (gizmo.manipulatePositionScale(psc->position, psc->scale, camera, input, !inputCaptured, ldrRenderPass))
			{
				inputCaptured = true;
				objectMovedByGizmo = true;
			}
			else if (draggingBefore)
			{
				currentUndoStack->pushAction("object transform",
					component_undo<position_scale_component>(selectedEntity, position_scale_component{ gizmo.originalTransform.position, gizmo.originalTransform.scale }));
			}
		}
		else
		{
			gizmo.manipulateNothing(camera, input, !inputCaptured, ldrRenderPass);
		}

		if (!inputCaptured && !ImGui::IsAnyItemActive())
		{
			if (ImGui::IsKeyPressed(key_backspace) || ImGui::IsKeyPressed(key_delete))
			{
				// Delete entity.
				currentUndoStack->pushAction("entity deletion", entity_existence_undo(*scene, selectedEntity));
				scene->deleteEntity(selectedEntity);
				setSelectedEntity({});
				inputCaptured = true;
				objectMovedByGizmo = true;
			}
			else if (ImGui::IsKeyDown(key_ctrl) && ImGui::IsKeyPressed('D'))
			{
				// Duplicate entity.
				scene_entity newEntity = scene->copyEntity(selectedEntity);
				setSelectedEntity(newEntity);
				inputCaptured = true;
				objectMovedByGizmo = true;
			}
		}
	}
	else
	{
		gizmo.manipulateNothing(camera, input, !inputCaptured, ldrRenderPass);
	}


	float simControlsWidth = IMGUI_ICON_DEFAULT_SIZE * 3 + IMGUI_ICON_DEFAULT_SPACING * 2 + ImGui::GetStyle().WindowPadding.x * 2;

	if (ImGui::BeginControlsWindow("##SimulationControls", ImVec2(0.5f, 0.f), ImVec2(-simControlsWidth * 0.5f, 20.f)))
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		if (ImGui::IconButton(imgui_icon_play, imgui_icon_play, IMGUI_ICON_DEFAULT_SIZE, this->scene->isPlayable()))
		{
			this->scene->play();
			undoStacks[1].reset();
			setSelectedEntity({});
		}
		ImGui::SameLine(0.f, IMGUI_ICON_DEFAULT_SPACING);
		if (ImGui::IconButton(imgui_icon_pause, imgui_icon_pause, IMGUI_ICON_DEFAULT_SIZE, this->scene->isPausable()))
		{
			this->scene->pause();
		}
		ImGui::SameLine(0.f, IMGUI_ICON_DEFAULT_SPACING);
		if (ImGui::IconButton(imgui_icon_stop, imgui_icon_stop, IMGUI_ICON_DEFAULT_SIZE, this->scene->isStoppable()))
		{
			this->scene->stop();
			this->scene->environment.forceUpdate(this->scene->sun.direction);
			setSelectedEntity({});
		}

		scene = &this->scene->getCurrentScene();
		cameraController.camera = &camera;

		currentUndoStack = &undoStacks[this->scene->mode > 0];
		currentUndoBuffer = &undoBuffers[this->scene->mode > 0];

		ImGui::PopStyleColor();
	}
	ImGui::End();


	if (!ImGui::IsAnyItemActive())
	{
		if (!inputCaptured && ImGui::IsKeyDown(key_ctrl) && ImGui::IsKeyPressed('Z'))
		{
			currentUndoStack->undo();
			inputCaptured = true;
			objectMovedByGizmo = true;
		}
		if (!inputCaptured && ImGui::IsKeyDown(key_ctrl) && ImGui::IsKeyPressed('Y'))
		{
			currentUndoStack->redo();
			inputCaptured = true;
			objectMovedByGizmo = true;
		}
		if (!inputCaptured && ImGui::IsKeyDown(key_ctrl) && ImGui::IsKeyPressed('S'))
		{
			serializeToFile();
			inputCaptured = true;
			ImGui::GetIO().KeysDown['S'] = false; // Hack: Window does not get notified of inputs due to the file dialog.
		}
		if (!inputCaptured && ImGui::IsKeyDown(key_ctrl) && ImGui::IsKeyPressed('O'))
		{
			deserializeFromFile();
			inputCaptured = true;
			ImGui::GetIO().KeysDown['O'] = false; // Hack: Window does not get notified of inputs due to the file dialog.
		}
		if (!inputCaptured && ImGui::IsKeyDown(key_ctrl) && ImGui::IsKeyPressed('L'))
		{
			logWindowOpen = !logWindowOpen;
			inputCaptured = true;
		}
	}

	if (!inputCaptured && input.mouse.left.clickEvent)
	{
		// Temporary.
		if (input.keyboard[key_shift].down)
		{
			testPhysicsInteraction(*scene, camera.generateWorldSpaceRay(input.mouse.relX, input.mouse.relY), physicsTestForce);
		}
		else if (input.keyboard[key_ctrl].down)
		{
			vec3 dir = -camera.generateWorldSpaceRay(input.mouse.relX, input.mouse.relY).direction;
			vec3 beforeDir = this->scene->sun.direction;
			this->scene->sun.direction = dir;
			currentUndoStack->pushAction("sun direction", settings_undo<vec3>(this->scene->sun.direction, beforeDir));
			inputCaptured = true;
		}
		else
		{
			if (renderer->hoveredObjectID != -1)
			{
				setSelectedEntity({ renderer->hoveredObjectID, *scene });
			}
			else
			{
				setSelectedEntity({});
			}
		}
		inputCaptured = true;
	}

	return objectMovedByGizmo;
}

bool scene_editor::drawEntityCreationPopup()
{
	game_scene* scene = &this->scene->getCurrentScene();
	render_camera& camera = this->scene->camera;

	bool clicked = false;

	if (ImGui::BeginPopup("CreateEntityPopup"))
	{
		if (ImGui::MenuItem("Point light", "P") || ImGui::IsKeyPressed('P'))
		{
			auto pl = scene->createEntity("Point light")
				.addComponent<position_component>(camera.position + camera.rotation * vec3(0.f, 0.f, -3.f))
				.addComponent<point_light_component>(
					vec3(1.f, 1.f, 1.f),
					1.f,
					10.f,
					false,
					512u
					);

			currentUndoStack->pushAction("entity creation", entity_existence_undo(*scene, pl));

			setSelectedEntity(pl);
			clicked = true;
		}

		if (ImGui::MenuItem("Spot light", "S") || ImGui::IsKeyPressed('S'))
		{
			auto sl = scene->createEntity("Spot light")
				.addComponent<position_rotation_component>(camera.position + camera.rotation * vec3(0.f, 0.f, -3.f), quat::identity)
				.addComponent<spot_light_component>(
					vec3(1.f, 1.f, 1.f),
					1.f,
					25.f,
					deg2rad(20.f),
					deg2rad(30.f),
					false,
					512u
					);

			currentUndoStack->pushAction("entity creation", entity_existence_undo(*scene, sl));

			setSelectedEntity(sl);
			clicked = true;
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Cloth", "C") || ImGui::IsKeyPressed('C'))
		{
			auto cloth = scene->createEntity("Cloth")
				.addComponent<transform_component>(camera.position + camera.rotation * vec3(0.f, 0.f, -3.f), camera.rotation)
				.addComponent<cloth_component>(10.f, 10.f, 20u, 20u, 8.f)
				.addComponent<cloth_render_component>();

			currentUndoStack->pushAction("entity creation", entity_existence_undo(*scene, cloth));

			setSelectedEntity(cloth);
			clicked = true;
		}

		if (ImGui::MenuItem("Humanoid ragdoll", "R") || ImGui::IsKeyPressed('R'))
		{
			auto ragdoll = humanoid_ragdoll::create(*scene, camera.position + camera.rotation * vec3(0.f, 0.f, -3.f));
			setSelectedEntity(ragdoll.torso);
			clicked = true;
		}

		if (ImGui::MenuItem("Vehicle", "V") || ImGui::IsKeyPressed('V'))
		{
			auto vehicle = vehicle::create(*scene, camera.position + camera.rotation * vec3(0.f, 0.f, -4.f));
			setSelectedEntity(vehicle.motor);
			clicked = true;
		}

		if (clicked)
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	return clicked;
}

void scene_editor::serializeToFile()
{
	serializeSceneToYAMLFile(*scene, renderer->settings);
}

bool scene_editor::deserializeFromFile()
{
	std::string environmentName;
	if (deserializeSceneFromYAMLFile(*scene, renderer->settings, environmentName))
	{
		scene->stop();

		setSelectedEntity({});
		scene->environment.setFromTexture(environmentName);
		scene->environment.forceUpdate(this->scene->sun.direction);
		renderer->pathTracer.resetRendering();

		return true;
	}
	return false;
}

bool scene_editor::editCamera(render_camera& camera)
{
	bool result = false;
	if (ImGui::BeginTree("Camera"))
	{
		if (ImGui::BeginProperties())
		{
			UNDOABLE_SETTING("field of view", camera.verticalFOV,
				result |= ImGui::PropertySliderAngle("Field of view", camera.verticalFOV, 1.f, 150.f));
			UNDOABLE_SETTING("near plane", camera.nearPlane,
				result |= ImGui::PropertyDrag("Near plane", camera.nearPlane, 0.01f, 0.f));

			bool infiniteFarplane = camera.farPlane < 0.f;
			UNDOABLE_SETTING("infinite far plane", camera.farPlane,
				if (ImGui::PropertyCheckbox("Infinite far plane", infiniteFarplane))
				{
					if (!infiniteFarplane)
					{
						camera.farPlane = (camera.farPlane == -1.f) ? 500.f : -camera.farPlane;
					}
					else
					{
						camera.farPlane = -camera.farPlane;
					}
					result = true;
				});
			if (!infiniteFarplane)
			{
				UNDOABLE_SETTING("far plane", camera.farPlane,
					result |= ImGui::PropertyDrag("Far plane", camera.farPlane, 0.1f, 0.f));
			}

			ImGui::EndProperties();
		}

		ImGui::EndTree();
	}
	return result;
}

bool scene_editor::editTonemapping(tonemap_settings& tonemap)
{
	bool result = false;
	if (ImGui::BeginTree("Tonemapping"))
	{
		ImGui::PlotLines("",
			[](void* data, int idx)
		{
			float t = idx * 0.01f;
			tonemap_settings& aces = *(tonemap_settings*)data;
			return aces.tonemap(t);
		},
			&tonemap, 100, 0, 0, 0.f, 1.f, ImVec2(250.f, 250.f));

		if (ImGui::BeginProperties())
		{
			UNDOABLE_SETTING("shoulder strength", tonemap.A,
				result |= ImGui::PropertySlider("Shoulder strength", tonemap.A, 0.f, 1.f));
			UNDOABLE_SETTING("linear strength", tonemap.B,
				result |= ImGui::PropertySlider("Linear strength", tonemap.B, 0.f, 1.f));
			UNDOABLE_SETTING("linear angle", tonemap.C,
				result |= ImGui::PropertySlider("Linear angle", tonemap.C, 0.f, 1.f));
			UNDOABLE_SETTING("toe strength", tonemap.D,
				result |= ImGui::PropertySlider("Toe strength", tonemap.D, 0.f, 1.f));
			UNDOABLE_SETTING("tone numerator", tonemap.E,
				result |= ImGui::PropertySlider("Tone numerator", tonemap.E, 0.f, 1.f));
			UNDOABLE_SETTING("toe denominator", tonemap.F,
				result |= ImGui::PropertySlider("Toe denominator", tonemap.F, 0.f, 1.f));
			UNDOABLE_SETTING("linear white", tonemap.linearWhite,
				result |= ImGui::PropertySlider("Linear white", tonemap.linearWhite, 0.f, 100.f));
			UNDOABLE_SETTING("exposure", tonemap.exposure,
				result |= ImGui::PropertySlider("Exposure", tonemap.exposure, -3.f, 3.f));
			ImGui::EndProperties();
		}

		ImGui::EndTree();
	}
	return result;
}

bool scene_editor::editSunShadowParameters(directional_light& sun)
{
	bool result = false;
	if (ImGui::BeginTree("Sun"))
	{
		if (ImGui::BeginProperties())
		{
			UNDOABLE_SETTING("sun intensity", sun.intensity,
				result |= ImGui::PropertySlider("Intensity", sun.intensity, 0.f, 1000.f));
			UNDOABLE_SETTING("sun color", sun.color,
				result |= ImGui::PropertyColorWheel("Color", sun.color));

			UNDOABLE_SETTING("sun shadow resolution", sun.shadowDimensions,
				result |= ImGui::PropertyDropdownPowerOfTwo("Shadow resolution", 128, 2048, sun.shadowDimensions));
			UNDOABLE_SETTING("stabilize", sun.stabilize,
				result |= ImGui::PropertyCheckbox("Stabilize", sun.stabilize));

			UNDOABLE_SETTING("cascade count", sun.numShadowCascades,
				result |= ImGui::PropertySlider("Cascade count", sun.numShadowCascades, 1, 4));

			const float minCascadeDistance = 0.f, maxCascadeDistance = 300.f;
			const float minBias = 0.f, maxBias = 0.0015f;
			const float minBlend = 0.f, maxBlend = 10.f;
			if (sun.numShadowCascades == 1)
			{
				UNDOABLE_SETTING("cascade distance", sun.cascadeDistances.x,
					result |= ImGui::PropertySlider("Distance", sun.cascadeDistances.x, minCascadeDistance, maxCascadeDistance));
				UNDOABLE_SETTING("cascade bias", sun.bias.x,
					result |= ImGui::PropertySlider("Bias", sun.bias.x, minBias, maxBias, "%.6f"));
				UNDOABLE_SETTING("cascade blend distances", sun.blendDistances.x,
					result |= ImGui::PropertySlider("Blend distances", sun.blendDistances.x, minBlend, maxBlend, "%.6f"));
			}
			else if (sun.numShadowCascades == 2)
			{
				UNDOABLE_SETTING("cascade distance", sun.cascadeDistances.xy,
					result |= ImGui::PropertySlider("Distance", sun.cascadeDistances.xy, minCascadeDistance, maxCascadeDistance));
				UNDOABLE_SETTING("cascade bias", sun.bias.xy,
					result |= ImGui::PropertySlider("Bias", sun.bias.xy, minBias, maxBias, "%.6f"));
				UNDOABLE_SETTING("cascade blend distances", sun.blendDistances.xy,
					result |= ImGui::PropertySlider("Blend distances", sun.blendDistances.xy, minBlend, maxBlend, "%.6f"));
			}
			else if (sun.numShadowCascades == 3)
			{
				UNDOABLE_SETTING("cascade distance", sun.cascadeDistances.xyz,
					result |= ImGui::PropertySlider("Distance", sun.cascadeDistances.xyz, minCascadeDistance, maxCascadeDistance));
				UNDOABLE_SETTING("cascade bias", sun.bias.xyz,
					result |= ImGui::PropertySlider("Bias", sun.bias.xyz, minBias, maxBias, "%.6f"));
				UNDOABLE_SETTING("cascade blend distances", sun.blendDistances.xyz,
					result |= ImGui::PropertySlider("Blend distances", sun.blendDistances.xyz, minBlend, maxBlend, "%.6f"));
			}
			else if (sun.numShadowCascades == 4)
			{
				UNDOABLE_SETTING("cascade distance", sun.cascadeDistances,
					result |= ImGui::PropertySlider("Distance", sun.cascadeDistances, minCascadeDistance, maxCascadeDistance));
				UNDOABLE_SETTING("cascade bias", sun.bias,
					result |= ImGui::PropertySlider("Bias", sun.bias, minBias, maxBias, "%.6f"));
				UNDOABLE_SETTING("cascade blend distances", sun.blendDistances,
					result |= ImGui::PropertySlider("Blend distances", sun.blendDistances, minBlend, maxBlend, "%.6f"));
			}

			ImGui::EndProperties();
		}

		ImGui::EndTree();
	}
	return result;
}

bool scene_editor::editAO(bool& enable, hbao_settings& settings, const ref<dx_texture>& aoTexture)
{
	bool result = false;
	if (ImGui::BeginProperties())
	{
		UNDOABLE_SETTING("enable HBAO", enable,
			result |= ImGui::PropertyCheckbox("Enable HBAO", enable));
		if (enable)
		{
			UNDOABLE_SETTING("num AO rays", settings.numRays,
				result |= ImGui::PropertySlider("Num rays", settings.numRays, 1, 16));
			UNDOABLE_SETTING("max num steps per AO ray", settings.maxNumStepsPerRay,
				result |= ImGui::PropertySlider("Max num steps per ray", settings.maxNumStepsPerRay, 1, 16));
			UNDOABLE_SETTING("AO radius", settings.radius,
				result |= ImGui::PropertySlider("Radius", settings.radius, 0.f, 1.f, "%.3fm"));
			UNDOABLE_SETTING("AO strength", settings.strength,
				result |= ImGui::PropertySlider("Strength", settings.strength, 0.f, 2.f));
		}
		ImGui::EndProperties();
	}
	if (enable && aoTexture && ImGui::BeginTree("Show##ShowAO"))
	{
		ImGui::Image(aoTexture);
		ImGui::EndTree();
	}
	return result;
}

bool scene_editor::editSSS(bool& enable, sss_settings& settings, const ref<dx_texture>& sssTexture)
{
	bool result = false;
	if (ImGui::BeginProperties())
	{
		UNDOABLE_SETTING("enable SSS", enable,
			result |= ImGui::PropertyCheckbox("Enable SSS", enable));
		if (enable)
		{
			UNDOABLE_SETTING("num SSS iterations", settings.numSteps,
				result |= ImGui::PropertySlider("Num iterations", settings.numSteps, 1, 64));
			UNDOABLE_SETTING("SSS ray distance", settings.rayDistance,
				result |= ImGui::PropertySlider("Ray distance", settings.rayDistance, 0.05f, 3.f, "%.3fm"));
			UNDOABLE_SETTING("SSS thickness", settings.thickness,
				result |= ImGui::PropertySlider("Thickness", settings.thickness, 0.05f, 1.f, "%.3fm"));
			UNDOABLE_SETTING("SSS max distance from camera", settings.maxDistanceFromCamera,
				result |= ImGui::PropertySlider("Max distance from camera", settings.maxDistanceFromCamera, 5.f, 1000.f, "%.3fm"));
			UNDOABLE_SETTING("SSS distance fadeout range", settings.distanceFadeoutRange,
				result |= ImGui::PropertySlider("Distance fadeout range", settings.distanceFadeoutRange, 1.f, 5.f, "%.3fm"));
			UNDOABLE_SETTING("SSS border fadeout", settings.borderFadeout,
				result |= ImGui::PropertySlider("Border fadeout", settings.borderFadeout, 0.f, 0.5f));
		}
		ImGui::EndProperties();
	}
	if (enable && sssTexture && ImGui::BeginTree("Show##ShowSSS"))
	{
		ImGui::Image(sssTexture);
		ImGui::EndTree();
	}
	return result;
}

bool scene_editor::editSSR(bool& enable, ssr_settings& settings, const ref<dx_texture>& ssrTexture)
{
	bool result = false;
	if (ImGui::BeginProperties())
	{
		UNDOABLE_SETTING("enable SSR", enable,
			result |= ImGui::PropertyCheckbox("Enable SSR", enable));
		if (enable)
		{
			UNDOABLE_SETTING("num SSR iterations", settings.numSteps,
				result |= ImGui::PropertySlider("Num iterations", settings.numSteps, 1, 1024));
			UNDOABLE_SETTING("SSR max distance", settings.maxDistance,
				result |= ImGui::PropertySlider("Max distance", settings.maxDistance, 5.f, 1000.f, "%.3fm"));
			UNDOABLE_SETTING("SSR min stride", settings.minStride,
				result |= ImGui::PropertySlider("Min stride", settings.minStride, 1.f, 50.f, "%.3fm"));
			UNDOABLE_SETTING("SSR max stride", settings.maxStride,
				result |= ImGui::PropertySlider("Max stride", settings.maxStride, settings.minStride, 50.f, "%.3fm"));
		}
		ImGui::EndProperties();
	}
	if (enable && ssrTexture && ImGui::BeginTree("Show##ShowSSR"))
	{
		ImGui::Image(ssrTexture);
		ImGui::EndTree();
	}
	return result;
}

bool scene_editor::editTAA(bool& enable, taa_settings& settings, const ref<dx_texture>& velocityTexture)
{
	bool result = false;
	if (ImGui::BeginProperties())
	{
		UNDOABLE_SETTING("enable TAA", enable,
			result |= ImGui::PropertyCheckbox("Enable TAA", enable));
		if (enable)
		{
			UNDOABLE_SETTING("TAA jitter strength", settings.cameraJitterStrength,
				result |= ImGui::PropertySlider("Jitter strength", settings.cameraJitterStrength));
		}
		ImGui::EndProperties();
	}
	if (enable && velocityTexture && ImGui::BeginTree("Show##ShowTAA"))
	{
		ImGui::Image(velocityTexture);
		ImGui::EndTree();
	}
	return result;
}

bool scene_editor::editBloom(bool& enable, bloom_settings& settings, const ref<dx_texture>& bloomTexture)
{
	bool result = false;
	if (ImGui::BeginProperties())
	{
		UNDOABLE_SETTING("enable bloom", enable,
			result |= ImGui::PropertyCheckbox("Enable bloom", enable));
		if (enable)
		{
			UNDOABLE_SETTING("bloom threshold", settings.threshold,
				result |= ImGui::PropertySlider("Bloom threshold", settings.threshold, 0.5f, 100.f));
			UNDOABLE_SETTING("bloom strength", settings.strength,
				result |= ImGui::PropertySlider("Bloom strength", settings.strength));
		}
		ImGui::EndProperties();
	}
	if (enable && bloomTexture && ImGui::BeginTree("Show##ShowBloom"))
	{
		ImGui::Image(bloomTexture);
		ImGui::EndTree();
	}
	return result;
}

bool scene_editor::editSharpen(bool& enable, sharpen_settings& settings)
{
	bool result = false;
	if (ImGui::BeginProperties())
	{
		UNDOABLE_SETTING("enable sharpen", enable,
			result |= ImGui::PropertyCheckbox("Enable sharpen", enable));
		if (enable)
		{
			UNDOABLE_SETTING("sharpen strength", settings.strength,
				result |= ImGui::PropertySlider("Sharpen strength", settings.strength));
		}
		ImGui::EndProperties();
	}
	return result;
}

void scene_editor::drawSettings(float dt)
{
	game_scene* scene = &this->scene->getCurrentScene();

	if (ImGui::Begin("Settings"))
	{
		path_tracer& pathTracer = renderer->pathTracer;

		ImGui::Text("%.3f ms, %u FPS", dt * 1000.f, (uint32)(1.f / dt));

		if (ImGui::BeginProperties())
		{
			UNDOABLE_SETTING("time scale", this->scene->timestepScale,
				ImGui::PropertySlider("Time scale", this->scene->timestepScale));

			UNDOABLE_SETTING("renderer mode", renderer->mode,
				if (ImGui::PropertyDropdown("Renderer mode", rendererModeNames, renderer_mode_count, (uint32&)renderer->mode))
				{
					pathTracer.resetRendering();
				});

			dx_memory_usage memoryUsage = dxContext.getMemoryUsage();

			ImGui::PropertyValue("Video memory usage", "%u / %uMB", memoryUsage.currentlyUsed, memoryUsage.available);
			//ImGui::PropertyValue("Running command lists", "%u", dxContext.renderQueue.numRunningCommandLists);

			UNDOABLE_SETTING("aspect ratio", renderer->aspectRatioMode,
				ImGui::PropertyDropdown("Aspect ratio", aspectRatioNames, aspect_ratio_mode_count, (uint32&)renderer->aspectRatioMode));

			UNDOABLE_SETTING("static shadow map caching", enableStaticShadowMapCaching,
				ImGui::PropertyCheckbox("Static shadow map caching", enableStaticShadowMapCaching));

			ImGui::EndProperties();
		}

		editCamera(this->scene->camera);
		editTonemapping(renderer->settings.tonemapSettings);
		editSunShadowParameters(this->scene->sun);

		if (ImGui::BeginTree("Post processing"))
		{
			if (renderer->spec.allowAO) { editAO(renderer->settings.enableAO, renderer->settings.aoSettings, renderer->getAOResult()); ImGui::Separator(); }
			if (renderer->spec.allowSSS) { editSSS(renderer->settings.enableSSS, renderer->settings.sssSettings, renderer->getSSSResult()); ImGui::Separator(); }
			if (renderer->spec.allowSSR) { editSSR(renderer->settings.enableSSR, renderer->settings.ssrSettings, renderer->getSSRResult()); ImGui::Separator(); }
			if (renderer->spec.allowTAA) { editTAA(renderer->settings.enableTAA, renderer->settings.taaSettings, renderer->getScreenVelocities()); ImGui::Separator(); }
			if (renderer->spec.allowBloom) { editBloom(renderer->settings.enableBloom, renderer->settings.bloomSettings, renderer->getBloomResult()); ImGui::Separator(); }
			editSharpen(renderer->settings.enableSharpen, renderer->settings.sharpenSettings);

			ImGui::EndTree();
		}

		if (ImGui::BeginTree("Environment"))
		{
			auto& environment = this->scene->environment;

			if (ImGui::BeginProperties())
			{
				asset_handle handle = environment.handle;
				if (ImGui::PropertyAssetHandle("Texture source", EDITOR_ICON_IMAGE_HDR, handle, "Make proc"))
				{
					if (handle)
					{
						environment.setFromTexture(getPathFromAssetHandle(handle));
					}
					else
					{
						environment.setToProcedural(this->scene->sun.direction);
					}
				}

				UNDOABLE_SETTING("sky intensity", environment.skyIntensity,
					ImGui::PropertySlider("Sky intensity", environment.skyIntensity, 0.f, 2.f));
				UNDOABLE_SETTING("GI intensity", environment.globalIlluminationIntensity,
					ImGui::PropertySlider("GI intensity", environment.globalIlluminationIntensity, 0.f, 2.f));

				UNDOABLE_SETTING("GI mode", environment.giMode,
					ImGui::PropertyDropdown("GI mode", environmentGIModeNames, 1 + dxContext.featureSupport.raytracing(), (uint32&)environment.giMode));

				ImGui::EndProperties();
			}

			if (environment.giMode == environment_gi_raytraced)
			{
				if (ImGui::BeginTree("Light probe"))
				{
					auto& grid = environment.lightProbeGrid;

					if (ImGui::BeginProperties())
					{
						UNDOABLE_SETTING("visualize probes", grid.visualizeProbes,
							ImGui::PropertyCheckbox("Visualize probes", grid.visualizeProbes));
						UNDOABLE_SETTING("visualize rays", grid.visualizeRays,
							ImGui::PropertyCheckbox("Visualize rays", grid.visualizeRays));

						UNDOABLE_SETTING("auto rotate rays", grid.autoRotateRays,
							ImGui::PropertyCheckbox("Auto rotate rays", grid.autoRotateRays));
						if (!grid.autoRotateRays)
						{
							grid.rotateRays = ImGui::PropertyButton("Rotate", "Go");
						}

						ImGui::EndProperties();
					}

					if (ImGui::BeginTree("Irradiance"))
					{
						if (ImGui::BeginProperties())
						{
							UNDOABLE_SETTING("irradiance UI scale", grid.irradianceUIScale,
								ImGui::PropertySlider("Scale", grid.irradianceUIScale, 0.1f, 20.f));
							ImGui::EndProperties();
						}
						ImGui::Image(grid.irradiance, (uint32)(grid.irradiance->width * grid.irradianceUIScale));
						ImGui::EndTree();
					}
					if (ImGui::BeginTree("Depth"))
					{
						if (ImGui::BeginProperties())
						{
							UNDOABLE_SETTING("depth UI scale", grid.depthUIScale,
								ImGui::PropertySlider("Scale", grid.depthUIScale, 0.1f, 20.f));
							ImGui::EndProperties();
						}
						ImGui::Image(grid.depth, (uint32)(grid.depth->width * grid.depthUIScale));
						ImGui::EndTree();
					}
					if (ImGui::BeginTree("Raytraced radiance"))
					{
						if (ImGui::BeginProperties())
						{
							UNDOABLE_SETTING("raytraced radiance UI scale", grid.raytracedRadianceUIScale,
								ImGui::PropertySlider("Scale", grid.raytracedRadianceUIScale, 0.1f, 20.f));
							ImGui::EndProperties();
						}
						ImGui::Image(grid.raytracedRadiance, (uint32)(grid.raytracedRadiance->width * grid.raytracedRadianceUIScale));
						ImGui::EndTree();
					}

					ImGui::EndTree();
				}
			}


			ImGui::EndTree();
		}

		if (ImGui::BeginTree("Physics"))
		{
			if (ImGui::BeginProperties())
			{
				UNDOABLE_SETTING("fixed frame rate", physicsSettings.fixedFrameRate,
					ImGui::PropertyCheckbox("Fixed frame rate (deterministic)", physicsSettings.fixedFrameRate));

				if (physicsSettings.fixedFrameRate)
				{
					UNDOABLE_SETTING("frame rate", physicsSettings.frameRate,
						ImGui::PropertyInput("Frame rate", physicsSettings.frameRate));
					if (physicsSettings.frameRate < 30)
					{
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));
						ImGui::PropertyValue("", "Low frame rate");
						ImGui::PopStyleColor();
					}

					UNDOABLE_SETTING("max physics steps per frame", physicsSettings.maxPhysicsIterationsPerFrame,
						ImGui::PropertyDrag("Max physics steps per frame", physicsSettings.maxPhysicsIterationsPerFrame));
				}

				UNDOABLE_SETTING("rigid solver iterations", physicsSettings.numRigidSolverIterations,
					ImGui::PropertySlider("Rigid solver iterations", physicsSettings.numRigidSolverIterations, 1, 200));

				UNDOABLE_SETTING("cloth velocity iterations", physicsSettings.numClothVelocityIterations,
					ImGui::PropertySlider("Cloth velocity iterations", physicsSettings.numClothVelocityIterations, 0, 10));
				UNDOABLE_SETTING("cloth position iterations", physicsSettings.numClothPositionIterations,
					ImGui::PropertySlider("Cloth position iterations", physicsSettings.numClothPositionIterations, 0, 10));
				UNDOABLE_SETTING("cloth drift iterations", physicsSettings.numClothDriftIterations,
					ImGui::PropertySlider("Cloth drift iterations", physicsSettings.numClothDriftIterations, 0, 10));

				UNDOABLE_SETTING("test force", physicsTestForce,
					ImGui::PropertySlider("Test force", physicsTestForce, 1.f, 10000.f));

				UNDOABLE_SETTING("SIMD broad phase", physicsSettings.simdBroadPhase,
					ImGui::PropertyCheckbox("SIMD broad phase", physicsSettings.simdBroadPhase));
				UNDOABLE_SETTING("SIMD narrow phase", physicsSettings.simdNarrowPhase,
					ImGui::PropertyCheckbox("SIMD narrow phase", physicsSettings.simdNarrowPhase));
				UNDOABLE_SETTING("SIMD constraint solver", physicsSettings.simdConstraintSolver,
					ImGui::PropertyCheckbox("SIMD constraint solver", physicsSettings.simdConstraintSolver));

				ImGui::EndProperties();
			}
			ImGui::EndTree();
		}

		if (ImGui::BeginTree("Audio"))
		{
			bool change = false;
			if (ImGui::BeginProperties())
			{
				const float maxVolume = 3.f;
				UNDOABLE_SETTING("master volume", masterAudioSettings.volume,
					change |= ImGui::PropertySlider("Master volume", masterAudioSettings.volume, 0.f, maxVolume));

				ImGui::PropertySeparator();

				for (uint32 i = 0; i < sound_type_count; ++i)
				{
					UNDOABLE_SETTING(soundTypeNames[i], soundTypeVolumes[i],
						change |= ImGui::PropertySlider(soundTypeNames[i], soundTypeVolumes[i], 0.f, 1.f));
				}

				ImGui::PropertySeparator();

				UNDOABLE_SETTING("toggle reverb", masterAudioSettings.reverbEnabled,
					ImGui::PropertyCheckbox("Reverb enabled", masterAudioSettings.reverbEnabled));

				if (masterAudioSettings.reverbEnabled)
				{
					UNDOABLE_SETTING("reverb preset", masterAudioSettings.reverbPreset,
						change |= ImGui::PropertyDropdown("Reverb preset", reverbPresetNames, reverb_preset_count, (uint32&)masterAudioSettings.reverbPreset));
				}

				ImGui::EndProperties();
			}
			ImGui::EndTree();
		}

		if (renderer->mode == renderer_mode_pathtraced)
		{
			if (ImGui::BeginProperties())
			{
				auto& settings = pathTracer.settings;

				UNDOABLE_SETTING("max recursion depth", settings.recursionDepth,
					ImGui::PropertySlider("Max recursion depth", settings.recursionDepth, 0, settings.maxRecursionDepth - 1));
				UNDOABLE_SETTING("start russion roulette after", settings.startRussianRouletteAfter,
					ImGui::PropertySlider("Start russian roulette after", settings.startRussianRouletteAfter, 0, settings.recursionDepth));
				UNDOABLE_SETTING("use thin lens camera", settings.useThinLensCamera,
					ImGui::PropertyCheckbox("Use thin lens camera", settings.useThinLensCamera));
				if (settings.useThinLensCamera)
				{
					UNDOABLE_SETTING("focal length", settings.focalLength,
						ImGui::PropertySlider("Focal length", settings.focalLength, 0.5f, 50.f));
					UNDOABLE_SETTING("f-number", settings.fNumber,
						ImGui::PropertySlider("F-Number", settings.fNumber, 1.f, 128.f));
				}
				UNDOABLE_SETTING("use real materials", settings.useRealMaterials,
					ImGui::PropertyCheckbox("Use real materials", settings.useRealMaterials));
				UNDOABLE_SETTING("enable direct lighting", settings.enableDirectLighting,
					ImGui::PropertyCheckbox("Enable direct lighting", settings.enableDirectLighting));
				if (settings.enableDirectLighting)
				{
					UNDOABLE_SETTING("light intensity scale", settings.lightIntensityScale,
						ImGui::PropertySlider("Light intensity scale", settings.lightIntensityScale, 0.f, 50.f));
					UNDOABLE_SETTING("point light radius", settings.pointLightRadius,
						ImGui::PropertySlider("Point light radius", settings.pointLightRadius, 0.01f, 1.f));

					UNDOABLE_SETTING("multiple importance sampling", settings.multipleImportanceSampling,
						ImGui::PropertyCheckbox("Multiple importance sampling", settings.multipleImportanceSampling));
				}

				ImGui::EndProperties();
			}
		}
		else
		{
			//if (ImGui::BeginTree("Particle systems"))
			//{
			//	editFireParticleSystem(fireParticleSystem);
			//	editBoidParticleSystem(boidParticleSystem);
			//
			//	ImGui::EndTree();
			//}
		}
	}

	ImGui::End();
}




