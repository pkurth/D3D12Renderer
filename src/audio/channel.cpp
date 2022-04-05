#include "pch.h"
#include "channel.h"



#define MAX_BUFFER_COUNT 3
#define STREAMING_BUFFER_SIZE (1024 * 8 * 6)

#define UPDATE_3D_PERIOD 3 // > 0.



audio_channel::audio_channel(const audio_context& context, const ref<audio_sound>& sound, const sound_settings& settings)
{
	initialize(context, sound, settings, false);
}

audio_channel::audio_channel(const audio_context& context, const ref<audio_sound>& sound, vec3 position, const sound_settings& settings)
{
	initialize(context, sound, settings, true, position);
}

void audio_channel::initialize(const audio_context& context, const ref<audio_sound>& sound, const sound_settings& settings, bool positioned, vec3 position)
{
	this->sound = sound;
	this->voiceCallback.channel = this;

	userSettings = settings;

	oldUserSettings.volume = -1.f;
	oldUserSettings.pitch = -1.f;
	oldUserSettings.loop = false;

	this->positioned = positioned;
	this->position = position;

	XAUDIO2_SEND_DESCRIPTOR sendDescriptors[2];
	sendDescriptors[0].Flags = XAUDIO2_SEND_USEFILTER; // Direct.
	sendDescriptors[0].pOutputVoice = context.masterVoice;
	sendDescriptors[1].Flags = XAUDIO2_SEND_USEFILTER; // Reverb.
	sendDescriptors[1].pOutputVoice = context.reverbSubmixVoice;

	// Reverb only for positioned voices.
	const XAUDIO2_VOICE_SENDS sendList = { positioned ? 2u : 1u, sendDescriptors };

	checkResult(context.xaudio->CreateSourceVoice(&voice, (WAVEFORMATEX*)&sound->wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &voiceCallback, &sendList));
	checkResult(voice->Start());

	XAUDIO2_VOICE_DETAILS voiceDetails;
	voice->GetVoiceDetails(&voiceDetails);
	srcChannels = voiceDetails.InputChannels;


	volumeFader.initialize(settings.volume);
	pitchFader.initialize(settings.pitch);

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
	userSettings.pitch = clamp(userSettings.pitch, 0.f, XAUDIO2_DEFAULT_FREQ_RATIO);

	if (userSettings.volume != oldUserSettings.volume)
	{
		volumeFader.startFade(userSettings.volume, userSettings.volumeFadeTime);
	}
	if (userSettings.pitch != oldUserSettings.pitch)
	{
		pitchFader.startFade(userSettings.pitch, userSettings.pitchFadeTime);
	}

	oldUserSettings = userSettings;

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

		float channelAzimuths[8] = {};

		X3DAUDIO_EMITTER emitter = { 0 };
		emitter.ChannelCount = srcChannels;
		emitter.ChannelRadius = 0.1f;
		emitter.InnerRadius = 2.f;
		emitter.InnerRadiusAngle = X3DAUDIO_PI / 4.f;
		emitter.pChannelAzimuths = channelAzimuths;



		// A physically correct model would have CurveDistanceScaler=1 and pVolumeCurve=nullptr.
		// However the default reverb curve is linear, which sounds very weird, because it just stops way before the sound stops being audible.
		// Thus we just set both curves to linear and use CurveDistanceScaler to scale the curve to the desired radius.
		
#if 1
		emitter.CurveDistanceScaler = userSettings.radius;
		emitter.pVolumeCurve = (X3DAUDIO_DISTANCE_CURVE*)&X3DAudioDefault_LinearCurve;
		emitter.pReverbCurve = (X3DAUDIO_DISTANCE_CURVE*)&X3DAudioDefault_LinearCurve;
#else
		emitter.CurveDistanceScaler = 1;
#endif

		emitter.OrientFront = { forward.x, forward.y, -forward.z };
		emitter.OrientTop = { up.x, up.y, -up.z };
		emitter.Position = { position.x, position.y, -position.z };

		float matrix[8] = {};


		X3DAUDIO_DSP_SETTINGS dspSettings = { 0 };
		dspSettings.SrcChannelCount = srcChannels;
		dspSettings.DstChannelCount = context.masterVoiceDetails.InputChannels;
		dspSettings.pMatrixCoefficients = matrix;

		UINT32 flags = 0;
		flags |= X3DAUDIO_CALCULATE_MATRIX;
		flags |= X3DAUDIO_CALCULATE_LPF_DIRECT;
		flags |= X3DAUDIO_CALCULATE_REVERB;
		flags |= X3DAUDIO_CALCULATE_LPF_REVERB;
		//flags |= X3DAUDIO_CALCULATE_DOPPLER;

		X3DAudioCalculate(context.xaudio3D, &context.listener, &emitter, flags, &dspSettings);

		checkResult(voice->SetOutputMatrix(context.masterVoice, srcChannels, context.masterVoiceDetails.InputChannels, matrix));


		for (uint32 i = 0; i < srcChannels; ++i)
		{
			matrix[i] = dspSettings.ReverbLevel;
		}

		checkResult(voice->SetOutputMatrix(context.reverbSubmixVoice, srcChannels, 1, matrix));

		XAUDIO2_FILTER_PARAMETERS filterParametersDirect = { LowPassFilter, 2.f * sin(X3DAUDIO_PI / 6.f * dspSettings.LPFDirectCoefficient), 1.f };
		checkResult(voice->SetOutputFilterParameters(context.masterVoice, &filterParametersDirect));

		XAUDIO2_FILTER_PARAMETERS filterParametersReverb = { LowPassFilter, 2.f * sin(X3DAUDIO_PI / 6.f * dspSettings.LPFReverbCoefficient), 1.f };
		checkResult(voice->SetOutputFilterParameters(context.reverbSubmixVoice, &filterParametersReverb));

		//std::cout << dspSettings.ReverbLevel << ", " << dspSettings.LPFDirectCoefficient << ", " << dspSettings.LPFReverbCoefficient << '\n';

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

		if (!channel->userSettings.loop)
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

		if (!channel->userSettings.loop)
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


