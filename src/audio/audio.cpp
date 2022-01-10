#include "pch.h"
#include "audio.h"
#include "core/math.h"

#include <xaudio2.h>
#include <numeric>

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

    std::iota(freeVoices, freeVoices + numFreeVoices, 0);

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

static XAUDIO2_BUFFER createBuffer(audio_voice& voice, uint32 index, uint32 numSamples)
{
    uint32 flags = 0;
    uint32 numBytes = numSamples * audio_source::bytesPerChannel;
    if (numBytes < voice.numBytesPerBuffer)
    {
        flags = XAUDIO2_END_OF_STREAM;
    }

    XAUDIO2_BUFFER buffer = {};
    buffer.Flags = flags;
    buffer.AudioBytes = numBytes;
    buffer.PlayBegin = 0;
    buffer.PlayLength = 0;
    buffer.LoopBegin = 0;
    buffer.LoopLength = 0;
    buffer.LoopCount = 0;
    buffer.pContext = &voice;
    buffer.pAudioData = (BYTE*)voice.buffers[index];
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
        audio_voice& voice = *(audio_voice*)bufferContext;

        if (voice.stopOn == voice.nextToWrite)
        {
            audio::retireVoice(voice);
            return;
        }

        uint32 numSamplesWritten = voice.source->createSamples(voice.buffers[voice.nextToWrite], voice.numSamplesPerBuffer);

        if (numSamplesWritten > 0)
        {
            XAUDIO2_BUFFER buffer = createBuffer(voice, voice.nextToWrite, numSamplesWritten);
            checkResult(voice.voice->SubmitSourceBuffer(&buffer));
        }

        voice.nextToWrite = 1 - voice.nextToWrite;

        if (numSamplesWritten < voice.numSamplesPerBuffer && voice.stopOn == -1)
        {
            // Reached end of sound.

            if (numSamplesWritten == 0)
            {
                voice.stopOn = voice.nextToWrite;
            }
            else
            {
                voice.stopOn = 1 - voice.nextToWrite;
            }
        }
    }
    virtual void __stdcall OnLoopEnd(void* bufferContext) override
    {
        // Called after each buffer loop.
    }
};

static voice_callback callback;

audio_handle audio::playSound(audio_source* source, float volume, float pitch)
{
    uint32 voiceIndex = freeVoices[--numFreeVoices];

    audio_voice& voice = sourceVoices[voiceIndex];
    voice.source = source;
    voice.volume = volume;
    voice.pitch = pitch;
    voice.nextToWrite = 0;
    voice.stopOn = -1;
    voice.paused = false;

    if (!source->streaming)
    {
        // This causes the sound to end as soon as the first buffer stops playing.
        voice.nextToWrite = 1;
        voice.stopOn = 1;
    }

    assert(source->sampleHz * source->numChannels * source->bufferedMilliseconds % 1000 == 0);

    voice.numSamplesPerBuffer = source->sampleHz * source->numChannels * source->bufferedMilliseconds / 1000;
    voice.numBytesPerBuffer = voice.numSamplesPerBuffer * audio_source::bytesPerChannel;

    uint32 numBuffers = source->streaming ? 2 : 1;
    voice.allMemory = new float[voice.numSamplesPerBuffer * numBuffers];
    voice.buffers[0] = voice.allMemory;
    voice.buffers[1] = voice.allMemory + voice.numSamplesPerBuffer;

    uint32 numSamplesWritten0 = source->createSamples(voice.buffers[0], voice.numSamplesPerBuffer);
    if (numSamplesWritten0)
    {
        WAVEFORMATEX waveFormat = {};
        waveFormat.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        waveFormat.nChannels = source->numChannels;
        waveFormat.nSamplesPerSec = source->sampleHz;
        waveFormat.wBitsPerSample = audio_source::bytesPerChannel * 8;
        waveFormat.nBlockAlign = source->numChannels * audio_source::bytesPerChannel;
        waveFormat.nAvgBytesPerSec = source->sampleHz * source->numChannels * audio_source::bytesPerChannel;
        waveFormat.cbSize = 0; // Set to zero for PCM or IEEE float.


        checkResult(xaudio->CreateSourceVoice(&voice.voice, &waveFormat, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &callback));
        checkResult(voice.voice->Start());

        XAUDIO2_BUFFER buffer0 = createBuffer(voice, 0, numSamplesWritten0);
        checkResult(voice.voice->SubmitSourceBuffer(&buffer0));
    }

    if (numBuffers == 2)
    {
        uint32 numSamplesWritten1 = source->createSamples(voice.buffers[1], voice.numSamplesPerBuffer);
        if (numSamplesWritten1 > 0)
        {
            XAUDIO2_BUFFER buffer1 = createBuffer(voice, 1, numSamplesWritten1);
            checkResult(voice.voice->SubmitSourceBuffer(&buffer1));
        }
    }

    if (volume != 1.f)
    {
        checkResult(voice.voice->SetVolume(volume));
    }
    if (pitch != 1.f)
    {
        checkResult(voice.voice->SetFrequencyRatio(pitch));
    }

    return { voice.generation, voiceIndex };
}

void audio::retireVoice(audio_voice& voice)
{
    ++voice.generation;
    voice.voice->DestroyVoice();
    delete[] voice.allMemory;
    uint32 index = (uint32)(&voice - sourceVoices);
    freeVoices[numFreeVoices++] = index;
}

void audio::pause(audio_handle handle)
{
    audio_voice& voice = sourceVoices[handle.index];
    if (voice.generation == handle.generation && !voice.paused)
    {
        voice.voice->Stop();
        voice.paused = true;
    }
}

void audio::resume(audio_handle handle)
{
    audio_voice& voice = sourceVoices[handle.index];
    if (voice.generation == handle.generation && voice.paused)
    {
        voice.voice->Start();
        voice.paused = false;
    }
}

void audio::stop(audio_handle handle)
{
    audio_voice& voice = sourceVoices[handle.index];
    if (voice.generation == handle.generation)
    {
        voice.voice->Stop();
        retireVoice(voice);
    }
}

uint32 sine_wave_audio_source::createSamples(float* buffer, uint32 numSamples)
{
    uint32 offset = this->offset;
    if (duration > 0.f)
    {
        uint32 numTotalSamples = (uint32)(duration * sampleHz);
        if (offset + numSamples > numTotalSamples)
        {
            numSamples = numTotalSamples - offset;
        }
    }

    float factor = M_TAU / sampleHz * hz;
    for (uint32 i = 0; i < numSamples; ++i, ++offset)
    {
        buffer[i] = sin(offset * factor);
    }
    this->offset = offset;
    return numSamples;
}
