#pragma once

#include "sound.h"
#include "core/math.h"

#include <x3daudio.h>

enum channel_state
{
	channel_state_to_play,
	channel_state_playing,

	channel_state_stopping,
	channel_state_stopped,

	channel_state_virtualizing,
	channel_state_virtual,
};

struct property_fader
{
	void initialize(float start)
	{
		current = start;
		totalTime = 0.f;
	}

	void startFade(float to, float time)
	{
		this->from = current;
		this->to = to;
		this->totalTime = time;
		this->timer = 0.f;

		if (time <= 0.f)
		{
			this->current = to;
		}
	}

	void update(float dt)
	{
		if (totalTime != 0.f)
		{
			timer += dt;
			current = lerp(from, to, clamp01(timer / totalTime));
		}
	}

	float from, to;
	float totalTime;
	float timer;

	float current;
};

struct audio_context
{
	com<IXAudio2> xaudio;
	
	IXAudio2MasteringVoice* masterVoice;
	XAUDIO2_VOICE_DETAILS masterVoiceDetails;

	IXAudio2SubmixVoice* soundTypeSubmixVoices[sound_type_count];
	XAUDIO2_VOICE_DETAILS soundTypeSubmixVoiceDetails[sound_type_count];
	IXAudio2SubmixVoice* reverbSubmixVoices[sound_type_count];

	X3DAUDIO_HANDLE xaudio3D;
	X3DAUDIO_LISTENER listener;
};

struct audio_channel
{
	audio_channel(const audio_context& context, const ref<audio_sound>& sound, const sound_settings& settings);
	audio_channel(const audio_context& context, const ref<audio_sound>& sound, vec3 position, const sound_settings& settings);
	~audio_channel();

	void update(const audio_context& context, float dt);
	void stop(float fadeOutTime);

	sound_settings* getSettings() { return &userSettings; }

	bool hasStopped();

	ref<audio_sound> sound;
	bool positioned;
	vec3 position;

private:
	void initialize(const audio_context& context, const ref<audio_sound>& sound, const sound_settings& settings, bool positioned, vec3 position = vec3(0.f));

	void updateSoundSettings(const audio_context& context, float dt);

	bool shouldBeVirtual();

	uint32 update3DTimer = 0;

	volatile channel_state state = channel_state_to_play;

	property_fader upDownFader;
	property_fader volumeFader;
	property_fader pitchFader;

	IXAudio2SourceVoice* voice;

	uint32 srcChannels;
	
	sound_settings userSettings;
	sound_settings oldUserSettings;

	float oldVolume = -1.f;
	float oldPitch = -1.f;


	struct voice_callback : IXAudio2VoiceCallback
	{
		audio_channel* channel;

		virtual void __stdcall OnVoiceProcessingPassStart(uint32 bytesRequired) override {}
		virtual void __stdcall OnVoiceProcessingPassEnd() override {}
		virtual void __stdcall OnStreamEnd() override { /*std::cout << "Stream end\n";*/ channel->stop(0.f); }
		virtual void __stdcall OnVoiceError(void* bufferContext, HRESULT error) override { std::cerr << "Error!\n"; }
		virtual void __stdcall OnBufferStart(void* bufferContext) override {}
		virtual void __stdcall OnBufferEnd(void* bufferContext) override { /*std::cout << "Buffer end\n";*/ if (bufferContext) { ReleaseSemaphore((HANDLE)bufferContext, 1, 0); } }
		virtual void __stdcall OnLoopEnd(void* bufferContext) override {}
	};

	voice_callback voiceCallback;

	HANDLE threadHandle = INVALID_HANDLE_VALUE;
	friend DWORD WINAPI streamFileAudio(void* parameter);
	friend DWORD WINAPI streamSynthAudio(void* parameter);

	HANDLE bufferEndSemaphore;

	bool threadStopped = true;
};
