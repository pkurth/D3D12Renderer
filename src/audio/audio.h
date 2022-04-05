#pragma once

#include "sound.h"
#include "core/math.h"

#include <xaudio2.h>


extern float masterAudioVolume;


bool initializeAudio();
void shutdownAudio();

void setAudioListener(vec3 position, quat rotation, vec3 velocity = vec3(0.f));

void updateAudio(float dt);


struct sound_handle
{
	uint32 id;
	operator bool() { return id != 0; }
};


sound_handle play2DSound(uint32 id, const sound_settings& settings);
sound_handle play3DSound(uint32 id, vec3 position, const sound_settings& settings);


bool soundStillPlaying(sound_handle handle);
bool stop(sound_handle handle, float fadeOutTime = 0.1f);

// Only hold on to this pointer for one frame! Retrieve each frame! Returns null, if sound has stopped already.
sound_settings* getSettings(sound_handle handle);


float dbToVolume(float db);
float volumeToDB(float volume);


