#include "pch.h"
#include "audio_file.h"
#include "core/log.h"


static bool findChunk(HANDLE fileHandle, DWORD fourcc, DWORD& chunkSize, DWORD& chunkDataPosition)
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

bool readChunkData(HANDLE fileHandle, void* buffer, DWORD buffersize, DWORD bufferoffset)
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

bool readChunkData(const audio_file& file, void* buffer, DWORD buffersize, DWORD bufferoffset)
{
    return readChunkData(file.fileHandle, buffer, buffersize, bufferoffset);
}


audio_file openAudioFile(const fs::path& path)
{
    HANDLE fileHandle = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        LOG_ERROR("Could not open file '%ws'", path.c_str());
        return {};
    }


    DWORD chunkSize;
    DWORD chunkPosition;
    WAVEFORMATEXTENSIBLE wfx = { 0 };


    //Check the file type, should be fourccWAVE or 'XWMA'.
    if (!findChunk(fileHandle, fourccRIFF, chunkSize, chunkPosition))
    {
        CloseHandle(fileHandle);
        return {};
    }

    DWORD filetype;
    if (!readChunkData(fileHandle, &filetype, sizeof(DWORD), chunkPosition))
    {
        CloseHandle(fileHandle);
        return {};
    }

    if (filetype != fourccWAVE)
    {
        LOG_ERROR("File type of file '%ws' must be WAVE", path.c_str());
        CloseHandle(fileHandle);
        return {};
    }

    if (!findChunk(fileHandle, fourccFMT, chunkSize, chunkPosition))
    {
        CloseHandle(fileHandle);
        return {};
    }
    if (!readChunkData(fileHandle, &wfx, chunkSize, chunkPosition))
    {
        CloseHandle(fileHandle);
        return {};
    }

    if (!findChunk(fileHandle, fourccDATA, chunkSize, chunkPosition))
    {
        CloseHandle(fileHandle);
        return {};
    }

    audio_file result;
    result.fileHandle = fileHandle;
    result.wfx = wfx;
    result.dataChunkSize = chunkSize;
    result.dataChunkPosition = chunkPosition;
    return result;
}

