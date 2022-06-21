#include "pch.h"
#include "sound_management.h"
#include "audio.h"

#include "core/log.h"
#include "core/imgui.h"
#include "core/yaml.h"

#include "sound_ids.h"



bool soundEditorWindowOpen = false;


static sound_spec soundRegistry[sound_id_count];
static const fs::path registryPath = fs::path(L"resources/sounds.yaml").lexically_normal();



void loadSoundRegistry()
{
    std::ifstream stream(registryPath);
    YAML::Node n = YAML::Load(stream);

    for (uint32 i = 0; i < sound_id_count; ++i)
    {
        if (auto entryNode = n[soundIDNames[i]])
        {
            sound_spec spec = {};

            YAML_LOAD(entryNode, spec.asset, "Asset");
            YAML_LOAD_ENUM(entryNode, spec.type, "Type");
            YAML_LOAD(entryNode, spec.stream, "Stream");

            soundRegistry[i] = spec;
        }
    }
}

static void saveSoundRegistry()
{
    YAML::Emitter out;
    out << YAML::BeginMap;

    for (uint32 i = 0; i < sound_id_count; ++i)
    {
        sound_spec spec = soundRegistry[i];

        out << YAML::Key << soundIDNames[i] << YAML::Value;
        
        out << YAML::BeginMap;
        out << YAML::Key << "Asset" << YAML::Value << spec.asset;
        out << YAML::Key << "Type" << YAML::Value << (uint32)spec.type;
        out << YAML::Key << "Stream" << YAML::Value << spec.stream;
        out << YAML::EndMap;
    }

    out << YAML::EndMap;

    fs::create_directories(registryPath.parent_path());

    LOG_MESSAGE("Rewriting sound registry");
    std::ofstream fout(registryPath);
    fout << out.c_str();
}

void drawSoundEditor()
{
    if (soundEditorWindowOpen)
    {
        if (ImGui::Begin(ICON_FA_VOLUME_UP "  Sound Editing", &soundEditorWindowOpen))
        {
            static ImGuiTextFilter filter;
            filter.Draw();

            static bool dirty = false;

            if (ImGui::DisableableButton("Save registry", dirty))
            {
                saveSoundRegistry();

                unloadAllSounds(); // Unload, such that the spec gets reloaded when the sound is played the next time.
                restartAllSounds();

                dirty = false;
            }

            for (uint32 i = 0; i < sound_id_count; ++i)
            {
                if (filter.PassFilter(soundIDNames[i]))
                {
                    sound_spec& spec = soundRegistry[i];

                    if (ImGui::BeginTree(soundIDNames[i]))
                    {
                        if (ImGui::BeginProperties())
                        {
                            dirty |= ImGui::PropertyAssetHandle("Asset", "content_browser_audio", spec.asset);
                            dirty |= ImGui::PropertyDropdown("Type", soundTypeNames, sound_type_count, (uint32&)spec.type);
                            dirty |= ImGui::PropertyCheckbox("Stream", spec.stream);

                            ImGui::EndProperties();
                        }

                        ImGui::EndTree();
                    }
                }
            }
        }
        ImGui::End();
    }
}

sound_spec getSoundSpec(const sound_id& id)
{
    return soundRegistry[id];
}
