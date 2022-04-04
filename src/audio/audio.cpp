#include "pch.h"
#include "audio.h"
#include "sound.h"
#include "channel.h"

#include "core/log.h"

#include <unordered_map>


float masterAudioVolume = 0.1f;
static float oldMasterVolume = masterAudioVolume;

static property_fader masterVolumeFader;


static IXAudio2MasteringVoice* masterVoice;
static com<IXAudio2> xaudio;


typedef std::unordered_map<uint32, ref<audio_channel>> channel_map;
static channel_map channels;
static uint32 nextChannelID = 1;

bool initializeAudio()
{
	uint32 flags = 0;
#ifdef _DEBUG
	flags |= XAUDIO2_DEBUG_ENGINE;
#endif
	checkResult(XAudio2Create(xaudio.GetAddressOf(), flags));
	checkResult(xaudio->CreateMasteringVoice(&masterVoice));

	masterVolumeFader.initialize(masterAudioVolume);

	masterVoice->SetVolume(masterAudioVolume);

	return true;
}

void shutdownAudio()
{
	channels.clear();

	if (xaudio)
	{
		xaudio->StopEngine();
		xaudio.Reset();
	}
}

void updateAudio(float dt)
{
	masterAudioVolume = max(0.f, masterAudioVolume);
	if (oldMasterVolume != masterAudioVolume)
	{
		masterVolumeFader.startFade(masterAudioVolume, 0.1f);
		oldMasterVolume = masterAudioVolume;
	}

	masterVolumeFader.update(dt);
	masterVoice->SetVolume(masterVolumeFader.current);

	channel_map::iterator stoppedChannels[64];
	uint32 numStoppedChannels = 0;

	for (auto it = channels.begin(), end = channels.end(); it != end; ++it)
	{
		it->second->update(dt);
		if (it->second->hasStopped())
		{
			stoppedChannels[numStoppedChannels++] = it;
			LOG_MESSAGE("Deleting channel");
		}
	}

	for (uint32 i = 0; i < numStoppedChannels; ++i)
	{
		channels.erase(stoppedChannels[i]);
	}
}


sound_handle play2DSound(uint32 id, float volume, bool loop)
{
	ref<audio_sound> sound = getSound(id);
	if (!sound)
	{
		return {};
	}

	uint32 channelID = nextChannelID++;
	
	ref<audio_channel> channel = make_ref<audio_channel>(xaudio, sound, volume, loop);
	channels.insert({ channelID, channel });

	return { channelID };
}

bool setVolume(sound_handle handle, float volume)
{
	if (handle)
	{
		auto it = channels.find(handle.id);
		if (it != channels.end())
		{
			it->second->setVolume(volume);
			return true;
		}
	}
	return false;
}

bool stop(sound_handle handle, float fadeOutTime)
{
	if (handle)
	{
		auto it = channels.find(handle.id);
		if (it != channels.end())
		{
			it->second->stop(fadeOutTime);
			return true;
		}
	}
	return false;
}





