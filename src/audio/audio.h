#pragma once

#include "audio_generator.h"
#include "audio_clip.h"
#include "scene/scene.h"


struct audio_handle
{
	uint32 slotIndex = (uint32)-1;
	uint32 generation;

	bool valid();

	void pause();
	void resume();
	void stop();
	void changeVolume(float volume);
};

extern float masterAudioVolume;



bool initializeAudio();
void shutdownAudio();

audio_handle play2DAudio(const ref<audio_clip>& clip, float volume, float pitch);
audio_handle play3DAudio(const ref<audio_clip>& clip, float volume, float pitch, vec3 position);

void setAudioListener(vec3 position, quat rotation, vec3 velocity);

void updateAudio(game_scene& scene);




