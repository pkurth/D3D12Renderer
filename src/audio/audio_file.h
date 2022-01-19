#pragma once

#include <xaudio2.h>


// Little endian.
#define fourccRIFF 'FFIR'
#define fourccDATA 'atad'
#define fourccFMT ' tmf'
#define fourccWAVE 'EVAW'
#define fourccXWMA 'AMWX'
#define fourccDPDS 'sdpd'

struct audio_file
{
    HANDLE fileHandle = INVALID_HANDLE_VALUE;
    WAVEFORMATEXTENSIBLE wfx;

    DWORD dataChunkSize;
    DWORD dataChunkPosition;
};

audio_file openAudioFile(const fs::path& path);
bool readChunkData(const audio_file& file, void* buffer, DWORD buffersize, DWORD bufferoffset);