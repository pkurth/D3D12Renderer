#include "pch.h"

#define XAUDIO2_HELPER_FUNCTIONS
#include <xaudio2.h>
#include <xaudio2fx.h>


#undef M_PI

#include "audio.h"
#include "sound.h"
#include "channel.h"

#include "core/log.h"
#include "core/cpu_profiling.h"

#include <unordered_map>
#include <x3daudio.h>


master_audio_settings masterAudioSettings;
master_audio_settings oldMasterAudioSettings;

static property_fader masterVolumeFader;


float soundTypeVolumes[sound_type_count];
static float oldSoundTypeVolumes[sound_type_count];
static property_fader soundTypeVolumeFaders[sound_type_count];

static audio_context context;

typedef std::unordered_map<uint32, ref<audio_channel>> channel_map;
static channel_map channels;
static uint32 nextChannelID = 1;





static XAUDIO2FX_REVERB_I3DL2_PARAMETERS reverbPresets[] =
{
	XAUDIO2FX_I3DL2_PRESET_DEFAULT,
	XAUDIO2FX_I3DL2_PRESET_GENERIC,
	XAUDIO2FX_I3DL2_PRESET_PADDEDCELL,
	XAUDIO2FX_I3DL2_PRESET_ROOM,
	XAUDIO2FX_I3DL2_PRESET_BATHROOM,
	XAUDIO2FX_I3DL2_PRESET_LIVINGROOM,
	XAUDIO2FX_I3DL2_PRESET_STONEROOM,
	XAUDIO2FX_I3DL2_PRESET_AUDITORIUM,
	XAUDIO2FX_I3DL2_PRESET_CONCERTHALL,
	XAUDIO2FX_I3DL2_PRESET_CAVE,
	XAUDIO2FX_I3DL2_PRESET_ARENA,
	XAUDIO2FX_I3DL2_PRESET_HANGAR,
	XAUDIO2FX_I3DL2_PRESET_CARPETEDHALLWAY,
	XAUDIO2FX_I3DL2_PRESET_HALLWAY,
	XAUDIO2FX_I3DL2_PRESET_STONECORRIDOR,
	XAUDIO2FX_I3DL2_PRESET_ALLEY,
	XAUDIO2FX_I3DL2_PRESET_FOREST,
	XAUDIO2FX_I3DL2_PRESET_CITY,
	XAUDIO2FX_I3DL2_PRESET_MOUNTAINS,
	XAUDIO2FX_I3DL2_PRESET_QUARRY,
	XAUDIO2FX_I3DL2_PRESET_PLAIN,
	XAUDIO2FX_I3DL2_PRESET_PARKINGLOT,
	XAUDIO2FX_I3DL2_PRESET_SEWERPIPE,
	XAUDIO2FX_I3DL2_PRESET_UNDERWATER,
	XAUDIO2FX_I3DL2_PRESET_SMALLROOM,
	XAUDIO2FX_I3DL2_PRESET_MEDIUMROOM,
	XAUDIO2FX_I3DL2_PRESET_LARGEROOM,
	XAUDIO2FX_I3DL2_PRESET_MEDIUMHALL,
	XAUDIO2FX_I3DL2_PRESET_LARGEHALL,
	XAUDIO2FX_I3DL2_PRESET_PLATE,
};



/*
	Our audio graph looks like this:

	[Source(s)] ----> [Sound type submix] ----> [Master]
	     `-> [Sound type reverb] -^
*/



static void setReverb()
{
	bool reverbOn = masterAudioSettings.reverbEnabled && masterAudioSettings.reverbPreset != reverb_none;

	if (reverbOn)
	{
		XAUDIO2FX_REVERB_PARAMETERS reverbParameters;
		ReverbConvertI3DL2ToNative(&reverbPresets[masterAudioSettings.reverbPreset], &reverbParameters);

		for (uint32 i = 0; i < sound_type_count; ++i)
		{
			context.reverbSubmixVoices[i]->SetEffectParameters(0, &reverbParameters, sizeof(reverbParameters));
		}
	}

	for (uint32 i = 0; i < sound_type_count; ++i)
	{
		float level[16];
		float reverbLevel = reverbOn ? 1.f : 0.f;
		for (uint32 j = 0; j < context.soundTypeSubmixVoiceDetails[i].InputChannels; ++j)
		{
			level[j] = reverbLevel;
		}

		context.reverbSubmixVoices[i]->SetOutputMatrix(context.soundTypeSubmixVoices[i], 1, context.soundTypeSubmixVoiceDetails[i].InputChannels, level);
	}
}

bool initializeAudio()
{
	uint32 flags = 0;
#ifdef _DEBUG
	flags |= XAUDIO2_DEBUG_ENGINE;
#endif
	checkResult(XAudio2Create(context.xaudio.GetAddressOf(), flags));
	checkResult(context.xaudio->CreateMasteringVoice(&context.masterVoice));

	masterVolumeFader.initialize(masterAudioSettings.volume);
	context.masterVoice->SetVolume(masterAudioSettings.volume);

	DWORD channelMask;
	context.masterVoice->GetChannelMask(&channelMask);
	context.masterVoice->GetVoiceDetails(&context.masterVoiceDetails);




	X3DAudioInitialize(channelMask, X3DAUDIO_SPEED_OF_SOUND, context.xaudio3D);




	for (uint32 i = 0; i < sound_type_count; ++i)
	{
		soundTypeVolumes[i] = oldSoundTypeVolumes[i] = 1.f;
		soundTypeVolumeFaders[i].initialize(soundTypeVolumes[i]);

		checkResult(context.xaudio->CreateSubmixVoice(&context.soundTypeSubmixVoices[i], context.masterVoiceDetails.InputChannels, context.masterVoiceDetails.InputSampleRate, 0, 1));
		context.soundTypeSubmixVoices[i]->GetVoiceDetails(&context.soundTypeSubmixVoiceDetails[i]);
		context.soundTypeSubmixVoices[i]->SetVolume(soundTypeVolumes[i]);


		IUnknown* reverbXAPO;
		checkResult(XAudio2CreateReverb(&reverbXAPO));


		XAUDIO2_EFFECT_DESCRIPTOR descriptor;
		descriptor.InitialState = true;
		descriptor.OutputChannels = 1;
		descriptor.pEffect = reverbXAPO;

		XAUDIO2_EFFECT_CHAIN chain;
		chain.EffectCount = 1;
		chain.pEffectDescriptors = &descriptor;


		XAUDIO2_SEND_DESCRIPTOR sendDescriptor;
		sendDescriptor.Flags = 0;
		sendDescriptor.pOutputVoice = context.soundTypeSubmixVoices[i];

		const XAUDIO2_VOICE_SENDS sendList = { 1, &sendDescriptor };

		checkResult(context.xaudio->CreateSubmixVoice(&context.reverbSubmixVoices[i], 1, context.soundTypeSubmixVoiceDetails[i].InputSampleRate, 0, 0, &sendList, &chain));

		reverbXAPO->Release();
	}


	setReverb();



	context.listener.OrientFront = { 0.f, 0.f, 1.f };
	context.listener.OrientTop = { 0.f, 1.f, 0.f };
	context.listener.pCone = (X3DAUDIO_CONE*)&X3DAudioDefault_DirectionalCone;

	loadSoundRegistry();

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
}

void updateAudio(float dt)
{
	CPU_PROFILE_BLOCK("Update audio");

	masterAudioSettings.volume = max(0.f, masterAudioSettings.volume);

	if (oldMasterAudioSettings.volume != masterAudioSettings.volume)
	{
		masterVolumeFader.startFade(masterAudioSettings.volume, 0.1f);
	}

	if (oldMasterAudioSettings.reverbEnabled != masterAudioSettings.reverbEnabled || oldMasterAudioSettings.reverbPreset != masterAudioSettings.reverbPreset)
	{
		setReverb();
	}

	oldMasterAudioSettings = masterAudioSettings;

	masterVolumeFader.update(dt);
	context.masterVoice->SetVolume(masterVolumeFader.current);



	for (uint32 i = 0; i < sound_type_count; ++i)
	{
		soundTypeVolumes[i] = max(0.f, soundTypeVolumes[i]);

		if (oldSoundTypeVolumes[i] != soundTypeVolumes[i])
		{
			soundTypeVolumeFaders[i].startFade(soundTypeVolumes[i], 0.1f);
		}

		oldSoundTypeVolumes[i] = soundTypeVolumes[i];

		soundTypeVolumeFaders[i].update(dt);
		context.soundTypeSubmixVoices[i]->SetVolume(soundTypeVolumeFaders[i].current);
	}








	channel_map::iterator stoppedChannels[64];
	uint32 numStoppedChannels = 0;

	for (auto it = channels.begin(), end = channels.end(); it != end; ++it)
	{
		it->second->update(context, dt);
		if (it->second->hasStopped())
		{
			stoppedChannels[numStoppedChannels++] = it;
			//LOG_MESSAGE("Deleting channel");
		}
	}

	for (uint32 i = 0; i < numStoppedChannels; ++i)
	{
		channels.erase(stoppedChannels[i]);
	}
}


sound_handle play2DSound(const sound_id& id, const sound_settings& settings)
{
	if (settings.volume <= 0.f)
	{
		return {};
	}

	ref<audio_sound> sound = getSound(id);
	if (!sound)
	{
		// Not all sounds are file sounds, but we have no chance of creating a synth here.
		if (!loadFileSound(id))
		{
			return {};
		}

		sound = getSound(id);
	}

	uint32 channelID = nextChannelID++;
	
	ref<audio_channel> channel = make_ref<audio_channel>(context, sound, settings);
	channels.insert({ channelID, channel });

	return { channelID };
}

sound_handle play3DSound(const sound_id& id, vec3 position, const sound_settings& settings)
{
	if (settings.volume <= 0.f)
	{
		return {};
	}

	ref<audio_sound> sound = getSound(id);
	if (!sound)
	{
		if (!loadFileSound(id))
		{
			return {};
		}

		sound = getSound(id);
	}

	uint32 channelID = nextChannelID++;

	ref<audio_channel> channel = make_ref<audio_channel>(context, sound, position, settings);
	channels.insert({ channelID, channel });

	return { channelID };
}

bool soundStillPlaying(sound_handle handle)
{
	return handle && (channels.find(handle.id) != channels.end());
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

void restartAllSounds()
{
	auto channelsCopy = std::move(channels);

	for (auto it = channelsCopy.begin(), end = channelsCopy.end(); it != end; ++it)
	{
		sound_id id = it->second->sound->id;
		sound_settings settings = *it->second->getSettings();
		bool positioned = it->second->positioned;
		vec3 position = it->second->position;

		it->second->stop(0.f);

		if (positioned)
		{
			play3DSound(id, position, settings);
		}
		else
		{
			play2DSound(id, settings);
		}
	}
}

sound_settings* getSettings(sound_handle handle)
{
	if (handle)
	{
		auto it = channels.find(handle.id);
		if (it != channels.end())
		{
			return it->second->getSettings();
		}
	}
	return 0;
}

float dbToVolume(float db)
{
	return XAudio2DecibelsToAmplitudeRatio(db);
}

float volumeToDB(float volume)
{
	return XAudio2AmplitudeRatioToDecibels(volume);
}





