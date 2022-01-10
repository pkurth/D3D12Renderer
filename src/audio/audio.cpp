#include "pch.h"
#include "audio.h"
#include "core/math.h"

#include <xaudio2.h>

com<IXAudio2> audio::xaudio;

bool audio::initialize()
{
	uint32 flags = 0;
#ifdef _DEBUG
	flags |= XAUDIO2_DEBUG_ENGINE;
#endif
	checkResult(XAudio2Create(xaudio.GetAddressOf(), flags));
	checkResult(xaudio->CreateMasteringVoice(&masterVoice, 1));

	XAUDIO2_VOICE_DETAILS details;
	masterVoice->GetVoiceDetails(&details);
    numOutputChannels = details.InputChannels;

    notifyOnSettingsChange();

	return true;
}

void audio::shutdown()
{
    xaudio->StopEngine();
}

void audio::notifyOnSettingsChange()
{
    checkResult(masterVoice->SetVolume(masterVolume));
}

static XAUDIO2_BUFFER createBuffer(audio_voice* voice, uint32 index)
{
    XAUDIO2_BUFFER buffer = {};
    buffer.Flags = 0;
    buffer.AudioBytes = audio_voice::numBytesPerBuffer;
    buffer.PlayBegin = 0;
    buffer.PlayLength = 0;
    buffer.LoopBegin = 0;
    buffer.LoopLength = 0;
    buffer.LoopCount = 0;
    buffer.pContext = voice;
    buffer.pAudioData = (BYTE*)&voice->memory[index];
    return buffer;
}

struct voice_callback : IXAudio2VoiceCallback
{
    virtual void __stdcall OnVoiceProcessingPassStart(uint32 bytesRequired) override {}
    virtual void __stdcall OnVoiceProcessingPassEnd() override {}
    virtual void __stdcall OnStreamEnd() override {}
    virtual void __stdcall OnVoiceError(void* bufferContext, HRESULT error) override {}
    virtual void __stdcall OnBufferStart(void* bufferContext) override
    {
        // Called in the beginning of a buffer.
    }
    virtual void __stdcall OnBufferEnd(void* bufferContext) override
    {
        // Called after the last loop of a buffer.
        audio_voice* voice = (audio_voice*)bufferContext;
        voice->source->createSamples(voice->memory[voice->nextToWrite], audio_voice::numSamplesPerBuffer);

        XAUDIO2_BUFFER buffer = createBuffer(voice, voice->nextToWrite);
        checkResult(voice->voice->SubmitSourceBuffer(&buffer));

        voice->nextToWrite = 1 - voice->nextToWrite;
    }
    virtual void __stdcall OnLoopEnd(void* bufferContext) override
    {
        // Called after each buffer loop.
    }
};

static voice_callback callback;

bool audio::playSound(audio_source* source, float volume, float pitch)
{
    audio_voice* voice = new audio_voice;
    voice->source = source;
    sourceVoices.push_back(voice);

    source->createSamples(voice->memory[0], audio_voice::numSamplesPerBuffer);
    source->createSamples(voice->memory[1], audio_voice::numSamplesPerBuffer);


    WAVEFORMATEX waveFormat = {};
    waveFormat.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    waveFormat.nChannels = audio_voice::numChannels;
    waveFormat.nSamplesPerSec = audio_voice::sampleHz;
    waveFormat.wBitsPerSample = audio_voice::bytesPerChannel * 8;
    waveFormat.nBlockAlign = audio_voice::numChannels * audio_voice::bytesPerChannel;
    waveFormat.nAvgBytesPerSec = audio_voice::sampleHz * audio_voice::numChannels * audio_voice::bytesPerChannel;
    waveFormat.cbSize = 0; // Set to zero for PCM or IEEE float.

    checkResult(xaudio->CreateSourceVoice(&voice->voice, &waveFormat, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &callback));

    checkResult(voice->voice->Start());

    XAUDIO2_BUFFER buffer0 = createBuffer(voice, 0);
    checkResult(voice->voice->SubmitSourceBuffer(&buffer0));

    XAUDIO2_BUFFER buffer1 = createBuffer(voice, 1);
    checkResult(voice->voice->SubmitSourceBuffer(&buffer1));


    if (volume != 1.f)
    {
        checkResult(voice->voice->SetVolume(volume));
    }
    if (pitch != 1.f)
    {
        checkResult(voice->voice->SetFrequencyRatio(pitch));
    }

    return true;
}

uint32 sine_wave_audio_source::createSamples(float* buffer, uint32 numSamples)
{
    float hz = this->hz;
    uint32 offset = this->offset;
    for (uint32 i = 0; i < numSamples; ++i, ++offset)
    {
        buffer[i] = sin(offset * M_TAU / audio_voice::sampleHz * hz);
    }
    this->offset = offset;
    return numSamples;
}
