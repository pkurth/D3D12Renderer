#pragma once


#define C_HZ		261.63f
#define C_SHARP_HZ	277.18f
#define D_HZ		293.66f
#define D_SHARP_HZ	311.13f
#define E_HZ		329.63f
#define F_HZ		349.23f
#define F_SHARP_HZ	369.99f
#define G_HZ		392.00f
#define G_SHARP_HZ	415.30f
#define A_HZ		440.00f
#define A_SHARP_HZ	466.16f
#define B_HZ		493.88f


#define AUDIO_DURATION_INFINITE -1.f

struct audio_source
{
	uint32 numChannels;
	uint32 sampleHz;
	uint32 bufferedMilliseconds;
	bool streaming;

	static const uint32 bytesPerChannel = sizeof(float);

	audio_source(uint32 numChannels = 1, uint32 sampleHz = 44100, uint32 bufferedMilliseconds = 20, bool streaming = false)
		: numChannels(numChannels), sampleHz(sampleHz), bufferedMilliseconds(bufferedMilliseconds), streaming(streaming) {}

	virtual uint32 createSamples(float* buffer, uint32 numSamples) = 0;
};

struct sine_wave_audio_source : audio_source
{
	sine_wave_audio_source(float hz = C_HZ, float duration = AUDIO_DURATION_INFINITE)
		: audio_source(1, 44100, 20, true), hz(hz), duration(duration) {}

	virtual uint32 createSamples(float* buffer, uint32 numSamples) override;

private:
	float hz;
	float duration;
	uint32 offset = 0;
};

struct audio_voice
{
	uint32 numSamplesPerBuffer;
	uint32 numBytesPerBuffer;

	float* allMemory;
	float* buffers[2];

	audio_source* source;
	struct IXAudio2SourceVoice* voice;
	uint32 nextToWrite;
	uint32 stopOn;

	float volume;
	float pitch;

	bool paused;

	uint32 generation = 0;
};

struct audio_handle
{
	uint32 generation;
	uint32 index;
};

struct audio
{
	static bool initialize();
	static void shutdown();

	static void notifyOnSettingsChange();

	static audio_handle playSound(audio_source* source, float volume = 1.f, float pitch = 1.f);

	static void pause(audio_handle handle);
	static void resume(audio_handle handle);
	static void stop(audio_handle handle);

	static inline float masterVolume = 0.1f;

private:

	static void retireVoice(audio_voice& voice);
	
	static com<struct IXAudio2> xaudio;
	static inline struct IXAudio2MasteringVoice* masterVoice;
	
	static inline audio_voice sourceVoices[64];
	static inline uint32 freeVoices[64];
	static inline uint32 numFreeVoices = 64;

	static inline uint32 numOutputChannels;

	friend struct voice_callback;
};
