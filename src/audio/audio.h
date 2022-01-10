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


struct audio_source
{
	virtual uint32 createSamples(float* buffer, uint32 numSamples) = 0;
};

struct sine_wave_audio_source : audio_source
{
	sine_wave_audio_source(float hz = C_HZ) : hz(hz) {}

	virtual uint32 createSamples(float* buffer, uint32 numSamples) override;

private:
	float hz;
	uint32 offset = 0;
};

struct audio_voice
{
	static const uint32 sampleHz = 44100;
	static const uint32 numChannels = 1;
	static const uint32 bytesPerChannel = sizeof(float);
	static const uint32 bufferedMilliseconds = 20;

	static_assert(sampleHz * numChannels * bufferedMilliseconds % 1000 == 0, "");

	static const uint32 numSamplesPerBuffer = sampleHz * numChannels * bufferedMilliseconds / 1000;
	static const uint32 numBytesPerBuffer = numSamplesPerBuffer * bytesPerChannel;

	float memory[2][numSamplesPerBuffer];

	audio_source* source;
	struct IXAudio2SourceVoice* voice;
	uint32 nextToWrite = 0;

	float volume = 1.f;
	float pitch = 1.f;
};

struct audio
{
	static bool initialize();
	static void shutdown();

	static void notifyOnSettingsChange();

	static bool playSound(audio_source* source, float volume = 1.f, float pitch = 1.f);


	static inline float masterVolume = 0.1f;

private:
	
	static com<struct IXAudio2> xaudio;
	static inline struct IXAudio2MasteringVoice* masterVoice;
	static inline std::vector<audio_voice*> sourceVoices;

	static inline uint32 numOutputChannels;
};
