#include "pch.h"
#include "audio.h"
#include "sound.h"
#include "channel.h"

#include "core/log.h"

#include <unordered_map>
#include <x3daudio.h>


float masterAudioVolume = 0.1f;
static float oldMasterVolume = masterAudioVolume;


static property_fader masterVolumeFader;

static audio_context context;

typedef std::unordered_map<uint32, ref<audio_channel>> channel_map;
static channel_map channels;
static uint32 nextChannelID = 1;

bool initializeAudio()
{
	uint32 flags = 0;
#ifdef _DEBUG
	flags |= XAUDIO2_DEBUG_ENGINE;
#endif
	checkResult(XAudio2Create(context.xaudio.GetAddressOf(), flags));
	checkResult(context.xaudio->CreateMasteringVoice(&context.masterVoice));

	masterVolumeFader.initialize(masterAudioVolume);

	context.masterVoice->SetVolume(masterAudioVolume);


	// 3D.

	DWORD channelMask;
	context.masterVoice->GetChannelMask(&channelMask);
	context.masterVoice->GetVoiceDetails(&context.masterVoiceDetails);

	X3DAudioInitialize(channelMask, X3DAUDIO_SPEED_OF_SOUND, context.xaudio3D);

	context.listener.OrientFront = { 0.f, 0.f, 1.f };
	context.listener.OrientTop = { 0.f, 1.f, 0.f };

	return true;
}

void shutdownAudio()
{
	channels.clear();

	if (context.xaudio)
	{
		context.xaudio->StopEngine();
		context.xaudio.Reset();
	}
}

void setAudioListener(vec3 position, quat rotation, vec3 velocity)
{
	vec3 forward = rotation * vec3(0.f, 0.f, -1.f);
	vec3 up = rotation * vec3(0.f, 1.f, 0.f);

	// XAudio2 uses a lefthanded coordinate system, which is why we need to flip the z-axis.
	context.listener.OrientFront = { forward.x, forward.y, -forward.z };
	context.listener.OrientTop = { up.x, up.y, -up.z };
	context.listener.Position = { position.x, position.y, -position.z };
	context.listener.Velocity = { velocity.x, velocity.y, -velocity.z };
	context.listener.pCone = 0;
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
	context.masterVoice->SetVolume(masterVolumeFader.current);

	channel_map::iterator stoppedChannels[64];
	uint32 numStoppedChannels = 0;

	for (auto it = channels.begin(), end = channels.end(); it != end; ++it)
	{
		it->second->update(context, dt);
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
	
	ref<audio_channel> channel = make_ref<audio_channel>(context, sound, volume, loop);
	channels.insert({ channelID, channel });

	return { channelID };
}

sound_handle play3DSound(uint32 id, vec3 position, float volume, bool loop)
{
	ref<audio_sound> sound = getSound(id);
	if (!sound)
	{
		return {};
	}

	uint32 channelID = nextChannelID++;

	ref<audio_channel> channel = make_ref<audio_channel>(context, sound, position, volume, loop);
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





