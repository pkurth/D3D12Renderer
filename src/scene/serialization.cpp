#include "pch.h"
#include "serialization.h"

#include "editor/file_dialog.h"
#include "core/yaml.h"


static YAML::Emitter& operator<<(YAML::Emitter& out, const render_camera& camera)
{
	out << YAML::BeginMap
		<< YAML::Key << "Position" << YAML::Value << camera.position
		<< YAML::Key << "Rotation" << YAML::Value << camera.rotation
		<< YAML::Key << "Near plane" << YAML::Value << camera.nearPlane
		<< YAML::Key << "Far plane" << YAML::Value << camera.farPlane
		<< YAML::Key << "Type" << YAML::Value << camera.type;

	if (camera.type == camera_type_ingame)
	{
		out << YAML::Key << "FOV" << YAML::Value << camera.verticalFOV;
	}
	else
	{
		out << YAML::Key << "Fx" << YAML::Value << camera.fx
			<< YAML::Key << "Fy" << YAML::Value << camera.fy
			<< YAML::Key << "Cx" << YAML::Value << camera.cx
			<< YAML::Key << "Cy" << YAML::Value << camera.cy;
	}

	out << YAML::EndMap;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const directional_light& sun)
{
	out << YAML::BeginMap
		<< YAML::Key << "Color" << YAML::Value << sun.color
		<< YAML::Key << "Intensity" << YAML::Value << sun.intensity
		<< YAML::Key << "Direction" << YAML::Value << sun.direction
		<< YAML::Key << "Cascades" << YAML::Value << sun.numShadowCascades
		<< YAML::Key << "Distances" << YAML::Value << sun.cascadeDistances
		<< YAML::Key << "Bias" << YAML::Value << sun.bias
		<< YAML::Key << "Blend distances" << YAML::Value << sun.blendDistances
		<< YAML::EndMap;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const tonemap_settings& s)
{
	out << YAML::BeginMap
		<< YAML::Key << "A" << YAML::Value << s.A
		<< YAML::Key << "B" << YAML::Value << s.B
		<< YAML::Key << "C" << YAML::Value << s.C
		<< YAML::Key << "D" << YAML::Value << s.D
		<< YAML::Key << "E" << YAML::Value << s.E
		<< YAML::Key << "F" << YAML::Value << s.F
		<< YAML::Key << "Linear white" << YAML::Value << s.linearWhite
		<< YAML::Key << "Exposure" << YAML::Value << s.exposure
		<< YAML::EndMap;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const ssr_settings& s)
{
	out << YAML::BeginMap
		<< YAML::Key << "Num steps" << YAML::Value << s.numSteps
		<< YAML::Key << "Max distance" << YAML::Value << s.maxDistance
		<< YAML::Key << "Stride cutoff" << YAML::Value << s.strideCutoff
		<< YAML::Key << "Min stride" << YAML::Value << s.minStride
		<< YAML::Key << "Max stride" << YAML::Value << s.maxStride
		<< YAML::EndMap;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const taa_settings& s)
{
	out << YAML::BeginMap
		<< YAML::Key << "Camera jitter" << YAML::Value << s.cameraJitterStrength
		<< YAML::EndMap;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const bloom_settings& s)
{
	out << YAML::BeginMap
		<< YAML::Key << "Threshold" << YAML::Value << s.threshold
		<< YAML::Key << "Strength" << YAML::Value << s.strength
		<< YAML::EndMap;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const sharpen_settings& s)
{
	out << YAML::BeginMap
		<< YAML::Key << "Strength" << YAML::Value << s.strength
		<< YAML::EndMap;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const renderer_settings& s)
{
	out << YAML::BeginMap
		<< YAML::Key << "Tone map" << YAML::Value << s.tonemapSettings
		<< YAML::Key << "Environment intensity" << YAML::Value << s.environmentIntensity
		<< YAML::Key << "Sky intensity" << YAML::Value << s.skyIntensity
		<< YAML::Key << "Enable SSR" << YAML::Value << s.enableSSR
		<< YAML::Key << "SSR" << YAML::Value << s.ssrSettings
		<< YAML::Key << "Enable TAA" << YAML::Value << s.enableTAA
		<< YAML::Key << "TAA" << YAML::Value << s.taaSettings
		<< YAML::Key << "Enable Bloom" << YAML::Value << s.enableBloom
		<< YAML::Key << "Bloom" << YAML::Value << s.bloomSettings
		<< YAML::Key << "Enable Sharpen" << YAML::Value << s.enableSharpen
		<< YAML::Key << "Sharpen" << YAML::Value << s.sharpenSettings
		<< YAML::EndMap;
	return out;
}

static YAML::Emitter& operator<<(YAML::Emitter& out, const transform_component& c)
{
	out << YAML::BeginMap
		<< YAML::Key << "Rotation" << YAML::Value << c.rotation
		<< YAML::Key << "Position" << YAML::Value << c.position
		<< YAML::Key << "Scale" << YAML::Value << c.scale
		<< YAML::EndMap;
	return out;
}

#define LOAD(var, name) var = n[name].as<std::remove_reference_t<decltype(var)>>()
#define LOAD_ENUM(var, name) var = (decltype(var))(n[name].as<int>())

namespace YAML
{
	template<>
	struct convert<render_camera>
	{
		static bool decode(const Node& n, render_camera& camera) 
		{ 
			if (!n.IsMap())
			{
				return false;
			}

			LOAD(camera.position, "Position");
			LOAD(camera.rotation, "Rotation");
			LOAD(camera.nearPlane, "Near plane");
			LOAD(camera.farPlane, "Far plane");
			LOAD_ENUM(camera.type, "Type");

			if (camera.type == camera_type_ingame)
			{
				LOAD(camera.verticalFOV, "FOV");
			}
			else
			{
				LOAD(camera.fx, "Fx");
				LOAD(camera.fy, "Fy");
				LOAD(camera.cx, "Cx");
				LOAD(camera.cy, "Cy");
			}

			return true; 
		}
	};

	template<>
	struct convert<tonemap_settings>
	{
		static bool decode(const Node& n, tonemap_settings& s)
		{
			if (!n.IsMap())
			{
				return false;
			}

			LOAD(s.A, "A");
			LOAD(s.B, "B");
			LOAD(s.C, "C");
			LOAD(s.D, "D");
			LOAD(s.E, "E");
			LOAD(s.F, "F");
			LOAD(s.linearWhite, "Linear white");
			LOAD(s.exposure, "Exposure");

			return true;
		}
	};

	template<>
	struct convert<ssr_settings>
	{
		static bool decode(const Node& n, ssr_settings& s)
		{
			if (!n.IsMap())
			{
				return false;
			}

			LOAD(s.numSteps, "Num steps");
			LOAD(s.maxDistance, "Max distance");
			LOAD(s.strideCutoff, "Stride cutoff");
			LOAD(s.minStride, "Min stride");
			LOAD(s.maxStride, "Max stride");

			return true;
		}
	};

	template<>
	struct convert<taa_settings>
	{
		static bool decode(const Node& n, taa_settings& s)
		{
			if (!n.IsMap())
			{
				return false;
			}

			LOAD(s.cameraJitterStrength, "Camera jitter");

			return true;
		}
	};

	template<>
	struct convert<bloom_settings>
	{
		static bool decode(const Node& n, bloom_settings& s)
		{
			if (!n.IsMap())
			{
				return false;
			}

			LOAD(s.threshold, "Threshold");
			LOAD(s.strength, "Strength");

			return true;
		}
	};

	template<>
	struct convert<sharpen_settings>
	{
		static bool decode(const Node& n, sharpen_settings& s)
		{
			if (!n.IsMap())
			{
				return false;
			}

			LOAD(s.strength, "Strength");

			return true;
		}
	};

	template<>
	struct convert<renderer_settings>
	{
		static bool decode(const Node& n, renderer_settings& settings)
		{
			if (!n.IsMap())
			{
				return false;
			}

			LOAD(settings.tonemapSettings, "Tone map");
			LOAD(settings.environmentIntensity, "Environment intensity");
			LOAD(settings.skyIntensity, "Sky intensity");
			LOAD(settings.enableSSR, "Enable SSR");
			LOAD(settings.ssrSettings, "SSR");
			LOAD(settings.enableTAA, "Enable TAA");
			LOAD(settings.taaSettings, "TAA");
			LOAD(settings.enableBloom, "Enable Bloom");
			LOAD(settings.bloomSettings, "Bloom");
			LOAD(settings.enableSharpen, "Enable Sharpen");
			LOAD(settings.sharpenSettings, "Sharpen");

			return true;
		}
	};

	template<>
	struct convert<directional_light>
	{
		static bool decode(const Node& n, directional_light& sun)
		{
			if (!n.IsMap())
			{
				return false;
			}

			LOAD(sun.color, "Color");
			LOAD(sun.intensity, "Intensity");
			LOAD(sun.direction, "Direction");
			LOAD(sun.numShadowCascades, "Cascades");
			LOAD(sun.cascadeDistances, "Distances");
			LOAD(sun.bias, "Bias");
			LOAD(sun.blendDistances, "Blend distances");

			return true;
		}
	};

	template<>
	struct convert<transform_component>
	{
		static bool decode(const Node& n, transform_component& c)
		{
			if (!n.IsMap())
			{
				return false;
			}

			LOAD(c.rotation, "Rotation");
			LOAD(c.position, "Position");
			LOAD(c.scale, "Scale");

			return true;
		}
	};
}

void serializeSceneToDisk(scene& appScene, const render_camera& camera, const directional_light& sun, const renderer_settings& rendererSettings, const ref<pbr_environment>& environment)
{
	std::string filename = saveFileDialog("Scene files", "sc");
	if (filename.empty())
	{
		return;
	}

	YAML::Emitter out;
	out << YAML::BeginMap;

	out << YAML::Key << "Scene" << YAML::Value << "My scene";
	out << YAML::Key << "Camera" << YAML::Value << camera;
	out << YAML::Key << "Rendering" << YAML::Value << rendererSettings;
	out << YAML::Key << "Sun" << YAML::Value << sun;
	out << YAML::Key << "Environment" << environment->name;

	out << YAML::Key << "Entities"
		<< YAML::Value
		<< YAML::BeginSeq;


	appScene.forEachEntity([&out, &appScene](entt::entity entityID)
	{
		scene_entity entity = { entityID, appScene };

		// Only entities with tags are valid top level entities. All others are helpers like colliders and constraints.
		if (tag_component* tag = entity.getComponentIfExists<tag_component>())
		{
			out << YAML::BeginMap;

			out << YAML::Key << "Tag" << YAML::Value << tag->name;

			if (transform_component* transform = entity.getComponentIfExists<transform_component>())
			{
				out << YAML::Key << "Transform" << YAML::Value << *transform;
			}

			out << YAML::EndMap;
		}
	});

	out << YAML::EndSeq;

	out << YAML::EndMap;

	fs::create_directories(fs::path(filename).parent_path());

	std::ofstream fout(filename);
	fout << out.c_str();
}

bool deserializeSceneFromDisk(scene& appScene, render_camera& camera, directional_light& sun, renderer_settings& rendererSettings, std::string& environmentName)
{
	std::string filename = openFileDialog("Scene files", "sc");
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

	appScene = scene();

	std::string sceneName = n["Scene"].as<std::string>();

	LOAD(camera, "Camera");
	LOAD(rendererSettings, "Rendering");
	LOAD(sun, "Sun");

	LOAD(environmentName, "Environment");

	auto entitiesNode = n["Entities"];
	for (auto entityNode : entitiesNode)
	{
		std::string name = entityNode["Tag"].as<std::string>();
		scene_entity entity = appScene.createEntity(name.c_str());

		if (entityNode["Transform"])
		{
			auto transformNode = entityNode["Transform"];
			entity.addComponent<transform_component>(transformNode.as<transform_component>());
		}
	}

	return true;
}
