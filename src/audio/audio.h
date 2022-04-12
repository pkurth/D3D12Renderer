#pragma once

#include "sound.h"
#include "sound_management.h"
#include "reverb.h"
#include "core/math.h"

#include <xaudio2.h>



struct master_audio_settings
{
	float volume = 0.1f;
	reverb_preset reverbPreset = reverb_preset_default;
};


extern master_audio_settings masterAudioSettings;
extern float soundTypeVolumes[sound_type_count];


bool initializeAudio();
void shutdownAudio();

void setAudioListener(vec3 position, quat rotation, vec3 velocity = vec3(0.f));

void updateAudio(float dt);


struct sound_handle
{
	uint32 id;
	operator bool() { return id != 0; }
};


sound_handle play2DSound(const sound_id& id, const sound_settings& settings);
sound_handle play3DSound(const sound_id& id, vec3 position, const sound_settings& settings);


bool soundStillPlaying(sound_handle handle);
bool stop(sound_handle handle, float fadeOutTime = 0.1f);

// Only hold on to this pointer for one frame! Retrieve each frame! Returns null, if sound has stopped already.
sound_settings* getSettings(sound_handle handle);


float dbToVolume(float db);
float volumeToDB(float volume);


