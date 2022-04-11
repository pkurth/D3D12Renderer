#include "pch.h"
#include "sound_management.h"
#include "sound.h"

#include "core/log.h"
#include "core/imgui.h"
#include "core/asset.h"
#include "core/yaml.h"




bool soundEditorWindowOpen = false;


struct sound_spec
{
    asset_handle asset;
    sound_type type;
    bool stream;
};

static std::unordered_map<std::string, sound_spec> soundRegistry;
static const fs::path registryPath = fs::path(L"resources/sounds.yaml").lexically_normal();


static constexpr const uint32 collisionSoundTableSize = physics_material_type_count * (physics_material_type_count + 1) / 2;
static std::string collisionSoundEvents[collisionSoundTableSize];



void loadSoundRegistry()
{
    std::ifstream stream(registryPath);
    YAML::Node n = YAML::Load(stream);

    soundRegistry.clear();

    for (auto entryNode : n)
    {
        std::string name;
        sound_spec spec = {};

        YAML_LOAD(entryNode, name, "Name");
        YAML_LOAD(entryNode, spec.asset, "Asset");
        YAML_LOAD_ENUM(entryNode, spec.type, "Type");
        YAML_LOAD(entryNode, spec.stream, "Stream");

        soundRegistry.insert({ name, spec });
    }
}

static void saveSoundRegistry()
{
    YAML::Emitter out;
    out << YAML::BeginSeq;

    for (const auto& [name, spec] : soundRegistry)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "Name" << YAML::Value << name;
        out << YAML::Key << "Asset" << YAML::Value << spec.asset;
        out << YAML::Key << "Type" << YAML::Value << (uint32)spec.type;
        out << YAML::Key << "Stream" << YAML::Value << spec.stream;
        out << YAML::EndMap;
    }

    out << YAML::EndSeq;

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

            if (ImGui::BeginTree("Sound registry"))
            {
                static bool dirty = false;

                ImGui::Text("CREATE NEW SOUND");
                {
                    static char name[64];
                    static sound_spec spec = { 0, sound_type_music, false };

                    if (ImGui::BeginProperties())
                    {
                        ImGui::PropertyInputText("Name", name, sizeof(name));
                        ImGui::PropertyAssetHandle("Asset", "content_browser_audio", spec.asset);
                        ImGui::PropertyDropdown("Type", soundTypeNames, sound_type_count, (uint32&)spec.type);
                        ImGui::PropertyCheckbox("Stream", spec.stream);

                        ImGui::EndProperties();
                    }

                    bool createable = name[0] != '\0' && spec.asset;

                    if (ImGui::DisableableButton("Create", createable))
                    {
                        soundRegistry.insert({ name, spec });

                        name[0] = '\0';
                        spec = {};

                        dirty = true;
                    }

                    if (!createable)
                    {
                        ImGui::SameLine();
                        ImGui::TextColored(ImColor(212, 68, 82), "  Missing name and/or asset");
                    }
                }


                if (soundRegistry.size() > 0)
                {
                    ImGui::Separator();
                    ImGui::Separator();
                    ImGui::Separator();

                    ImGui::Text("EXISTING SOUNDS");

                    if (ImGui::DisableableButton("Save registry", dirty))
                    {
                        saveSoundRegistry();
                        dirty = false;
                    }


                    static ImGuiTextFilter filter;
                    filter.Draw();

                    for (auto& [name, spec] : soundRegistry)
                    {
                        if (filter.PassFilter(name.c_str()))
                        {
                            if (ImGui::BeginTree(name.c_str()))
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

                ImGui::EndTree();
            }


            // Sound events.
            if (ImGui::BeginTree("Sound events"))
            {
                if (ImGui::BeginTree("Collision sounds"))
                {
                    if (collisionSoundEvents[0].capacity() == 0)
                    {
                        for (uint32 i = 0; i < collisionSoundTableSize; ++i)
                        {
                            collisionSoundEvents[i].reserve(64);
                        }
                    }

                    if (ImGui::BeginTable("##collision", physics_material_type_count + 1))
                    {
                        ImU32 nameBG = ImColor(ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);

                        ImGui::TableNextColumn();
                        ImGui::Text("");
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, nameBG);

                        for (uint32 x = 0; x < physics_material_type_count; ++x)
                        {
                            ImGui::TableNextColumn();
                            ImGui::Text(physicsMaterialTypeNames[x]);
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, nameBG);
                        }

                        uint32 bufferIndex = 0;

                        for (uint32 y = 0; y < physics_material_type_count; ++y)
                        {
                            ImGui::TableNextColumn();
                            ImGui::Text(physicsMaterialTypeNames[y]);
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, nameBG);

                            for (uint32 x = 0; x < physics_material_type_count; ++x)
                            {
                                ImGui::TableNextColumn();

                                if (x >= y)
                                {
                                    ImGui::PushItemWidth(-1);
                                    ImGui::PushID(bufferIndex);
                                    std::string& s = collisionSoundEvents[bufferIndex];
                                    ImGui::InputText("", (char*)s.c_str(), s.capacity());
                                    ImGui::PopID();
                                    ImGui::PopItemWidth();

                                    ++bufferIndex;
                                }
                            }
                        }

                        ImGui::EndTable();
                    }


                    ImGui::EndTree();
                }


                ImGui::EndTree();
            }

        }
        ImGui::End();
    }
}

const std::string& getCollisionSoundName(physics_material_type typeA, physics_material_type typeB)
{
    uint32 x = max(typeA, typeB);
    uint32 y = min(typeA, typeB);

    uint32 padding = y * (y + 1) / 2;

    uint32 i = physics_material_type_count * y + x;
    assert(i >= padding);

    uint32 index = i - padding;
    assert(index < collisionSoundTableSize);

    return collisionSoundEvents[index];
}

