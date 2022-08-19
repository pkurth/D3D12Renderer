#include "pch.h"
#include "editor.h"
#include "core/imgui.h"
#include "core/cpu_profiling.h"
#include "core/log.h"
#include "dx/dx_profiling.h"
#include "scene/components.h"
#include "animation/animation.h"
#include "geometry/mesh.h"
#include "physics/physics.h"
#include "physics/ragdoll.h"
#include "physics/vehicle.h"
#include "scene/serialization.h"
#include "audio/audio.h"

#include <fontawesome/list.h>

struct transform_undo
{
	scene_entity entity;
	trs before;
	trs after;

	void undo() { entity.getComponent<transform_component>() = before; }
	void redo() { entity.getComponent<transform_component>() = after; }
};

struct selection_undo
{
	scene_editor* editor;
	scene_entity before;
	scene_entity after;

	void undo() { editor->setSelectedEntityNoUndo(before); }
	void redo() { editor->setSelectedEntityNoUndo(after); }
};

struct sun_direction_undo
{
	directional_light* sun;
	vec3 before;
	vec3 after;

	void undo() { sun->direction = before; }
	void redo() { sun->direction = after; }
};

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

		selectedEntityEulerRotation = quatToEuler(rotation);
		selectedEntityEulerRotation.x = rad2deg(angleToZeroToTwoPi(selectedEntityEulerRotation.x));
		selectedEntityEulerRotation.y = rad2deg(angleToZeroToTwoPi(selectedEntityEulerRotation.y));
		selectedEntityEulerRotation.z = rad2deg(angleToZeroToTwoPi(selectedEntityEulerRotation.z));
	}
}

void scene_editor::setSelectedEntity(scene_entity entity)
{
	if (selectedEntity != entity)
	{
		undoStack.pushAction("selection", selection_undo{ this, selectedEntity, entity });
	}

	setSelectedEntityNoUndo(entity);
}

void scene_editor::setSelectedEntityNoUndo(scene_entity entity)
{
	selectedEntity = entity;
	updateSelectedEntityUIRotation();
}

void scene_editor::initialize(editor_scene* scene, main_renderer* renderer)
{
	this->scene = scene;
	this->renderer = renderer;
	cameraController.initialize(&scene->camera);

	systemInfo = getSystemInfo();
}

bool scene_editor::update(const user_input& input, ldr_render_pass* ldrRenderPass, float dt)
{
	CPU_PROFILE_BLOCK("Update editor");

	if (selectedEntity && !scene->getCurrentScene().isEntityValid(selectedEntity))
	{
		setSelectedEntityNoUndo({});
	}

	bool objectDragged = false;
	objectDragged |= handleUserInput(input, ldrRenderPass, dt);
	objectDragged |= drawSceneHierarchy();
	drawMainMenuBar();
	drawSettings(dt);

	return objectDragged;
}

void scene_editor::drawMainMenuBar()
{
	static bool showIconsWindow = false;
	static bool showDemoWindow = false;
	static bool showSystemWindow = false;

	bool controlsClicked = false;
	bool aboutClicked = false;
	bool systemClicked = false;

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu(ICON_FA_FILE "  File"))
		{
			char textBuffer[128];
			snprintf(textBuffer, sizeof(textBuffer), ICON_FA_UNDO " Undo %s", undoStack.undoPossible() ? undoStack.getUndoName() : "");
			if (ImGui::MenuItem(textBuffer, "Ctrl+Z", false, undoStack.undoPossible()))
			{
				undoStack.undo();
			}

			snprintf(textBuffer, sizeof(textBuffer), ICON_FA_REDO " Redo %s", undoStack.redoPossible() ? undoStack.getRedoName() : "");
			if (ImGui::MenuItem(textBuffer, "Ctrl+Y", false, undoStack.redoPossible()))
			{
				undoStack.redo();
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
			if (ImGui::MenuItem(showIconsWindow ? (ICON_FA_ICONS "  Hide available icons") : (ICON_FA_ICONS "  Show available icons")))
			{
				showIconsWindow = !showIconsWindow;
			}

			if (ImGui::MenuItem(showDemoWindow ? (ICON_FA_PUZZLE_PIECE "  Hide demo window") : (ICON_FA_PUZZLE_PIECE "  Show demo window")))
			{
				showDemoWindow = !showDemoWindow;
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

			if (ImGui::MenuItem(soundEditorWindowOpen ? (ICON_FA_VOLUME_UP "  Hide sound editor") : (ICON_FA_VOLUME_UP "  Show sound editor")))
			{
				soundEditorWindowOpen = !soundEditorWindowOpen;
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
		ImGui::Text("Direct3D renderer");
		ImGui::Separator();

		ImGui::PopupOkButton();
		ImGui::EndPopup();
	}


	if (showIconsWindow)
	{
		ImGui::Begin(ICON_FA_ICONS "  Icons", &showIconsWindow);

		static ImGuiTextFilter filter;
		filter.Draw();

		ImGui::BeginChild("Icons List");
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
		ImGui::EndChild();
		ImGui::End();
	}

	if (showDemoWindow)
	{
		ImGui::ShowDemoWindow(&showDemoWindow);
	}

	if (soundEditorWindowOpen)
	{
		drawSoundEditor();
	}
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
				const char* name = tag.name;
				scene_entity entity = { entityHandle, scene };

				if (entity == selectedEntity)
				{
					ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), name);
				}
				else
				{
					ImGui::Text(name);
				}

				if (ImGui::IsItemClicked(0) || ImGui::IsItemClicked(1))
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
					scene.deleteEntity(entity);
					setSelectedEntityNoUndo({});
				}
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
				ImGui::InputText("Name", selectedEntity.getComponent<tag_component>().name, sizeof(tag_component::name));
				ImGui::PopID();
				ImGui::SameLine();
				if (ImGui::Button(ICON_FA_TRASH_ALT))
				{
					scene.deleteEntity(selectedEntity);
					setSelectedEntityNoUndo({});
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
						objectMovedByWidget |= ImGui::DragFloat3("Position", transform.position.data, 0.1f, 0.f, 0.f);

						if (ImGui::DragFloat3("Rotation", selectedEntityEulerRotation.data, 0.1f, 0.f, 0.f))
						{
							vec3 euler = selectedEntityEulerRotation;
							euler.x = deg2rad(euler.x);
							euler.y = deg2rad(euler.y);
							euler.z = deg2rad(euler.z);
							transform.rotation = eulerToQuat(euler);

							objectMovedByWidget = true;
						}

						objectMovedByWidget |= ImGui::DragFloat3("Scale", transform.scale.data, 0.1f, 0.f, 0.f);
					});

					drawComponent<position_component>(selectedEntity, "TRANSFORM", [&objectMovedByWidget](position_component& position)
					{
						objectMovedByWidget |= ImGui::DragFloat3("Position", position.position.data, 0.1f, 0.f, 0.f);
					});

					drawComponent<position_rotation_component>(selectedEntity, "TRANSFORM", [this, &objectMovedByWidget](position_rotation_component& pr)
					{
						objectMovedByWidget |= ImGui::DragFloat3("Translation", pr.position.data, 0.1f, 0.f, 0.f);
						if (ImGui::DragFloat3("Rotation", selectedEntityEulerRotation.data, 0.1f, 0.f, 0.f))
						{
							vec3 euler = selectedEntityEulerRotation;
							euler.x = deg2rad(euler.x);
							euler.y = deg2rad(euler.y);
							euler.z = deg2rad(euler.z);
							pr.rotation = eulerToQuat(euler);

							objectMovedByWidget = true;
						}
					});

					drawComponent<dynamic_transform_component>(selectedEntity, "DYNAMIC", [](dynamic_transform_component& dynamic)
					{
						ImGui::Text("Dynamic");
					});

					drawComponent<animation_component>(selectedEntity, "ANIMATION", [this](animation_component& anim)
					{
						if (raster_component* raster = selectedEntity.getComponentIfExists<raster_component>())
						{
							if (ImGui::BeginProperties())
							{
								uint32 animationIndex = anim.animation.clip ? (uint32)(anim.animation.clip - raster->mesh->skeleton.clips.data()) : -1;

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
								}, animationIndex, &raster->mesh->skeleton);

								if (animationChanged)
								{
									anim.animation.set(&raster->mesh->skeleton.clips[animationIndex]);
								}

								ImGui::EndProperties();
							}
						}
					});

					drawComponent<rigid_body_component>(selectedEntity, "RIGID BODY", [this, &scene](rigid_body_component& rb)
					{
						if (ImGui::BeginProperties())
						{
							bool kinematic = rb.invMass == 0;
							if (ImGui::PropertyCheckbox("Kinematic", kinematic))
							{
								if (kinematic)
								{
									rb.invMass = 0.f;
									rb.invInertia = mat3::zero;
									rb.linearVelocity = vec3(0.f);
									rb.angularVelocity = vec3(0.f);
									rb.forceAccumulator = vec3(0.f);
									rb.torqueAccumulator = vec3(0.f);
								}
								else
								{
									rb.invMass = 1.f;
									rb.invInertia = mat3::identity;

									if (physics_reference_component* ref = selectedEntity.getComponentIfExists<physics_reference_component>())
									{
										rb.recalculateProperties(&scene.registry, *ref);
									}
								}
							}

							if (!kinematic)
							{
								ImGui::PropertyValue("Mass", 1.f / rb.invMass, "%.3fkg");
							}
							ImGui::PropertySlider("Linear velocity damping", rb.linearDamping);
							ImGui::PropertySlider("Angular velocity damping", rb.angularDamping);
							ImGui::PropertySlider("Gravity factor", rb.gravityFactor);

							//ImGui::PropertyValue("Linear velocity", rb.linearVelocity);
							//ImGui::PropertyValue("Angular velocity", rb.angularVelocity);

							ImGui::EndProperties();
						}
					});

					drawComponent<physics_reference_component>(selectedEntity, "COLLIDERS", [this, &scene](physics_reference_component& reference)
					{
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
												dirty |= ImGui::PropertyInput("Local center", collider.sphere.center);
												dirty |= ImGui::PropertyInput("Radius", collider.sphere.radius);
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
												dirty |= ImGui::PropertyInput("Local point A", collider.capsule.positionA);
												dirty |= ImGui::PropertyInput("Local point B", collider.capsule.positionB);
												dirty |= ImGui::PropertyInput("Radius", collider.capsule.radius);
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
												dirty |= ImGui::PropertyInput("Local min", collider.aabb.minCorner);
												dirty |= ImGui::PropertyInput("Local max", collider.aabb.maxCorner);
												ImGui::EndProperties();
											}
											ImGui::EndTree();
										}
									} break;

									// TODO: UI for remaining collider types.
								}

								if (ImGui::BeginProperties())
								{
									ImGui::PropertySlider("Restitution", collider.material.restitution);
									ImGui::PropertySlider("Friction", collider.material.friction);
									dirty |= ImGui::PropertyInput("Density", collider.material.density);

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
											ImGui::EndProperties();
										}
									});
								} break;
							}

							ImGui::PopID();
						}
					});

					drawComponent<cloth_component>(selectedEntity, "CLOTH", [](cloth_component& cloth)
					{
						bool dirty = false;
						if (ImGui::BeginProperties())
						{
							dirty |= ImGui::PropertyInput("Total mass", cloth.totalMass);
							dirty |= ImGui::PropertySlider("Stiffness", cloth.stiffness, 0.01f, 0.7f);

							// These two don't need to notify the cloth on change.
							ImGui::PropertySlider("Velocity damping", cloth.damping, 0.f, 1.f);
							ImGui::PropertySlider("Gravity factor", cloth.gravityFactor, 0.f, 1.f);

							ImGui::EndProperties();
						}

						if (dirty)
						{
							cloth.recalculateProperties();
						}
					});

					drawComponent<point_light_component>(selectedEntity, "POINT LIGHT", [](point_light_component& pl)
					{
						if (ImGui::BeginProperties())
						{
							ImGui::PropertyColor("Color", pl.color);
							ImGui::PropertySlider("Intensity", pl.intensity, 0.f, 10.f);
							ImGui::PropertySlider("Radius", pl.radius, 0.f, 100.f);
							ImGui::PropertyCheckbox("Casts shadow", pl.castsShadow);
							if (pl.castsShadow)
							{
								ImGui::PropertyDropdownPowerOfTwo("Shadow resolution", 128, 2048, pl.shadowMapResolution);
							}

							ImGui::EndProperties();
						}
					});

					drawComponent<spot_light_component>(selectedEntity, "SPOT LIGHT", [](spot_light_component& sl)
					{
						if (ImGui::BeginProperties())
						{
							float inner = rad2deg(sl.innerAngle);
							float outer = rad2deg(sl.outerAngle);

							ImGui::PropertyColor("Color", sl.color);
							ImGui::PropertySlider("Intensity", sl.intensity, 0.f, 10.f);
							ImGui::PropertySlider("Distance", sl.distance, 0.f, 100.f);
							ImGui::PropertySlider("Inner angle", inner, 0.1f, 80.f);
							ImGui::PropertySlider("Outer angle", outer, 0.2f, 85.f);
							ImGui::PropertyCheckbox("Casts shadow", sl.castsShadow);
							if (sl.castsShadow)
							{
								ImGui::PropertyDropdownPowerOfTwo("Shadow resolution", 128, 2048, sl.shadowMapResolution);
							}

							sl.innerAngle = deg2rad(inner);
							sl.outerAngle = deg2rad(outer);

							ImGui::EndProperties();
						}
					});

					if (objectMovedByWidget)
					{
						if (cloth_component* cloth = selectedEntity.getComponentIfExists<cloth_component>())
						{
							cloth->setWorldPositionOfFixedVertices(selectedEntity.getComponent<transform_component>(), true);
						}
					}
				}
			}
			ImGui::EndChild();
		}
	}
	ImGui::End();

	return objectMovedByWidget;
}

bool scene_editor::handleUserInput(const user_input& input, ldr_render_pass* ldrRenderPass, float dt)
{
	game_scene* scene = &this->scene->getCurrentScene();

	// Returns true, if the user dragged an object using a gizmo.

	if (input.keyboard['F'].pressEvent && selectedEntity && selectedEntity.hasComponent<transform_component>())
	{
		auto& transform = selectedEntity.getComponent<transform_component>();

		auto aabb = selectedEntity.hasComponent<raster_component>() ? selectedEntity.getComponent<raster_component>().mesh->aabb : bounding_box::fromCenterRadius(0.f, 1.f);
		aabb.minCorner *= transform.scale;
		aabb.maxCorner *= transform.scale;

		aabb.minCorner += transform.position;
		aabb.maxCorner += transform.position;

		cameraController.centerCameraOnObject(aabb);
	}

	bool inputCaptured = cameraController.update(input, renderer->renderWidth, renderer->renderHeight, dt);

	if (inputCaptured)
	{
		renderer->pathTracer.resetRendering();
	}

	bool objectMovedByGizmo = false;


	render_camera& camera = this->scene->camera;

	if (selectedEntity)
	{
		if (physics_transform1_component* transform = selectedEntity.getComponentIfExists<physics_transform1_component>())
		{
			// Saved rigid-body properties. When an RB is dragged, we make it kinematic.
			static bool saved = false;
			static float invMass;

			bool draggingBefore = gizmo.dragging;

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

				if (cloth_component* cloth = selectedEntity.getComponentIfExists<cloth_component>())
				{
					cloth->setWorldPositionOfFixedVertices(*transform, input.keyboard[key_shift].down);
				}
			}
		}
		else if (position_component* pc = selectedEntity.getComponentIfExists<position_component>())
		{
			if (gizmo.manipulatePosition(pc->position, camera, input, !inputCaptured, ldrRenderPass))
			{
				inputCaptured = true;
			}
		}
		else if (position_rotation_component* prc = selectedEntity.getComponentIfExists<position_rotation_component>())
		{
			if (gizmo.manipulatePositionRotation(prc->position, prc->rotation, camera, input, !inputCaptured, ldrRenderPass))
			{
				inputCaptured = true;
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


	if (!inputCaptured && !ImGui::IsAnyItemActive() && ImGui::IsKeyDown(key_shift) && ImGui::IsKeyPressed('A'))
	{
		ImGui::OpenPopup("CreateEntityPopup");
		inputCaptured = true;
	}

	drawEntityCreationPopup();

	if (ImGui::BeginControlsWindow("##EntityControls"))
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		if (ImGui::Button(ICON_FA_PLUS)) { ImGui::OpenPopup("CreateEntityPopup"); }
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Create entity (Shift+A)"); }
		drawEntityCreationPopup();

		ImGui::PopStyleColor();
	}
	ImGui::End();

	// Current window is expected to be scene viewport!
	ImVec2 viewportPos = ImGui::GetWindowPos();
	ImVec2 viewportSize = ImGui::GetWindowSize();
	if (ImGui::BeginControlsWindow("##SimulationControls"))
	{
		float width = IMGUI_ICON_DEFAULT_SIZE * 3 + IMGUI_ICON_DEFAULT_SPACING * 2 + ImGui::GetStyle().WindowPadding.x * 2;

		ImVec2 pos = ImGui::GetWindowPos(); // Initially this is the position read from the imgui.ini file.
		ImGui::SetWindowPos(ImVec2(viewportPos.x + (viewportSize.x - width) * 0.5f, pos.y));

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

		if (ImGui::IconButton(imgui_icon_play, imgui_icon_play, IMGUI_ICON_DEFAULT_SIZE, this->scene->isPlayable()))
		{
			this->scene->play();
			setSelectedEntityNoUndo({});
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
			setSelectedEntityNoUndo({});
		}

		scene = &this->scene->getCurrentScene();
		cameraController.camera = &camera;

		ImGui::PopStyleColor();
	}
	ImGui::End();


	if (!ImGui::IsAnyItemActive())
	{
		if (!inputCaptured && ImGui::IsKeyDown(key_ctrl) && ImGui::IsKeyPressed('Z'))
		{
			undoStack.undo();
			inputCaptured = true;
			objectMovedByGizmo = true;
		}
		if (!inputCaptured && ImGui::IsKeyDown(key_ctrl) && ImGui::IsKeyPressed('Y'))
		{
			undoStack.redo();
			inputCaptured = true;
			objectMovedByGizmo = true;
		}
		if (!inputCaptured && ImGui::IsKeyDown(key_ctrl) && ImGui::IsKeyPressed('S'))
		{
			serializeSceneToDisk(*this->scene, renderer->settings);
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
			testPhysicsInteraction(*scene, camera.generateWorldSpaceRay(input.mouse.relX, input.mouse.relY));
		}
		else if (input.keyboard[key_ctrl].down)
		{
			vec3 dir = -camera.generateWorldSpaceRay(input.mouse.relX, input.mouse.relY).direction;
			undoStack.pushAction("sun direction", sun_direction_undo{ &this->scene->sun, this->scene->sun.direction, dir });
			this->scene->sun.direction = dir;
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

void scene_editor::drawEntityCreationPopup()
{
	game_scene* scene = &this->scene->getCurrentScene();
	render_camera& camera = this->scene->camera;

	if (ImGui::BeginPopup("CreateEntityPopup"))
	{
		bool clicked = false;

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
}

void scene_editor::serializeToFile()
{
	serializeSceneToDisk(*scene, renderer->settings);
}

bool scene_editor::deserializeFromFile()
{
	std::string environmentName;
	if (deserializeSceneFromDisk(*scene, renderer->settings, environmentName))
	{
		scene->stop();

		setSelectedEntityNoUndo({});
		scene->environment.setFromTexture(environmentName);
		scene->environment.forceUpdate(this->scene->sun.direction);
		renderer->pathTracer.resetRendering();

		return true;
	}
	return false;
}

static bool editCamera(render_camera& camera)
{
	bool result = false;
	if (ImGui::BeginTree("Camera"))
	{
		if (ImGui::BeginProperties())
		{
			result |= ImGui::PropertySliderAngle("Field of view", camera.verticalFOV, 1.f, 150.f);
			result |= ImGui::PropertyInput("Near plane", camera.nearPlane);
			bool infiniteFarplane = camera.farPlane < 0.f;
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
			}
			if (!infiniteFarplane)
			{
				result |= ImGui::PropertyInput("Far plane", camera.farPlane);
			}

			ImGui::EndProperties();
		}
		
		ImGui::EndTree();
	}
	return result;
}

static bool plotAndEditTonemapping(tonemap_settings& tonemap)
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
			result |= ImGui::PropertySlider("Shoulder strength", tonemap.A, 0.f, 1.f);
			result |= ImGui::PropertySlider("Linear strength", tonemap.B, 0.f, 1.f);
			result |= ImGui::PropertySlider("Linear angle", tonemap.C, 0.f, 1.f);
			result |= ImGui::PropertySlider("Toe strength", tonemap.D, 0.f, 1.f);
			result |= ImGui::PropertySlider("Tone numerator", tonemap.E, 0.f, 1.f);
			result |= ImGui::PropertySlider("Toe denominator", tonemap.F, 0.f, 1.f);
			result |= ImGui::PropertySlider("Linear white", tonemap.linearWhite, 0.f, 100.f);
			result |= ImGui::PropertySlider("Exposure", tonemap.exposure, -3.f, 3.f);
			ImGui::EndProperties();
		}

		ImGui::EndTree();
	}
	return result;
}

static bool editSunShadowParameters(directional_light& sun)
{
	bool result = false;
	if (ImGui::BeginTree("Sun"))
	{
		if (ImGui::BeginProperties())
		{
			result |= ImGui::PropertySlider("Intensity", sun.intensity, 0.f, 1000.f);
			result |= ImGui::PropertyColor("Color", sun.color);

			result |= ImGui::PropertyDropdownPowerOfTwo("Shadow resolution", 128, 2048, sun.shadowDimensions);
			result |= ImGui::PropertyCheckbox("Stabilize", sun.stabilize);

			result |= ImGui::PropertySlider("# Cascades", sun.numShadowCascades, 1, 4);

			const float minCascadeDistance = 0.f, maxCascadeDistance = 300.f;
			const float minBias = 0.f, maxBias = 0.0015f;
			const float minBlend = 0.f, maxBlend = 10.f;
			if (sun.numShadowCascades == 1)
			{
				result |= ImGui::PropertySlider("Distance", sun.cascadeDistances.x, minCascadeDistance, maxCascadeDistance);
				result |= ImGui::PropertySlider("Bias", sun.bias.x, minBias, maxBias, "%.6f");
				result |= ImGui::PropertySlider("Blend distances", sun.blendDistances.x, minBlend, maxBlend, "%.6f");
			}
			else if (sun.numShadowCascades == 2)
			{
				result |= ImGui::PropertySlider("Distance", sun.cascadeDistances.xy, minCascadeDistance, maxCascadeDistance);
				result |= ImGui::PropertySlider("Bias", sun.bias.xy, minBias, maxBias, "%.6f");
				result |= ImGui::PropertySlider("Blend distances", sun.blendDistances.xy, minBlend, maxBlend, "%.6f");
			}
			else if (sun.numShadowCascades == 3)
			{
				result |= ImGui::PropertySlider("Distance", sun.cascadeDistances.xyz, minCascadeDistance, maxCascadeDistance);
				result |= ImGui::PropertySlider("Bias", sun.bias.xyz, minBias, maxBias, "%.6f");
				result |= ImGui::PropertySlider("Blend distances", sun.blendDistances.xyz, minBlend, maxBlend, "%.6f");
			}
			else if (sun.numShadowCascades == 4)
			{
				result |= ImGui::PropertySlider("Distance", sun.cascadeDistances, minCascadeDistance, maxCascadeDistance);
				result |= ImGui::PropertySlider("Bias", sun.bias, minBias, maxBias, "%.6f");
				result |= ImGui::PropertySlider("Blend distances", sun.blendDistances, minBlend, maxBlend, "%.6f");
			}

			ImGui::EndProperties();
		}

		ImGui::EndTree();
	}
	return result;
}

static bool editAO(bool& enable, hbao_settings& settings, const ref<dx_texture>& aoTexture)
{
	bool result = false;
	if (ImGui::BeginProperties())
	{
		result |= ImGui::PropertyCheckbox("Enable HBAO", enable);
		if (enable)
		{
			result |= ImGui::PropertySlider("Num rays", settings.numRays, 1, 16);
			result |= ImGui::PropertySlider("Max num steps per ray", settings.maxNumStepsPerRay, 1, 16);
			result |= ImGui::PropertySlider("Radius", settings.radius, 0.f, 1.f, "%.3fm");
			result |= ImGui::PropertySlider("Strength", settings.strength, 0.f, 2.f);
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

static bool editSSS(bool& enable, sss_settings& settings, const ref<dx_texture>& sssTexture)
{
	bool result = false;
	if (ImGui::BeginProperties())
	{
		result |= ImGui::PropertyCheckbox("Enable SSS", enable);
		if (enable)
		{
			result |= ImGui::PropertySlider("Num iterations", settings.numSteps, 1, 64);
			result |= ImGui::PropertySlider("Ray distance", settings.rayDistance, 0.05f, 3.f, "%.3fm");
			result |= ImGui::PropertySlider("Thickness", settings.thickness, 0.05f, 1.f, "%.3fm");
			result |= ImGui::PropertySlider("Max distance from camera", settings.maxDistanceFromCamera, 5.f, 1000.f, "%.3fm");
			result |= ImGui::PropertySlider("Distance fadeout range", settings.distanceFadeoutRange, 1.f, 5.f, "%.3fm");
			result |= ImGui::PropertySlider("Border fadeout", settings.borderFadeout, 0.f, 0.5f);
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

static bool editSSR(bool& enable, ssr_settings& settings, const ref<dx_texture>& ssrTexture)
{
	bool result = false;
	if (ImGui::BeginProperties())
	{
		result |= ImGui::PropertyCheckbox("Enable SSR", enable);
		if (enable)
		{
			result |= ImGui::PropertySlider("Num iterations", settings.numSteps, 1, 1024);
			result |= ImGui::PropertySlider("Max distance", settings.maxDistance, 5.f, 1000.f, "%.3fm");
			result |= ImGui::PropertySlider("Min stride", settings.minStride, 1.f, 50.f, "%.3fm");
			result |= ImGui::PropertySlider("Max stride", settings.maxStride, settings.minStride, 50.f, "%.3fm");
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

static bool editTAA(bool& enable, taa_settings& settings)
{
	bool result = false;
	if (ImGui::BeginProperties())
	{
		result |= ImGui::PropertyCheckbox("Enable TAA", enable);
		if (enable)
		{
			result |= ImGui::PropertySlider("Jitter strength", settings.cameraJitterStrength);
		}
		ImGui::EndProperties();
	}
	return result;
}

static bool editBloom(bool& enable, bloom_settings& settings, const ref<dx_texture>& bloomTexture)
{
	bool result = false;
	if (ImGui::BeginProperties())
	{
		result |= ImGui::PropertyCheckbox("Enable bloom", enable);
		if (enable)
		{
			result |= ImGui::PropertySlider("Bloom threshold", settings.threshold, 0.5f, 100.f);
			result |= ImGui::PropertySlider("Bloom strength", settings.strength);
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

static bool editSharpen(bool& enable, sharpen_settings& settings)
{
	bool result = false;
	if (ImGui::BeginProperties())
	{
		result |= ImGui::PropertyCheckbox("Enable sharpen", enable);
		if (enable)
		{
			result |= ImGui::PropertySlider("Sharpen strength", settings.strength);
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
			ImGui::PropertySlider("Time scale", this->scene->timestepScale);

			if (ImGui::PropertyDropdown("Renderer mode", rendererModeNames, renderer_mode_count, (uint32&)renderer->mode))
			{
				pathTracer.resetRendering();
			}

			dx_memory_usage memoryUsage = dxContext.getMemoryUsage();

			ImGui::PropertyValue("Video memory usage", "%u / %uMB", memoryUsage.currentlyUsed, memoryUsage.available);
			//ImGui::PropertyValue("Running command lists", "%u", dxContext.renderQueue.numRunningCommandLists);

			ImGui::PropertyDropdown("Aspect ratio", aspectRatioNames, aspect_ratio_mode_count, (uint32&)renderer->aspectRatioMode);

			ImGui::PropertyCheckbox("Static shadow map caching", enableStaticShadowMapCaching);

			ImGui::EndProperties();
		}

		editCamera(this->scene->camera);
		plotAndEditTonemapping(renderer->settings.tonemapSettings);
		editSunShadowParameters(this->scene->sun);

		if (ImGui::BeginTree("Post processing"))
		{
			if (renderer->spec.allowAO) { editAO(renderer->settings.enableAO, renderer->settings.aoSettings, renderer->getAOResult()); ImGui::Separator(); }
			if (renderer->spec.allowSSS) { editSSS(renderer->settings.enableSSS, renderer->settings.sssSettings, renderer->getSSSResult()); ImGui::Separator(); }
			if (renderer->spec.allowSSR) { editSSR(renderer->settings.enableSSR, renderer->settings.ssrSettings, renderer->getSSRResult()); ImGui::Separator(); }
			if (renderer->spec.allowTAA) { editTAA(renderer->settings.enableTAA, renderer->settings.taaSettings); ImGui::Separator(); }
			if (renderer->spec.allowBloom) { editBloom(renderer->settings.enableBloom, renderer->settings.bloomSettings, renderer->getBloomResult()); ImGui::Separator(); }
			editSharpen(renderer->settings.enableSharpen, renderer->settings.sharpenSettings);

			ImGui::EndTree();
		}

		if (ImGui::BeginTree("Environment"))
		{
			auto& environment = this->scene->environment;

			if (ImGui::BeginProperties())
			{
				std::string source = environment.isProcedural() ? "None (procedural)" : environment.name.string();
				if (ImGui::PropertyDragDropStringTarget("Texture source", "content_browser_sky", source, environment.isProcedural() ? 0 : "Make procedural"))
				{
					if (source.empty())
					{
						environment.setToProcedural(this->scene->sun.direction);
					}
					else
					{
						environment.setFromTexture(source);
					}
				}

				ImGui::PropertySlider("Sky intensity", environment.skyIntensity, 0.f, 2.f);
				ImGui::PropertySlider("GI intensity", environment.globalIlluminationIntensity, 0.f, 2.f);

				ImGui::PropertyDropdown("GI mode", environmentGIModeNames, 1 + dxContext.featureSupport.raytracing(), (uint32&)environment.giMode);

				ImGui::EndProperties();

				if (environment.giMode == environment_gi_update_raytraced)
				{
					ImGui::TextColored(ImVec4(1, 0, 0, 1), "Raytraced GI currently is diffuse only");
				}
			}

			if (environment.giMode == environment_gi_update_raytraced)
			{
				if (ImGui::BeginTree("Light probe"))
				{
					auto& grid = environment.lightProbeGrid;

					if (ImGui::BeginProperties())
					{
						ImGui::PropertyCheckbox("Visualize probes", grid.visualizeProbes);
						ImGui::PropertyCheckbox("Visualize rays", grid.visualizeRays);
						ImGui::PropertyCheckbox("Show test sphere", grid.showTestSphere);

						ImGui::PropertyCheckbox("Auto rotate rays", grid.autoRotateRays);
						if (!grid.autoRotateRays)
						{
							grid.rotateRays = ImGui::PropertyButton("Rotate", "Go");
						}

						ImGui::EndProperties();
					}

					if (ImGui::BeginTree("Irradiance"))
					{
						if (ImGui::BeginProperties()) { ImGui::PropertySlider("Scale", grid.irradianceUIScale, 0.1f, 20.f); ImGui::EndProperties(); }
						ImGui::Image(grid.irradiance, (uint32)(grid.irradiance->width * grid.irradianceUIScale));
						ImGui::EndTree();
					}
					if (ImGui::BeginTree("Depth"))
					{
						if (ImGui::BeginProperties()) { ImGui::PropertySlider("Scale", grid.depthUIScale, 0.1f, 20.f); ImGui::EndProperties(); }
						ImGui::Image(grid.depth, (uint32)(grid.depth->width * grid.depthUIScale));
						ImGui::EndTree();
					}
					if (ImGui::BeginTree("Raytraced radiance"))
					{
						if (ImGui::BeginProperties()) { ImGui::PropertySlider("Scale", grid.raytracedRadianceUIScale, 0.1f, 20.f); ImGui::EndProperties(); }
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
				ImGui::PropertyInput("Fixed frame rate", physicsSettings.frameRate);
				if (physicsSettings.frameRate < 30)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));
					ImGui::PropertyValue("", "Low frame rate");
					ImGui::PopStyleColor();
				}

				ImGui::PropertySlider("Rigid solver iterations", physicsSettings.numRigidSolverIterations, 1, 200);

				ImGui::PropertySlider("Cloth velocity iterations", physicsSettings.numClothVelocityIterations, 0, 10);
				ImGui::PropertySlider("Cloth position iterations", physicsSettings.numClothPositionIterations, 0, 10);
				ImGui::PropertySlider("Cloth drift iterations", physicsSettings.numClothDriftIterations, 0, 10);

				ImGui::PropertySlider("Test force", physicsSettings.testForce, 1.f, 10000.f);

				ImGui::PropertyCheckbox("Use SIMD", physicsSettings.simd);

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
				change |= ImGui::PropertySlider("Master volume", masterAudioSettings.volume, 0.f, maxVolume);

				ImGui::PropertySeparator();

				for (uint32 i = 0; i < sound_type_count; ++i)
				{
					change |= ImGui::PropertySlider(soundTypeNames[i], soundTypeVolumes[i], 0.f, 1.f);
				}

				ImGui::PropertySeparator();

				static reverb_preset oldReverbPreset = masterAudioSettings.reverbPreset;

				bool reverbEnabled = masterAudioSettings.reverbPreset != reverb_none;
				if (ImGui::PropertyCheckbox("Reverb enabled", reverbEnabled))
				{
					if (reverbEnabled) { masterAudioSettings.reverbPreset = oldReverbPreset; }
					else { oldReverbPreset = masterAudioSettings.reverbPreset; masterAudioSettings.reverbPreset = reverb_none; }

					change = true;
				}

				if (reverbEnabled)
				{
					change |= ImGui::PropertyDropdown("Reverb preset", reverbPresetNames, reverb_preset_count, (uint32&)masterAudioSettings.reverbPreset);
				}

				ImGui::EndProperties();
			}
			ImGui::EndTree();
		}

		if (renderer->mode == renderer_mode_pathtraced)
		{
			bool pathTracerDirty = false;
			if (ImGui::BeginProperties())
			{
				pathTracerDirty |= ImGui::PropertySlider("Max recursion depth", pathTracer.recursionDepth, 0, pathTracer.maxRecursionDepth - 1);
				pathTracerDirty |= ImGui::PropertySlider("Start russian roulette after", pathTracer.startRussianRouletteAfter, 0, pathTracer.recursionDepth);
				pathTracerDirty |= ImGui::PropertyCheckbox("Use thin lens camera", pathTracer.useThinLensCamera);
				if (pathTracer.useThinLensCamera)
				{
					pathTracerDirty |= ImGui::PropertySlider("Focal length", pathTracer.focalLength, 0.5f, 50.f);
					pathTracerDirty |= ImGui::PropertySlider("F-Number", pathTracer.fNumber, 1.f, 128.f);
				}
				pathTracerDirty |= ImGui::PropertyCheckbox("Use real materials", pathTracer.useRealMaterials);
				pathTracerDirty |= ImGui::PropertyCheckbox("Enable direct lighting", pathTracer.enableDirectLighting);
				if (pathTracer.enableDirectLighting)
				{
					pathTracerDirty |= ImGui::PropertySlider("Light intensity scale", pathTracer.lightIntensityScale, 0.f, 50.f);
					pathTracerDirty |= ImGui::PropertySlider("Point light radius", pathTracer.pointLightRadius, 0.01f, 1.f);

					pathTracerDirty |= ImGui::PropertyCheckbox("Multiple importance sampling", pathTracer.multipleImportanceSampling);
				}

				ImGui::EndProperties();
			}


			if (pathTracerDirty)
			{
				pathTracer.numAveragedFrames = 0;
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




