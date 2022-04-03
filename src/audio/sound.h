#pragma once

#include <xaudio2.h>

struct audio_sound
{
    fs::path path;
    bool stream;
    bool isSynth;

    HANDLE fileHandle = INVALID_HANDLE_VALUE;
    WAVEFORMATEXTENSIBLE wfx = {};
    uint32 chunkSize;
    uint32 chunkPosition;
    BYTE* dataBuffer = 0;

    virtual ~audio_sound();

    virtual uint32 getSynthSamples(float* buffer, uint32 offset, uint32 numSamples) const { return 0; };
};

ref<audio_sound> getSound(uint32 id);


bool loadSound(uint32 id, const fs::path& path, bool stream);

template <typename synth_t>
bool loadSound(uint32 id, const synth_t& synth)
{
    bool checkForExistingSound(uint32 id, bool stream);
    void registerSound(uint32 id, const ref<audio_sound>&sound);

    struct synth_sound : audio_sound
    {
        synth_t synth;

        virtual uint32 getSynthSamples(float* buffer, uint32 offset, uint32 numSamples) const override { return synth.getSamples(buffer, offset, numSamples); };
    };


    if (checkForExistingSound(id, true))
    {
        return true;
    }
    else
    {
        WAVEFORMATEX wfx = {};
        wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        wfx.nChannels = synth.numChannels;
        wfx.nSamplesPerSec = synth.sampleHz;
        wfx.wBitsPerSample = synth.numChannels * (uint32)sizeof(float) * 8;
        wfx.nBlockAlign = synth.numChannels * (uint32)sizeof(float);
        wfx.nAvgBytesPerSec = synth.sampleHz * synth.numChannels * (uint32)sizeof(float);
        wfx.cbSize = 0; // Set to zero for PCM or IEEE float.


        ref<synth_sound> sound = make_ref<synth_sound>();
        sound->synth = synth;
        sound->stream = true;
        sound->wfx = { wfx };
        sound->dataBuffer = 0;
        sound->isSynth = true;

        registerSound(id, sound);

        return true;
    }
}

