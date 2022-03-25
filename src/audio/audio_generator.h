#pragma once

#include "core/math.h"


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


struct audio_generator
{
	audio_generator(uint32 totalNumSamples, uint32 numChannels = 1, uint32 sampleHz = 44100)
		: totalNumSamples(totalNumSamples), numChannels(numChannels), sampleHz(sampleHz) {}
	audio_generator(float duration, uint32 numChannels = 1, uint32 sampleHz = 44100)
		: totalNumSamples((uint32)(duration* numChannels* sampleHz)), numChannels(numChannels), sampleHz(sampleHz) {}

	virtual uint32 getNextSamples(float* buffer, uint32 offset, uint32 numSamples) const = 0;

	uint32 totalNumSamples;
	uint32 numChannels;
	uint32 sampleHz;
};

struct sine_wave_audio_generator : audio_generator
{
	sine_wave_audio_generator(float duration, float hz = C_HZ)
		: hz(hz), audio_generator(duration) {}

	virtual uint32 getNextSamples(float* buffer, uint32 offset, uint32 numSamples) const override
	{
		if (offset + numSamples > totalNumSamples)
		{
			numSamples = totalNumSamples - offset;
		}

		float factor = M_TAU / sampleHz * hz;
		for (uint32 i = 0; i < numSamples; ++i, ++offset)
		{
			buffer[i] = sin(offset * factor);
		}
		return numSamples;
	}

private:
	float hz;
};
