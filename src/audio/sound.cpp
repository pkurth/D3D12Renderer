#include "pch.h"
#include "sound.h"
#include "audio.h"
#include "core/log.h"

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

bool loadSound(uint32 id, const fs::path& path, bool stream)
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
        return {};
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

audio_sound::~audio_sound()
{
    closeFile(fileHandle);
    if (dataBuffer)
    {
        delete[] dataBuffer;
    }
}
