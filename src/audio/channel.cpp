#include "pch.h"
#include "channel.h"


#define MAX_BUFFER_COUNT 3
#define STREAMING_BUFFER_SIZE (1024 * 8 * 6)

#define UPDATE_3D_PERIOD 3 // > 0.




audio_channel::audio_channel(const audio_context& context, const ref<audio_sound>& sound, const sound_settings& settings, bool keepReferenceToSettings)
{
	initialize(context, sound, settings, keepReferenceToSettings, false);
}

audio_channel::audio_channel(const audio_context& context, const ref<audio_sound>& sound, vec3 position, const sound_settings& settings, bool keepReferenceToSettings)
{
	initialize(context, sound, settings, keepReferenceToSettings, true, position);
}

void audio_channel::initialize(const audio_context& context, const ref<audio_sound>& sound, const sound_settings& settings, bool keepReferenceToSettings, bool positioned, vec3 position)
{
	this->sound = sound;
	this->voiceCallback.channel = this;
	this->loop = settings.loop;

	if (keepReferenceToSettings)
	{
		userSettings = &settings;
	}

	this->positioned = positioned;
	this->position = position;

	checkResult(context.xaudio->CreateSourceVoice(&voice, (WAVEFORMATEX*)&sound->wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &voiceCallback));
	checkResult(voice->Start());

	volumeFader.initialize(settings.volume);
	pitchFader.initialize(settings.pitch);

	oldUserVolume = settings.volume;
	oldUserPitch = settings.pitch;

	stopFader.initialize(1.f);
	
	updateSoundSettings(0.f);

	update3D(context);

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
		if (settings.loop)
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

void audio_channel::update(const audio_context& context, float dt)
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
			updateSoundSettings(dt);
			update3D(context);
			if (stopRequested)
			{
				state = channel_state_stopping;
			}
		} break;

		case channel_state_stopping:
		{
			stopFader.update(dt);
			updateSoundSettings(dt);
			update3D(context);
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

bool audio_channel::hasStopped()
{
	return state == channel_state_stopped && threadStopped;
}

void audio_channel::updateSoundSettings(float dt)
{
	if (userSettings)
	{
		if (userSettings->volume != oldUserVolume)
		{
			volumeFader.startFade(userSettings->volume, userSettings->volumeFadeTime);
			oldUserVolume = userSettings->volume;
		}
		if (userSettings->pitch != oldUserPitch)
		{
			pitchFader.startFade(userSettings->pitch, userSettings->pitchFadeTime);
			oldUserPitch = userSettings->pitch;
		}

		loop = userSettings->loop;
	}

	volumeFader.update(dt);
	pitchFader.update(dt);

	float v = volumeFader.current * stopFader.current;
	if (v != oldVolume)
	{
		voice->SetVolume(v);
		oldVolume = v;
	}

	float p = pitchFader.current;
	if (p != oldPitch)
	{
		voice->SetFrequencyRatio(p);
		oldPitch = p;
	}
}

void audio_channel::update3D(const audio_context& context)
{
	if (!positioned)
	{
		return;
	}

	if (update3DTimer == 0)
	{
		quat rotation = quat::identity; // TODO.

		vec3 forward = rotation * vec3(0.f, 0.f, -1.f);
		vec3 up = rotation * vec3(0.f, 1.f, 0.f);


		X3DAUDIO_EMITTER emitter = { 0 };
		emitter.ChannelCount = 1;
		emitter.CurveDistanceScaler = 1.f;
		emitter.OrientFront = { forward.x, forward.y, -forward.z };
		emitter.OrientTop = { up.x, up.y, -up.z };
		emitter.Position = { position.x, position.y, -position.z };

		float matrix[8] = {};


		X3DAUDIO_DSP_SETTINGS dspSettings = { 0 };
		dspSettings.SrcChannelCount = sound->wfx.Format.nChannels;
		dspSettings.DstChannelCount = context.masterVoiceDetails.InputChannels;
		dspSettings.pMatrixCoefficients = matrix;

		X3DAudioCalculate(context.xaudio3D, &context.listener, &emitter,
			X3DAUDIO_CALCULATE_MATRIX/* | X3DAUDIO_CALCULATE_DOPPLER | X3DAUDIO_CALCULATE_LPF_DIRECT | X3DAUDIO_CALCULATE_REVERB*/,
			&dspSettings);

		checkResult(voice->SetOutputMatrix(context.masterVoice, sound->wfx.Format.nChannels, context.masterVoiceDetails.InputChannels, matrix));


		update3DTimer = UPDATE_3D_PERIOD;
	}

	--update3DTimer;
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
				voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);

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
		uint8 synthBuffer[MAX_SYNTH_SIZE];
		audio_synth* synth = sound->createSynth(synthBuffer);

		while (true)
		{
			uint32 size = STREAMING_BUFFER_SIZE;
			size = synth->getSamples(buffers[currentBufferIndex], size);

			if (size == 0)
			{
				break;
			}

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
				voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);

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


