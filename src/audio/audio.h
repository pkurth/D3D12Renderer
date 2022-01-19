#pragma once

#include "audio_generator.h"


bool initializeAudio();
void shutdownAudio();

bool playAudioFromFile(const fs::path& path, float volume, float pitch, bool stream, bool loop = false);
bool playAudioFromData(float* data, uint32 totalNumSamples, uint32 numChannels, uint32 sampleHz, float volume, float pitch, bool loop = false, bool deleteBufferAfterPlayback = false);
bool playAudioFromGenerator(audio_generator* generator, float volume, float pitch, bool stream, bool loop = false);





