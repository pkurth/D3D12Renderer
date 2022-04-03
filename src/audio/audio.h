#pragma once

#include "sound.h"
#include "core/math.h"
#include <xaudio2.h>


extern float masterAudioVolume;


bool initializeAudio();
void shutdownAudio();

inline void setAudioListener(vec3 position, quat rotation, vec3 velocity) {}

void updateAudio(float dt);


void unloadSound(uint32 id);


struct sound_handle
{
	uint32 id;
	operator bool() { return id != 0; }
};

sound_handle play2DSound(uint32 id, float volume = 1.f, bool loop = false);



bool setVolume(sound_handle handle, float volume);
bool stop(sound_handle handle, float fadeOutTime = 0.1f);


