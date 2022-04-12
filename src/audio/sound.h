#pragma once

#include "synth.h"
#include "core/string.h"
#include "core/asset.h"

#include <xaudio2.h>
#include <functional>


// Don't create this manually. Use SOUND_ID below.
struct sound_id
{
    const char* id;
    uint64 hash;
};

#define SOUND_ID(id) sound_id{ id, force_consteval<hashString64(id)> }

enum sound_type
{
    sound_type_music,
    sound_type_sfx,

    sound_type_count,
};

static const char* soundTypeNames[] =
{
    "Music",
    "Effects",
};

struct sound_spec
{
    asset_handle asset;
    sound_type type;
    bool stream;
};


struct audio_sound
{
    sound_id id;

    fs::path path;
    bool stream;
    bool isSynth;

    HANDLE fileHandle = INVALID_HANDLE_VALUE;
    WAVEFORMATEXTENSIBLE wfx = {};
    uint32 chunkSize;
    uint32 chunkPosition;
    BYTE* dataBuffer = 0;

    sound_type type;

    virtual ~audio_sound();

    virtual audio_synth* createSynth(void* buffer) const { return 0; }
};

struct sound_settings
{
    float volume = 1.f;
    float pitch = 1.f;
    float radius = 30.f;
    bool loop = false;

    float volumeFadeTime = 0.1f;
    float pitchFadeTime = 0.1f;
};

ref<audio_sound> getSound(const sound_id& id);


void unloadSound(const sound_id& id);
void unloadAllSounds();

bool loadFileSound(const sound_id& id);

template <typename synth_t, typename... args>
static bool loadSynthSound(const char* idStr, sound_type type, bool stream, const args&... a)
{
    static_assert(std::is_base_of_v<audio_synth, synth_t>, "Synthesizer must inherit from audio_synth");
    static_assert(sizeof(synth_t) <= MAX_SYNTH_SIZE);

    sound_id id = { idStr, hashString64(idStr) };
    sound_spec spec = { {}, type, stream };

    bool checkForExistingSound(const sound_id&);
    void registerSound(const sound_id&, const ref<audio_sound>&);

    if (checkForExistingSound(id))
    {
        return true;
    }
    else
    {
        struct synth_sound : audio_sound
        {
            std::function<audio_synth* (void*)> createFunc;

            virtual audio_synth* createSynth(void* buffer) const override
            {
                return createFunc(buffer);
            }
        };


        ref<audio_sound> sound;

        WAVEFORMATEX wfx = {};
        wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        wfx.nChannels = synth_t::numChannels;
        wfx.nSamplesPerSec = synth_t::sampleHz;
        wfx.wBitsPerSample = synth_t::numChannels * (uint32)sizeof(float) * 8;
        wfx.nBlockAlign = synth_t::numChannels * (uint32)sizeof(float);
        wfx.nAvgBytesPerSec = synth_t::sampleHz * synth_t::numChannels * (uint32)sizeof(float);
        wfx.cbSize = 0; // Set to zero for PCM or IEEE float.

        BYTE* dataBuffer = 0;
        uint32 dataSize = 0;

        if (!stream)
        {
            sound = make_ref<audio_sound>();

            synth_t synth(a...);
            uint32 totalNumSamples = (uint32)(synth.getDuration() * synth_t::numChannels * synth_t::sampleHz);
            uint32 size = sizeof(float) * totalNumSamples;

            dataBuffer = new BYTE[size];
            synth.getSamples((float*)dataBuffer, totalNumSamples);

            dataSize = size;
        }
        else
        {
            auto s = make_ref<synth_sound>();

            s->createFunc = [=](void* buffer)
            {
                return new(buffer) synth_t(a...);
            };

            sound = s;
        }

        sound->id = id;
        sound->stream = stream;
        sound->wfx = { wfx };
        sound->dataBuffer = dataBuffer;
        sound->chunkSize = dataSize;
        sound->isSynth = true;
        sound->type = type;

        registerSound(id, sound);

        return true;
    }
}

bool isSoundExtension(const fs::path& extension);
bool isSoundExtension(const std::string& extension);


namespace std
{
    template<>
    struct hash<sound_id>
    {
        size_t operator()(const sound_id& x) const
        {
            return x.hash; // Already hashed.
        }
    };
}

static bool operator==(const sound_id& a, const sound_id& b)
{
    return a.hash == b.hash; // TODO: Proper compare.
}
