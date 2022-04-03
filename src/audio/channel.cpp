#include "pch.h"
#include "channel.h"



constexpr uint32 MAX_BUFFER_COUNT = 3;
constexpr uint32 STREAMING_BUFFER_SIZE = (1024 * 8 * 6);

audio_channel::audio_channel(const com<IXAudio2>& xaudio, const ref<audio_sound>& sound, float volume, bool loop)
{
	volume = max(0.f, volume);

	this->sound = sound;
	this->voiceCallback.channel = this;
	this->loop = loop;

	checkResult(xaudio->CreateSourceVoice(&voice, (WAVEFORMATEX*)&sound->wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &voiceCallback));
	checkResult(voice->Start());

	volumeFader.initialize(volume);
	stopFader.initialize(1.f);
	updateVolume(0.f);


	if (sound->stream)
	{
		threadStopped = false;
		bufferEndSemaphore = CreateSemaphore(0, 0, MAX_BUFFER_COUNT, 0);

		auto func = (sound->isSynth) ? streamSynthAudio : streamFileAudio;
		threadHandle = CreateThread(0, 0, func, this, 0, 0);
	}
	else
	{
		XAUDIO2_BUFFER buffer = { 0 };
		buffer.AudioBytes = sound->chunkSize;
		buffer.pAudioData = sound->dataBuffer;
		if (loop)
		{
			buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
		}
		else
		{
			buffer.Flags = XAUDIO2_END_OF_STREAM;
		}

		checkResult(voice->SubmitSourceBuffer(&buffer));
	}
}

audio_channel::~audio_channel()
{
	voice->DestroyVoice();
	if (sound->stream)
	{
		CloseHandle(bufferEndSemaphore);
		CloseHandle(threadHandle);
	}
}

void audio_channel::setVolume(float volume, float fadeTime)
{
	volume = max(0.f, volume);
	volumeFader.startFade(volume, fadeTime);
}

void audio_channel::update(float dt)
{
	switch (state)
	{
		case channel_state_to_play:
		{
			state = channel_state_playing;
			if (stopRequested)
			{
				state = channel_state_stopping;
			}
		} break;

		case channel_state_playing:
		{
			updateVolume(dt);
			if (stopRequested)
			{
				state = channel_state_stopping;
			}
		} break;

		case channel_state_stopping:
		{
			stopFader.update(dt);
			updateVolume(dt);
			if (stopFader.current <= 0.f)
			{
				voice->Stop();
				state = channel_state_stopped;

				if (sound->stream)
				{
					ReleaseSemaphore(bufferEndSemaphore, 1, 0);
				}
			}
		} break;

		case channel_state_stopped:
		{
			stopRequested = false;
		} break;

		default:
			break;
	}
}

void audio_channel::stop(float fadeOutTime)
{
	if (state != channel_state_stopping && state != channel_state_stopped)
	{
		stopRequested = true;
		stopFader.startFade(0.f, fadeOutTime);
	}
}

bool audio_channel::canBeKilled()
{
	return state == channel_state_stopped && threadStopped;
}

void audio_channel::updateVolume(float dt)
{
	volumeFader.update(dt);

	float v = volumeFader.current * stopFader.current;
	if (v != oldVolume)
	{
		voice->SetVolume(v);
		oldVolume = v;
	}
}









static DWORD WINAPI streamFileAudio(void* parameter)
{
	audio_channel* channel = (audio_channel*)parameter;
	auto sound = channel->sound;
	auto voice = channel->voice;

	BYTE buffers[MAX_BUFFER_COUNT][STREAMING_BUFFER_SIZE];

	uint32 currentBufferIndex = 0;


	bool quit = false;

	while (!quit)
	{
		uint32 currentPosition = 0;

		if (SetFilePointer(sound->fileHandle, sound->chunkPosition, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
		{
			break;
		}

		while (!quit && currentPosition < sound->chunkSize)
		{
			DWORD size = STREAMING_BUFFER_SIZE;
			if (ReadFile(sound->fileHandle, buffers[currentBufferIndex], size, &size, 0) == 0)
			{
				quit = true;
				break;
			}

			currentPosition += size;

			XAUDIO2_BUFFER buffer = { 0 };
			buffer.AudioBytes = size;
			buffer.pAudioData = buffers[currentBufferIndex];
			buffer.pContext = channel->bufferEndSemaphore;

			checkResult(voice->SubmitSourceBuffer(&buffer));

			XAUDIO2_VOICE_STATE state;
			voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
			while (!quit && state.BuffersQueued > MAX_BUFFER_COUNT - 1)
			{
				WaitForSingleObject(channel->bufferEndSemaphore, INFINITE);
				voice->GetState(&state);

				if (channel->state == channel_state_stopped)
				{
					quit = true;
				}
			}
			currentBufferIndex++;
			currentBufferIndex %= MAX_BUFFER_COUNT;
		}

		if (!channel->loop)
		{
			break;
		}
	}

	if (!quit)
	{
		XAUDIO2_VOICE_STATE state;
		while (voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED), state.BuffersQueued > 0)
		{
			WaitForSingleObject(channel->bufferEndSemaphore, INFINITE);
		}
	}

	channel->stop(0.f);

	channel->threadStopped = true;

	return 0;
}

static DWORD WINAPI streamSynthAudio(void* parameter)
{
	audio_channel* channel = (audio_channel*)parameter;
	auto sound = channel->sound;
	auto voice = channel->voice;

	float buffers[MAX_BUFFER_COUNT][STREAMING_BUFFER_SIZE];

	uint32 currentBufferIndex = 0;


	bool quit = false;

	while (!quit)
	{
		uint32 currentPosition = 0;

		uint32 offset = 0;

		while (true)
		{
			uint32 size = STREAMING_BUFFER_SIZE;
			size = sound->getSynthSamples(buffers[currentBufferIndex], offset, size);

			if (size == 0)
			{
				break;
			}

			offset += size;

			currentPosition += size;

			XAUDIO2_BUFFER buffer = { 0 };
			buffer.AudioBytes = size * sizeof(float);
			buffer.pAudioData = (BYTE*)buffers[currentBufferIndex];
			buffer.pContext = channel->bufferEndSemaphore;

			checkResult(voice->SubmitSourceBuffer(&buffer));

			XAUDIO2_VOICE_STATE state;
			voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
			while (!quit && state.BuffersQueued > MAX_BUFFER_COUNT - 1)
			{
				WaitForSingleObject(channel->bufferEndSemaphore, INFINITE);
				voice->GetState(&state);

				if (channel->state == channel_state_stopped)
				{
					quit = true;
				}
			}
			currentBufferIndex++;
			currentBufferIndex %= MAX_BUFFER_COUNT;
		}

		if (!channel->loop)
		{
			break;
		}
	}

	if (!quit)
	{
		XAUDIO2_VOICE_STATE state;
		while (voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED), state.BuffersQueued > 0)
		{
			WaitForSingleObject(channel->bufferEndSemaphore, INFINITE);
		}
	}

	channel->stop(0.f);

	channel->threadStopped = true;

	return 0;
}


