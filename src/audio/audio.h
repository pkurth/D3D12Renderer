#pragma once

#include "audio_generator.h"


bool initializeAudio();
void shutdownAudio();


struct audio_handle
{
	uint32 slotIndex = (uint32)-1;
	uint32 generation;

	bool valid();
};

extern float masterAudioVolume;

audio_handle playAudioFromFile(const fs::path& path, float volume, float pitch, bool stream, bool loop = false);
audio_handle playAudioFromData(float* data, uint32 totalNumSamples, uint32 numChannels, uint32 sampleHz, float volume, float pitch, bool loop = false, bool deleteBufferAfterPlayback = false);
audio_handle playAudioFromGenerator(audio_generator* generator, float volume, float pitch, bool stream, bool loop = false);


void updateAudio();




