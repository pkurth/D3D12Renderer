#pragma once

#include "sound.h"
#include "core/math.h"

enum channel_state
{
	channel_state_to_play,
	channel_state_playing,

	channel_state_pausing,
	channel_state_paused,
	channel_state_resuming,

	channel_state_stopping,
	channel_state_stopped,
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

struct audio_channel
{
	audio_channel(const com<IXAudio2>& xaudio, const ref<audio_sound>& sound, float volume, bool loop);
	~audio_channel();

	void setVolume(float volume, float fadeTime = 0.1f);
	void update(float dt);
	void stop(float fadeOutTime);

	bool canBeKilled();

private:
	void updateVolume(float dt);

	bool loop;

	volatile channel_state state = channel_state_to_play;

	bool stopRequested = false;
	bool pauseRequested = false;
	property_fader stopFader;
	property_fader volumeFader;

	IXAudio2SourceVoice* voice;
	
	float oldVolume = -1.f;

	ref<audio_sound> sound;


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
