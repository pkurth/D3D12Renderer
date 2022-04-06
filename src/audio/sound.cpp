#include "pch.h"
#include "sound.h"
#include "audio.h"

#include "core/log.h"
#include "core/imgui.h"
#include "core/asset.h"
#include "core/yaml.h"

#include <unordered_map>

#define fourccRIFF 'FFIR'
#define fourccDATA 'atad'
#define fourccFMT ' tmf'
#define fourccWAVE 'EVAW'
#define fourccXWMA 'AMWX'
#define fourccDPDS 'sdpd'


static HANDLE openFile(const fs::path& path);
static void closeFile(HANDLE& fileHandle);
static bool getWFX(HANDLE fileHandle, const fs::path& path, WAVEFORMATEXTENSIBLE& wfx, uint32& chunkSize, uint32& chunkPosition);
static bool readChunkData(HANDLE fileHandle, void* buffer, uint32 buffersize, uint32 bufferoffset);


static std::unordered_map<uint32, ref<audio_sound>> sounds;

bool checkForExistingSound(uint32 id, bool stream)
{
    auto it = sounds.find(id);

    if (it != sounds.end())
    {
        ref<audio_sound>& sound = it->second;
        assert(sound->stream == stream);

        return true;
    }

    return false;
}

void registerSound(uint32 id, const ref<audio_sound>& sound)
{
    sounds.insert({ id, sound });
}

bool loadFileSound(uint32 id, sound_type type, const fs::path& path, bool stream)
{
    if (checkForExistingSound(id, stream))
    {
        return true;
    }
    else
    {
        HANDLE fileHandle = openFile(path);
        if (fileHandle != INVALID_HANDLE_VALUE)
        {
            WAVEFORMATEXTENSIBLE wfx;
            uint32 chunkSize, chunkPosition;

            bool success = false;
            BYTE* dataBuffer = 0;

            if (getWFX(fileHandle, path, wfx, chunkSize, chunkPosition))
            {
                success = true;

                if (!stream)
                {
                    dataBuffer = new BYTE[chunkSize];
                    success = readChunkData(fileHandle, dataBuffer, chunkSize, chunkPosition);
                    if (!success)
                    {
                        delete[] dataBuffer;
                    }
                    
                    closeFile(fileHandle);
                }
            }

            if (success)
            {
                ref<audio_sound> sound = make_ref<audio_sound>();
                sound->path = path;
                sound->stream = stream;
                sound->fileHandle = fileHandle;
                sound->wfx = wfx;
                sound->chunkSize = chunkSize;
                sound->chunkPosition = chunkPosition;
                sound->dataBuffer = dataBuffer;
                sound->isSynth = false;
                sound->type = type;

                registerSound(id, sound);
            }
            else
            {
                closeFile(fileHandle);
            }

            return success;
        }

        return false;
    }
}

void unloadSound(uint32 id)
{
    sounds.erase(id);
}

ref<audio_sound> getSound(uint32 id)
{
    auto it = sounds.find(id);
    return (it != sounds.end()) ? it->second : 0;
}

audio_sound::~audio_sound()
{
    closeFile(fileHandle);
    if (dataBuffer)
    {
        delete[] dataBuffer;
    }
}







static HANDLE openFile(const fs::path& path)
{
    HANDLE fileHandle = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0, //FILE_FLAG_NO_BUFFERING,
        NULL);

    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        LOG_ERROR("Could not open file '%ws'", path.c_str());
    }

    return fileHandle;
}

static void closeFile(HANDLE& fileHandle)
{
    if (fileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(fileHandle);
        fileHandle = INVALID_HANDLE_VALUE;
    }
}

static bool findChunk(HANDLE fileHandle, uint32 fourcc, uint32& chunkSize, uint32& chunkDataPosition)
{
    if (SetFilePointer(fileHandle, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
    {
        LOG_ERROR("Could not set file pointer");
        return false;
    }

    DWORD chunkType;
    DWORD chunkDataSize;
    DWORD riffDataSize = 0;
    DWORD fileType;
    DWORD bytesRead = 0;
    DWORD offset = 0;

    HRESULT hr = S_OK;
    while (hr == S_OK)
    {
        DWORD dwRead;
        if (ReadFile(fileHandle, &chunkType, sizeof(DWORD), &dwRead, NULL) == 0)
        {
            LOG_ERROR("Could not read chunk type");
            return false;
        }

        if (ReadFile(fileHandle, &chunkDataSize, sizeof(DWORD), &dwRead, NULL) == 0)
        {
            LOG_ERROR("Could not read chunk data size");
            return false;
        }


        if (chunkType == fourccRIFF)
        {
            riffDataSize = chunkDataSize;
            chunkDataSize = 4;
            if (ReadFile(fileHandle, &fileType, sizeof(DWORD), &dwRead, NULL) == 0)
            {
                LOG_ERROR("Could not read file type");
                return false;
            }
        }
        else
        {
            if (SetFilePointer(fileHandle, chunkDataSize, NULL, FILE_CURRENT) == INVALID_SET_FILE_POINTER)
            {
                LOG_ERROR("Could not set file pointer");
                return false;
            }
        }

        offset += sizeof(DWORD) * 2;

        if (chunkType == fourcc)
        {
            chunkSize = chunkDataSize;
            chunkDataPosition = offset;
            return true;
        }

        offset += chunkDataSize;

        if (bytesRead >= riffDataSize)
        {
            return false;
        }
    }

    return true;
}

static bool readChunkData(HANDLE fileHandle, void* buffer, uint32 buffersize, uint32 bufferoffset)
{
    if (SetFilePointer(fileHandle, bufferoffset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
    {
        LOG_ERROR("Could not set file pointer");
        return false;
    }

    DWORD dwRead;
    if (ReadFile(fileHandle, buffer, buffersize, &dwRead, NULL) == 0)
    {
        LOG_ERROR("Could not read chunk");
        return false;
    }

    return true;
}

static bool getWFX(HANDLE fileHandle, const fs::path& path, WAVEFORMATEXTENSIBLE& wfx, uint32& chunkSize, uint32& chunkPosition)
{
    //Check the file type, should be fourccWAVE or 'XWMA'.
    if (!findChunk(fileHandle, fourccRIFF, chunkSize, chunkPosition))
    {
        return false;
    }

    DWORD filetype;
    if (!readChunkData(fileHandle, &filetype, sizeof(DWORD), chunkPosition))
    {
        return false;
    }

    if (filetype != fourccWAVE)
    {
        LOG_ERROR("File type of file '%ws' must be WAVE", path.c_str());
        return false;
    }

    if (!findChunk(fileHandle, fourccFMT, chunkSize, chunkPosition))
    {
        return false;
    }
    if (!readChunkData(fileHandle, &wfx, chunkSize, chunkPosition))
    {
        return false;
    }

    if (!findChunk(fileHandle, fourccDATA, chunkSize, chunkPosition))
    {
        return false;
    }

    return true;
}


bool isSoundExtension(const fs::path& extension)
{
    return extension == ".wav";
}

bool isSoundExtension(const std::string& extension)
{
    return extension == ".wav";
}


bool soundEditorWindowOpen = false;


struct sound_spec
{
    asset_handle asset;
    sound_type type;
    bool stream;
};

static std::unordered_map<std::string, sound_spec> soundRegistry;
static const fs::path registryPath = fs::path(L"resources/sounds.yaml").lexically_normal();

void drawSoundEditor()
{
    if (soundEditorWindowOpen)
    {
        if (ImGui::Begin(ICON_FA_VOLUME_UP "  Sound Editing", &soundEditorWindowOpen))
        {
            static bool dirty = false;


            ImGui::Text("CREATE NEW SOUND");
            {
                static char name[64];
                static asset_handle asset;
                static sound_type type = sound_type_music;
                static bool stream = false;

                if (ImGui::BeginProperties())
                {
                    ImGui::PropertyInputText("Name", name, sizeof(name));
                    ImGui::PropertyAssetHandle("Asset", "content_browser_audio", asset);
                    ImGui::PropertyDropdown("Type", soundTypeNames, sound_type_count, (uint32&)type);
                    ImGui::PropertyCheckbox("Stream", stream);

                    ImGui::EndProperties();
                }

                bool createable = name[0] != '\0' && asset;

                if (ImGui::DisableableButton("Create", createable))
                {
                    soundRegistry.insert({ name, sound_spec{ asset, type, stream } });

                    name[0] = '\0';
                    asset.value = 0;

                    dirty = true;
                }

                if (!createable)
                {
                    ImGui::SameLine();
                    ImGui::TextColored(ImColor(212, 68, 82), "  Missing name and/or asset");
                }
            }



            ImGui::Separator();
            ImGui::Separator();
            ImGui::Separator();

            ImGui::Text("EXISTING SOUNDS");

            if (ImGui::DisableableButton("Save registry", dirty))
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

                dirty = false;
            }

            {
                static ImGuiTextFilter filter;
                filter.Draw();

                if (ImGui::BeginChild("##SoundList"))
                {
                    uint32 i = 0;
                    for (auto& [name, spec] : soundRegistry)
                    {
                        ImGui::PushID(i);
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
                        ImGui::PopID();
                        ++i;
                    }
                }
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }
}
