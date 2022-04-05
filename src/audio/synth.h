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

struct audio_synth
{
	virtual uint32 getSamples(float* buffer, uint32 numSamples) = 0;
	virtual float getDuration() const { return 0.f; } // You only need to override this, if you don't stream the audio.
};

struct sine_synth : audio_synth
{
	static const uint32 numChannels = 1;
	static const uint32 sampleHz = 44100;

	sine_synth(float duration = 10.f, float hz = C_HZ)
		: audio_synth()
	{
		this->hz = hz;

		// Round duration such that there is an integer amount of waves over the runtime. This prevents clicking for looping sounds.
		float waves = duration * hz;
		duration = floor(waves) / hz;

		this->duration = duration;
		this->totalNumSamples = (uint32)(duration * numChannels * sampleHz);
	}

	virtual uint32 getSamples(float* buffer, uint32 numSamples) override
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

	virtual float getDuration() const override { return duration; }

private:
	uint32 totalNumSamples;
	float hz;
	float duration;

	uint32 offset = 0;
};

