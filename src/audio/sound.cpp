#include "pch.h"
#include "sound.h"
#include "audio.h"
#include "sound_management.h"

#include "core/log.h"
#include "asset/file_registry.h"

#include <unordered_map>

#define fourccRIFF 'FFIR'
#define fourccDATA 'atad'
#define fourccFMT ' tmf'
#define fourccWAVE 'EVAW'
#define fourccXWMA 'AMWX'
#define fourccDPDS 'sdpd'


static HANDLE openFile(const fs::path& path);
static void closeFile(HANDLE& fileHandle);
static bool getWFX(HANDLE fileHandle, const fs::path& path, WAVEFORMATEXTENSIBLE& wfx);
static bool findChunk(HANDLE fileHandle, uint32 fourcc, uint32& chunkSize, uint32& chunkDataPosition);
static bool readChunkData(HANDLE fileHandle, void* buffer, uint32 buffersize, uint32 bufferoffset);


static std::unordered_map<uint64, ref<audio_sound>> fileSounds;
static std::unordered_map<uint64, ref<audio_sound>> synthSounds;


bool checkForExistingFileSound(sound_id id) 
{ 
    auto it = fileSounds.find(id.hash);
    return (it != fileSounds.end()) && it->second != 0;
}

bool checkForExistingSynthSound(sound_id id) 
{ 
    auto it = synthSounds.find(id.hash);
    return (it != synthSounds.end()) && it->second != 0;
}

void registerSound(sound_id id, const ref<audio_sound>& sound)
{
    if (!sound->isSynth)
    {
        fileSounds[id.hash] = sound;
    }
    else
    {
        synthSounds[id.hash] = sound;
    }
}

bool loadFileSound(sound_id id)
{
    if (checkForExistingFileSound(id))
    {
        return true;
    }
    else
    {
        const sound_spec& spec = getSoundSpec(id);

        fs::path path = getPathFromAssetHandle(spec.asset);
        if (!path.empty())
        {
            HANDLE fileHandle = openFile(path);
            if (fileHandle != INVALID_HANDLE_VALUE)
            {
                WAVEFORMATEXTENSIBLE wfx;
                uint32 chunkSize, chunkPosition;

                bool success = false;
                BYTE* dataBuffer = 0;

                // Find and retrieve format chunk.
                if (getWFX(fileHandle, path, wfx))
                {
                    // Find data chunk.
                    if (findChunk(fileHandle, fourccDATA, chunkSize, chunkPosition))
                    {
                        success = true;

                        if (!spec.stream)
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
                }

                if (success)
                {
                    ref<audio_sound> sound = make_ref<audio_sound>();
                    sound->id = id;
                    sound->path = path;
                    sound->stream = spec.stream;
                    sound->fileHandle = fileHandle;
                    sound->wfx = wfx;
                    sound->chunkSize = chunkSize;
                    sound->chunkPosition = chunkPosition;
                    sound->dataBuffer = dataBuffer;
                    sound->isSynth = false;
                    sound->type = spec.type;

                    registerSound(id, sound);
                }
                else
                {
                    closeFile(fileHandle);
                }

                return success;
            }
        }

        return false;
    }
}

void unloadSound(sound_id id)
{
    fileSounds.erase(id.hash);
    synthSounds.erase(id.hash);
}

void unloadAllSounds()
{
    fileSounds.clear();
    synthSounds.clear();
}

ref<audio_sound> getSound(sound_id id)
{
    auto it = fileSounds.find(id.hash);
    ref<audio_sound> result = (it != fileSounds.end()) ? it->second : 0;
    if (result) { return result; }

    it = synthSounds.find(id.hash);
    result = (it != synthSounds.end()) ? it->second : 0;
    return result;
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

static bool getWFX(HANDLE fileHandle, const fs::path& path, WAVEFORMATEXTENSIBLE& wfx)
{
    uint32 chunkSize, chunkPosition;

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

