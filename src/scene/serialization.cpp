#include "pch.h"
#include "serialization.h"

#include "editor/file_dialog.h"
#include "core/yaml.h"
#include "core/log.h"

#include "physics/physics.h"

namespace YAML
{
	template<typename T>
	struct convert
	{
		template <typename = std::enable_if_t<is_reflected_v<T>>>
		static Node encode(const T& v)
		{
			Node n;
			type_descriptor<T>::apply(
				[&n](const char* name, auto& member)
				{
					using T = std::decay_t<decltype(member)>;
					if constexpr (std::is_enum_v<T>)
					{
						n[name] = (int)member;
					}
					else
					{
						n[name] = member;
					}
				},
				v
			);
			return n;
		}

		template <typename = std::enable_if_t<is_reflected_v<T>>>
		static bool decode(const Node& n, T& v)
		{
			if (!n.IsMap()) { return false; }

			type_descriptor<T>::apply(
				[&n](const char* name, auto& member)
				{
					using T = std::decay_t<decltype(member)>;
					if constexpr (std::is_enum_v<T>)
					{
						YAML_LOAD_ENUM(n, member, name);
					}
					else
					{
						YAML_LOAD(n, member, name);
					}
				},
				v
			);

			return true;
		}
	};
}

namespace YAML
{
	template<>
	struct convert<rigid_body_component>
	{
		static Node encode(const rigid_body_component& c)
		{
			Node n;
			n["Local COG"] = c.localCOGPosition;
			n["Inv mass"] = c.invMass;
			n["Inv inertia"] = c.invInertia;
			n["Gravity factor"] = c.gravityFactor;
			n["Linear damping"] = c.linearDamping;
			n["Angular damping"] = c.angularDamping;
			return n;
		}

		static bool decode(const Node& n, rigid_body_component& c)
		{
			if (!n.IsMap()) { return false; }

			YAML_LOAD(n, c.localCOGPosition, "Local COG");
			YAML_LOAD(n, c.invMass, "Inv mass");
			YAML_LOAD(n, c.invInertia, "Inv inertia");
			YAML_LOAD(n, c.gravityFactor, "Gravity factor");
			YAML_LOAD(n, c.linearDamping, "Linear damping");
			YAML_LOAD(n, c.angularDamping, "Angular damping");

			return true;
		}
	};

	template<>
	struct convert<force_field_component>
	{
		static Node encode(const force_field_component& c)
		{
			Node n;
			n["Force"] = c.force;
			return n;
		}

		static bool decode(const Node& n, force_field_component& c)
		{
			if (!n.IsMap()) { return false; }

			YAML_LOAD(n, c.force, "Force");

			return true;
		}
	};

	template<>
	struct convert<collider_component>
	{
		static Node encode(const collider_component& c)
		{
			Node n;
			n["Type"] = colliderTypeNames[c.type];

			switch (c.type)
			{
			case collider_type_sphere:
			{
				n["Center"] = c.sphere.center;
				n["Radius"]= c.sphere.radius;
			} break;

			case collider_type_capsule:
			{
				n["Position A"] = c.capsule.positionA;
				n["Position B"] = c.capsule.positionB;
				n["Radius"] =c.capsule.radius;
			} break;

			case collider_type_aabb:
			{
				n["Min corner"] = c.aabb.minCorner;
				n["Max corner"] = c.aabb.maxCorner;
			} break;

			case collider_type_obb:
			{
				n["Center"] = c.obb.center;
				n["Radius"] = c.obb.radius;
				n["Rotation"] = c.obb.rotation;
			} break;

			case collider_type_hull:
			{
			} break;
			}

			n["Restitution"] = c.material.restitution;
			n["Friction"] = c.material.friction;
			n["Density"] = c.material.density;
			return n;
		}

		static bool decode(const Node& n, collider_component& c)
		{
			if (!n.IsMap())
			{
				return false;
			}

			std::string typeString;
			YAML_LOAD(n, typeString, "Type");
			for (uint32 i = 0; i < collider_type_count; ++i)
			{
				if (typeString == colliderTypeNames[i])
				{
					c.type = (collider_type)i;
					break;
				}
			}

			physics_material material;
			YAML_LOAD(n, material.restitution, "Restitution");
			YAML_LOAD(n, material.friction, "Friction");
			YAML_LOAD(n, material.density, "Density");

			switch (c.type)
			{
				case collider_type_sphere:
				{
					vec3 center;
					float radius;
					YAML_LOAD(n, center, "Center");
					YAML_LOAD(n, radius, "Radius");
					c = collider_component::asSphere({ center, radius }, material);
				} break;

				case collider_type_capsule:
				{
					vec3 positionA, positionB;
					float radius;
					YAML_LOAD(n, positionA, "Position A");
					YAML_LOAD(n, positionB, "Position B");
					YAML_LOAD(n, radius, "Radius");
					c = collider_component::asCapsule({ positionA, positionB, radius }, material);
				} break;

				case collider_type_aabb:
				{
					vec3 minCorner, maxCorner;
					YAML_LOAD(n, minCorner, "Min corner");
					YAML_LOAD(n, maxCorner, "Max corner");
					c = collider_component::asAABB(bounding_box::fromMinMax(minCorner, maxCorner), material);
				} break;

				case collider_type_obb:
				{
					vec3 center, radius;
					quat rotation;
					YAML_LOAD(n, center, "Center");
					YAML_LOAD(n, radius, "Radius");
					YAML_LOAD(n, rotation, "Rotation");

					c = collider_component::asOBB({ center, radius, rotation }, material);
				} break;

				case collider_type_hull:
				{
					return false;
				} break;

				default: assert(false); break;
			}

			return true;
		}
	};

	template<>
	struct convert<cloth_component>
	{
		static Node encode(const cloth_component& c)
		{
			Node n;
			n["Width"] = c.width;
			n["Height"] = c.height;
			n["Grid size x"] = c.gridSizeX;
			n["Grid size y"] = c.gridSizeY;
			n["Total mass"] = c.totalMass;
			n["Stiffness"] = c.stiffness;
			n["Damping"] = c.damping;
			n["Gravity factor"] = c.gravityFactor;
			return n;
		}

		static bool decode(const Node& n, cloth_component& c)
		{
			if (!n.IsMap()) { return false; }

			uint32 gridSizeX, gridSizeY;
			float width, height, totalMass, stiffness, damping, gravityFactor;

			YAML_LOAD(n, width, "Width");
			YAML_LOAD(n, height, "Height");
			YAML_LOAD(n, gridSizeX, "Grid size x");
			YAML_LOAD(n, gridSizeY, "Grid size y");
			YAML_LOAD(n, totalMass, "Total mass");
			YAML_LOAD(n, stiffness, "Stiffness");
			YAML_LOAD(n, damping, "Damping");
			YAML_LOAD(n, gravityFactor, "Gravity factor");

			c = cloth_component(width, height, gridSizeX, gridSizeY, totalMass, stiffness, damping, gravityFactor);

			return true;
		}
	};

	template<>
	struct convert<raster_component>
	{
		static Node encode(const raster_component& c)
		{
			Node n;
			n["Handle"] = c.mesh->handle;
			n["Flags"] = c.mesh->flags;
			return n;
		}

		static bool decode(const Node& n, raster_component& c)
		{
			if (!n.IsMap()) { return false; }

			asset_handle handle;
			uint32 flags;

			YAML_LOAD(n, handle, "Handle");
			YAML_LOAD(n, flags, "Flags");

			c.mesh = loadMeshFromHandle(handle, flags);

			return true;
		}
	};
}

void serializeSceneToDisk(game_scene& scene, const render_camera& camera, const renderer_settings& rendererSettings)
{
	if (scene.savePath.empty())
	{
		fs::path filename = saveFileDialog("Scene files", "sc");
		if (filename.empty())
		{
			return;
		}

		scene.savePath = filename;
	}

	YAML::Node out;
	out["Scene"] = "My scene";
	out["Camera"] = camera;
	out["Rendering"] = rendererSettings;
	out["Sun"] = scene.sun;
	out["Environment"] = (scene.environment ? scene.environment->name : fs::path());

	YAML::Node entityNode;

	scene.forEachEntity([&entityNode, &scene](entity_handle entityID)
	{
		scene_entity entity = { entityID, scene };

		// Only entities with tags are valid top level entities. All others are helpers like colliders and constraints.
		if (tag_component* tag = entity.getComponentIfExists<tag_component>())
		{
			YAML::Node n;
			n["Tag"] = tag->name;

			if (auto* c = entity.getComponentIfExists<transform_component>()) { n["Transform"] = *c; }
			if (auto* c = entity.getComponentIfExists<position_component>()) { n["Position"] = *c; }
			if (auto* c = entity.getComponentIfExists<position_rotation_component>()) { n["Position/Rotation"] = *c; }
			if (entity.hasComponent<dynamic_transform_component>()) { n["Dynamic"] = true; }
			if (auto* c = entity.getComponentIfExists<point_light_component>()) { n["Point light"] = *c; }
			if (auto* c = entity.getComponentIfExists<spot_light_component>()) { n["Spot light"] = *c; }
			if (auto* c = entity.getComponentIfExists<rigid_body_component>()) { n["Rigid body"] = *c; }
			if (auto* c = entity.getComponentIfExists<force_field_component>()) { n["Force field"] = *c; }
			if (auto* c = entity.getComponentIfExists<cloth_component>()) { n["Cloth"] = *c; }
			if (auto* c = entity.getComponentIfExists<raster_component>()) { n["Raster"] = *c; }
			if (auto* c = entity.getComponentIfExists<physics_reference_component>())
			{
				if (c->numColliders)
				{
					YAML::Node c;
					for (collider_component& collider : collider_component_iterator(entity))
					{
						c.push_back(collider);
					}
					n["Colliders"] = c;
				}
			}


			/*
			TODO:
				- Animation
				- Raytrace
				- Constraints
			*/
			
			entityNode.push_back(n);
		}
	});

	out["Entities"] = entityNode;


	fs::create_directories(scene.savePath.parent_path());

	std::ofstream fout(scene.savePath);
	fout << out;

	LOG_MESSAGE("Scene saved to '%ws'", scene.savePath.c_str());
}

bool deserializeSceneFromDisk(game_scene& scene, render_camera& camera, renderer_settings& rendererSettings, std::string& environmentName)
{
	fs::path filename = openFileDialog("Scene files", "sc");
	if (filename.empty())
	{
		return false;
	}

	std::ifstream stream(filename);
	YAML::Node n = YAML::Load(stream);
	if (!n["Scene"])
	{
		return false;
	}

	scene = game_scene();
	scene.savePath = std::move(filename);

	std::string sceneName = n["Scene"].as<std::string>();

	YAML_LOAD(n, camera, "Camera");
	YAML_LOAD(n, rendererSettings, "Rendering");
	YAML_LOAD(n, scene.sun, "Sun");

	YAML_LOAD(n, environmentName, "Environment");

	auto entitiesNode = n["Entities"];
	for (auto entityNode : entitiesNode)
	{
		std::string name = entityNode["Tag"].as<std::string>();
		scene_entity entity = scene.createEntity(name.c_str());

#define LOAD_COMPONENT(type, name) if (auto node = entityNode[name]) { entity.addComponent<type>(node.as<type>()); }

		LOAD_COMPONENT(transform_component, "Transform");
		LOAD_COMPONENT(position_component, "Position");
		LOAD_COMPONENT(position_rotation_component, "Position/Rotation");
		if (entityNode["Dynamic"]) { entity.addComponent<dynamic_transform_component>(); }
		LOAD_COMPONENT(point_light_component, "Point light");
		LOAD_COMPONENT(spot_light_component, "Spot light");
		LOAD_COMPONENT(rigid_body_component, "Rigid body");
		LOAD_COMPONENT(force_field_component, "Force field");
		LOAD_COMPONENT(cloth_component, "Cloth");
		LOAD_COMPONENT(raster_component, "Raster");

		if (auto collidersNode = entityNode["Colliders"])
		{
			for (uint32 i = 0; i < collidersNode.size(); ++i)
			{
				entity.addComponent<collider_component>(collidersNode[i].as<collider_component>());
			}
		}
	}

	LOG_MESSAGE("Scene loaded from '%ws'", scene.savePath.c_str());

	return true;
}

